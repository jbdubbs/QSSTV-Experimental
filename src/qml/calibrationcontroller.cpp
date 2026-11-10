#include "calibrationcontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "rxcontroller.h"
#include "configdialog.h"
#include "dispatcher.h"
#include "imageviewer.h"
#include "rxwidget.h"
#include "txwidget.h"
#include "soundconfig.h"
#include "calibrationsstv.h"
#include "sntpclient.h"
#include "rawclock.h"

#include <QDate>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QLocale>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSettings>
#include <cmath>
#include <numeric>
#include <algorithm>

// the limits are the ones of the desktop pages (calibrationntp.cpp, calibrationwwv.cpp, calibrationsstv.cpp)
#define DEFAULTSERVERS "time.nist.gov, time.cloudflare.com, 0.pool.ntp.org"
#define AUDIOWINDOW 10.0        // seconds; block arrival is only ever late, so the earliest block of a window is kept
#define MINNTPSAMPLES 5
#define MINAUDIOPOINTS 5
#define MINSECONDS 120.0        // shortest measurement that can be accepted
#define MAXACCEPTPPM 10.0       // largest standard error that can be accepted
#define MAXPPM 2000.0           // 0.2%: the same limit soundConfig applies to a stored clock
#define CLIPSIGMA 3.0
#define NTPMINCLIP 0.001
#define AUDIOMINCLIP 0.002
#define GOODTICKS 20            // ticks needed before the result can be accepted
#define LINEWIDTH 3             // pixels, of the line in the reference picture
#define MINROWS 40              // fewest rows that took part in the fit of an acceptable picture
#define MAXRMS 1.5              // pixels, largest deviation from the fitted line that is acceptable
#define BARSECONDS 300.0        // the bar of the internet time page is full after 5 minutes
#define BARTICKS 300.0

static const esstvMode sstvModeList[]={PD120W,PD120,PD160,MP175,MP140,SC2_120};   // 2 to 3 minute modes, the wider the better

CalibrationController *CalibrationController::instance()
{
  static CalibrationController *c=new CalibrationController;
  return c;
}

CalibrationController *CalibrationController::create(QQmlEngine *,QJSEngine *)
{
  CalibrationController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

CalibrationController::CalibrationController(QObject *parent) : QObject(parent)
{
  QSettings s;
  m_servers=s.value("Calibration/servers",QStringLiteral(DEFAULTSERVERS)).toString();
  m_applyTx=s.value("Calibration/applyTx",true).toBool();
  m_sstvMode=qBound(0,s.value("Calibration/sstvMode",1).toInt(),int(sizeof(sstvModeList)/sizeof(sstvModeList[0]))-1);
  m_sstvPosition=qBound(10,s.value("Calibration/sstvPosition",50).toInt(),90);
  connect(&m_timer,&QTimer::timeout,this,&CalibrationController::slotTimer);
  m_windowBest.x=m_windowBest.y=0;
  m_windowBest.group=0;
}

CalibrationController::~CalibrationController()
{
  delete m_detector;
  delete m_fit;
}

// ---- formats ----

// the program is in English: the numbers are written the English way, with a group separator, whatever the phone's own locale does
static QLocale numberLocale() {return QLocale(QLocale::English,QLocale::UnitedStates);}

QString CalibrationController::fmtHz(double hz)
{
  return numberLocale().toString(hz,'f',2)+QStringLiteral(" Hz");
}

QString CalibrationController::fmtPpm(double ppm)
{
  return QString(ppm>=0 ? "+" : "")+numberLocale().toString(ppm,'f',1)+QStringLiteral(" ppm");
}

static double ppmOf(double clock) {return (clock/BASESAMPLERATE-1.0)*1e6;}

// ---- what is in use now ----

QString CalibrationController::rxClockText() const {return fmtHz(::rxClock);}
QString CalibrationController::txClockText() const {return fmtHz(::txClock);}

bool CalibrationController::calibrated() const
{
  return std::fabs(ppmOf(::rxClock))>0.005 || std::fabs(ppmOf(::txClock))>0.005;
}

QString CalibrationController::rxNote() const
{
  const double p=ppmOf(::rxClock);
  if(std::fabs(p)<=0.005) return tr("not measured");
  return fmtPpm(p)+QStringLiteral(", ")+(p>=0 ? tr("fast") : tr("slow"));
}

QString CalibrationController::txNote() const
{
  const double p=ppmOf(::txClock);
  if(std::fabs(::txClock-::rxClock)<0.0005 && calibrated()) return tr("same as receive");
  if(std::fabs(p)<=0.005) return tr("not measured");
  return fmtPpm(p)+QStringLiteral(", ")+(p>=0 ? tr("fast") : tr("slow"));
}

QString CalibrationController::deviceText() const
{
  auto name=[](const QString &s,const QString &dflt) {return (s.isEmpty() || s==QLatin1String("default")) ? dflt : s;};
  const QString in=name(::inputAudioDevice,tr("phone microphone"));
  QString out=name(::outputAudioDevice,tr("speaker"));
  QString t;
  if(in==out) t=in;
  else
    {
      // "Phone microphone" and "Phone speaker" read better as "Phone microphone and speaker"
      const QString first=in.section(QLatin1Char(' '),0,0);
      if(!first.isEmpty() && in.contains(QLatin1Char(' ')) && out.startsWith(first+QLatin1Char(' '),Qt::CaseInsensitive)) out=out.mid(first.size()+1);
      t=tr("%1 and %2").arg(in,out);
    }
  if(t.size()>1 && t.at(0).isUpper() && t.at(1).isLower()) t[0]=t.at(0).toLower();   // it comes in the middle of a sentence
  return t;
}

QString CalibrationController::infoText() const
{
  if(!calibrated()) return tr("Sound device now: %1").arg(deviceText());
  QSettings s;
  const QString when=s.value("Calibration/when").toString();
  if(when.isEmpty()) return tr("Set earlier, not by this page. Sound device now: %1").arg(deviceText());
  const int how=s.value("Calibration/how").toInt();
  const QString howText=how==1 ? tr("internet time") : how==2 ? tr("a radio time signal") : tr("another SSTV station");
  const QString device=s.value("Calibration/device").toString();
  QString t=tr("Measured %1 with %2, for the %3").arg(numberLocale().toString(QDate::fromString(when,Qt::ISODate),QStringLiteral("d MMM yyyy")),howText,device);
  if(!device.isEmpty() && device!=deviceText()) t+=QStringLiteral(". ")+tr("A different sound device is in use now: measure again for it.");
  return t;
}

QString CalibrationController::summary() const
{
  if(!calibrated()) return tr("Sound clock: not measured yet");
  QString t=tr("Sound clock: %1").arg(fmtPpm(ppmOf(::rxClock)));
  if(std::fabs(::txClock-::rxClock)>0.0005) t+=tr(", send %1").arg(fmtPpm(ppmOf(::txClock)));
  return t;
}

// ---- the measurement as the page shows it ----

QString CalibrationController::rateText() const
{
  const double rx=resultRx();
  if(rx<=0) return QString();
  return fmtHz(rx);
}

QString CalibrationController::ppmText() const
{
  const double rx=resultRx();
  if(rx<=0) return QString();
  const double p=ppmOf(rx);
  return fmtPpm(p)+QStringLiteral(", ")+(p>=0 ? tr("the sound device runs fast") : tr("the sound device runs slow"));
}

QString CalibrationController::errorText() const
{
  double e=0;
  if(m_method==1 && m_valid) e=m_ppmError;
  else if(m_method==2 && m_fit && m_fit->result().valid) e=m_fit->result().ppmError;
  if(e<=0 || resultRx()<=0) return QString();
  return QString::fromUtf8("±")+numberLocale().toString(e,'f',1);
}

static QString clockTime(double seconds)
{
  const int s=int(seconds+0.5);
  return QStringLiteral("%1:%2").arg(s/60).arg(s%60,2,10,QLatin1Char('0'));
}

qreal CalibrationController::quality() const
{
  if(m_method==1) return qBound(0.0,m_elapsed/BARSECONDS,1.0);
  if(m_method==2) return m_fit ? qBound(0.0,m_fit->result().ticks/BARTICKS,1.0) : 0.0;
  return 0;
}

qreal CalibrationController::qualityMark() const
{
  if(m_method==1) return MINSECONDS/BARSECONDS;
  if(m_method==2) return GOODTICKS/BARTICKS;
  return 0;
}

QString CalibrationController::qualityLeft() const
{
  if(m_method==1) return clockTime(m_elapsed);
  if(m_method==2) return tr("%1 ticks").arg(m_fit ? m_fit->result().ticks : 0);
  return QString();
}

QString CalibrationController::qualityRight() const
{
  if(m_method==1) return canSave() ? tr("a few minutes more make it more exact") : tr("%1 minimum").arg(clockTime(MINSECONDS));
  if(m_method==2) return tr("%1 ticks minimum, 5 minutes is better").arg(GOODTICKS);
  return QString();
}

double CalibrationController::sstvMeanClock() const
{
  if(m_sstvClocks.empty()) return 0;
  return std::accumulate(m_sstvClocks.begin(),m_sstvClocks.end(),0.0)/m_sstvClocks.size();
}

double CalibrationController::resultRx() const
{
  switch(m_method)
    {
    case 1: return ntpHasResult() ? BASESAMPLERATE*(1.0+m_ppm*1e-6) : 0;
    case 2: return wwvHasResult() ? BASESAMPLERATE*(1.0+m_fit->result().ppm*1e-6) : 0;
    case 3: return m_sstvRole==0 ? sstvMeanClock() : 0;   // only the receiving side measures
    default: return 0;
    }
}

bool CalibrationController::canSave() const {return resultRx()>0;}

void CalibrationController::changed()
{
  emit measureChanged();
}

void CalibrationController::setStatus(const QString &s)
{
  m_status=s;
}

void CalibrationController::setProblem(const QString &text,int action)
{
  m_problem=text;
  m_problemAction=action;
}

void CalibrationController::setApplyTx(bool on)
{
  if(on==m_applyTx) return;
  m_applyTx=on;
  QSettings().setValue("Calibration/applyTx",on);
  changed();
}

void CalibrationController::setServers(const QString &s)
{
  if(s==m_servers) return;
  m_servers=s.trimmed().isEmpty() ? QStringLiteral(DEFAULTSERVERS) : s.trimmed();
  QSettings().setValue("Calibration/servers",m_servers);
  emit serversChanged();
}

QString CalibrationController::defaultServers() const {return QStringLiteral(DEFAULTSERVERS);}

void CalibrationController::setStation(int s)
{
  s=qBound(0,s,1);
  if(s==m_station) return;
  m_station=s;
  if(m_detector) m_detector->setTone(s==0 ? 1000.0 : 1200.0);
  if(m_method==2 && m_running)   // another tone: what was found so far does not count
    {
      if(m_detector) m_detector->reset();
      if(m_fit) m_fit->reset();
      m_hadResult=false;
      wwvDisplay();
      ++m_plotRevision;
      emit plotChanged();
    }
  changed();
}

// ---- the sound device ----

void CalibrationController::takeSound()
{
  if(!m_soundTaken)
    {
      m_rxWasListening=RxController::instance()->listening();
      m_soundTaken=true;
    }
  if(dispatcherPtr) dispatcherPtr->idleAll();
}

void CalibrationController::giveSoundBack()
{
  if(m_modeSwitched)
    {
      AppModel::instance()->setDigital(m_wasDigital);
      m_modeSwitched=false;
    }
  if(!m_soundTaken) return;
  m_soundTaken=false;
  if(m_rxWasListening) RxController::instance()->start();
}

bool CalibrationController::startSound()
{
  if(AppModel::instance()->micDenied())
    {
      setProblem(tr("The microphone is not allowed for this app."),1);
      changed();
      return false;
    }
  if(!soundIOPtr)
    {
      setProblem(tr("There is no sound device."));
      changed();
      return false;
    }
  takeSound();
  const double sr=soundIOPtr->streamSampleRate();
  m_streamRate=sr>0 ? sr : BASESAMPLERATE;
  if(!soundIOPtr->startListen())
    {
      setProblem(tr("The sound device cannot be used: %1").arg(soundIOPtr->getLastError()));
      changed();
      return false;
    }
  m_lastOverruns=soundIOPtr->listenOverruns();
  return true;
}

static void setRunningFlag(bool &flag,bool on)
{
  if(flag==on) return;
  flag=on;
  AppModel::instance()->keepScreenOn(on);   // a measurement takes minutes: the screen stays on
}

// ---- the pages ----

void CalibrationController::open(int method)
{
  if(m_running) stop();
  m_test=false;
  m_method=method;
  clearResult();
  setProblem(QString());
  if(method==3)
    setStatus(m_sstvRole==0 ? tr("Press Listen, then have the reference station send its picture.")
                           : tr("Sends a black picture with one straight white line. Your own picture is put back afterwards."));
  else setStatus(tr("Not started"));
  changed();
  emit plotChanged();
}

void CalibrationController::clearResult()
{
  // internet time
  m_stampBuf.clear();
  m_audioPts.clear();
  m_ntpPts.clear();
  m_haveWindow=false;
  m_lastNtpStatus.clear();
  m_audioFit=lineFitResult();
  m_ntpFit=lineFitResult();
  m_valid=false;
  m_ppm=m_ppmError=0;
  m_ntpTrace=calTrace();
  m_audioTrace=calTrace();
  m_elapsed=0;
  // radio time signal
  if(m_detector) m_detector->reset();
  if(m_fit) m_fit->reset();
  m_level=0;
  // another SSTV station
  m_sstvClocks.clear();
  m_sstvPpms.clear();
  m_sstvImage=QImage();
  m_sstvFitResult=slantFitResult();
  m_sstvLast.clear();
  m_hadResult=false;
  m_detail.clear();
  ++m_plotRevision;
}

void CalibrationController::start()
{
  if(m_test || m_running) return;
  setProblem(QString());
  switch(m_method)
    {
    case 1: startNtp(); break;
    case 2: startWwv(); break;
    case 3:
      if(m_sstvRole==0) startSstvListening();
      else sendReference();
      break;
    default: break;
    }
}

void CalibrationController::stopMeasuring()
{
  m_timer.stop();
  if(m_sntp) m_sntp->stop();
  if(m_method==1 || m_method==2)
    {
      if(m_running && soundIOPtr) soundIOPtr->stopListen();
      setRunningFlag(m_running,false);
    }
  if(m_method==3)
    {
      stopSstvListening();
      if(m_sstvSending && txWidgetPtr) txWidgetPtr->abortCalibrationTx();
    }
}

void CalibrationController::stop()
{
  if(m_test) return;
  stopMeasuring();
  if(m_method==1) ntpDisplay();
  else if(m_method==2) wwvDisplay();
  else if(m_method==3) sstvDisplay();
  changed();
}

void CalibrationController::leave()
{
  if(m_test)
    {
      m_test=false;
      m_method=0;
      setRunningFlag(m_running,false);
      clearResult();
      setProblem(QString());
      changed();
      emit plotChanged();
      return;
    }
  const bool had=m_method!=0 || m_running;
  stopMeasuring();
  m_method=0;
  setProblem(QString());
  giveSoundBack();
  if(had) {changed(); emit plotChanged();}
}

bool CalibrationController::save()
{
  const double rx=resultRx();
  if(rx<=0) return false;
  const int how=m_method;
  const double tx=m_applyTx ? rx : 0;
  if(m_method==1 || m_method==2) stopMeasuring();
  apply(rx,tx,QString::number(how));
  if(m_method==3)
    {
      // listening goes on: the next picture shows the line with the new clock; what was collected so far belongs to the old one
      m_sstvClocks.clear();
      m_sstvPpms.clear();
      setStatus(tr("Saved: receive clock %1. Listening for another picture to check the line.").arg(fmtHz(rx)));
      m_detail.clear();
    }
  emit saved(tr("Receive clock %1 saved").arg(fmtHz(rx)));
  emit AppModel::instance()->noticeShown(tr("Receive clock %1 saved").arg(fmtHz(rx)));
  changed();
  return true;
}

void CalibrationController::apply(double rx,double tx,const QString &how)
{
  // a method that did not measure a clock keeps the current value; the Configuration > Sound fields follow (they are read back at quit)
  const double r=rx>0 ? rx : ::rxClock;
  const double t=tx>0 ? tx : ::txClock;
  if(configDialogPtr) configDialogPtr->setSoundClocks(r,t);
  else {::rxClock=r; ::txClock=t;}
  QSettings s;
  s.beginGroup("SOUND");
  s.setValue("rxclock",::rxClock);
  s.setValue("txclock",::txClock);
  s.endGroup();
  s.setValue("Calibration/when",QDate::currentDate().toString(Qt::ISODate));
  s.setValue("Calibration/how",how.toInt());
  s.setValue("Calibration/device",deviceText());
  emit clocksChanged();
}

void CalibrationController::reset()
{
  if(m_running) stop();
  if(configDialogPtr) configDialogPtr->setSoundClocks(BASESAMPLERATE,BASESAMPLERATE);
  else {::rxClock=BASESAMPLERATE; ::txClock=BASESAMPLERATE;}
  QSettings s;
  s.beginGroup("SOUND");
  s.setValue("rxclock",::rxClock);
  s.setValue("txclock",::txClock);
  s.endGroup();
  s.remove("Calibration/when");
  s.remove("Calibration/how");
  s.remove("Calibration/device");
  emit clocksChanged();
}

// ---- internet time ----

void CalibrationController::startNtp()
{
  if(!m_sntp)
    {
      m_sntp=new sntpClient(this);
      connect(m_sntp,SIGNAL(sampleReady(int,double,double,double)),this,SLOT(slotSample(int,double,double,double)));
      connect(m_sntp,SIGNAL(statusChanged(QString)),this,SLOT(slotNtpStatus(QString)));
    }
  clearResult();
  m_t0=rawMonotonicSeconds();
  if(!startSound()) return;
  m_sntp->setServers(m_servers.split(QRegularExpression("[,;\\s]+"),Qt::SkipEmptyParts));
  m_sntp->start();
  setRunningFlag(m_running,true);
  m_timer.start(1000);
  ntpDisplay();
  changed();
  emit plotChanged();
}

void CalibrationController::slotNtpStatus(const QString &text)
{
  m_lastNtpStatus=text;
  ntpDisplay();
  changed();
}

void CalibrationController::slotSample(int server,double localTime,double theta,double)
{
  linePoint p;
  p.x=localTime-m_t0;
  p.y=theta;
  p.group=server;
  m_ntpPts.push_back(p);
  m_lastNtpStatus.clear();
  ntpSolve();
  ntpDisplay();
  ++m_plotRevision;
  emit plotChanged();
  changed();
}

void CalibrationController::slotTimer()
{
  if(!m_running) return;
  if(m_method==1) ntpTick();
  else if(m_method==2) wwvTick();
}

void CalibrationController::ntpTick()
{
  m_elapsed=rawMonotonicSeconds()-m_t0;
  // nothing uses the raw samples here, just keep the buffer from filling up
  std::vector<double> block(DOWNSAMPLESIZE);
  unsigned int avail=soundIOPtr->rawRxBuffer.count();
  while(avail>0)
    {
      unsigned int n=std::min<unsigned int>(avail,DOWNSAMPLESIZE);
      if(!soundIOPtr->rawRxBuffer.get(block.data(),n)) break;
      avail-=n;
    }
  m_stampBuf.clear();
  soundIOPtr->takeListenStamps(m_stampBuf);
  for(size_t i=0;i<m_stampBuf.size();i++)
    {
      const double x=m_stampBuf[i].time-m_t0;
      const double frames=double(m_stampBuf[i].frames);
      if(x<0) continue;
      const long w=long(std::floor(x/AUDIOWINDOW));
      // lateness of this block: how far its arrival is behind the arrival a perfect clock would give
      const double late=x-frames/m_streamRate;
      if(!m_haveWindow || w!=m_currentWindow)
        {
          if(m_haveWindow) m_audioPts.push_back(m_windowBest);
          m_haveWindow=true;
          m_currentWindow=w;
          m_windowBest.x=x;
          m_windowBest.y=frames;
          m_windowBest.group=0;
          m_windowLate=late;
        }
      else if(late<m_windowLate)
        {
          m_windowBest.x=x;
          m_windowBest.y=frames;
          m_windowLate=late;
        }
    }
  ntpSolve();
  ntpDisplay();
  ++m_plotRevision;
  emit plotChanged();
  changed();
}

void CalibrationController::ntpSolve()
{
  // soundcard (the open window counts too)
  std::vector<linePoint> ap=m_audioPts;
  if(m_haveWindow) ap.push_back(m_windowBest);
  std::vector<double> ares;
  std::vector<bool> ain;
  m_audioFit=fitLine(ap,CLIPSIGMA,AUDIOMINCLIP*m_streamRate,&ares,&ain);
  std::vector<double> nres;
  std::vector<bool> nin;
  m_ntpFit=fitLine(m_ntpPts,CLIPSIGMA,NTPMINCLIP,&nres,&nin);
  m_valid=m_audioFit.valid && m_ntpFit.valid && m_audioFit.inliers>=MINAUDIOPOINTS && m_ntpFit.inliers>=MINNTPSAMPLES;
  if(m_audioFit.valid && m_ntpFit.valid)
    {
      const double rateLocal=m_audioFit.slope;                       // frames per local second
      const double trueRate=rateLocal/(1.0+m_ntpFit.slope);
      m_ppm=(trueRate/m_streamRate-1.0)*1e6;
      const double ea=m_audioFit.slopeError/rateLocal*1e6;
      const double en=m_ntpFit.slopeError*1e6;
      m_ppmError=std::sqrt(ea*ea+en*en);
    }
  // plot data
  m_audioTrace=calTrace();
  if(m_audioFit.valid)
    {
      const double f0=ap.front().y,x0=ap.front().x;
      for(size_t i=0;i<ap.size();i++)
        {
          m_audioTrace.x.push_back(ap[i].x-x0);
          m_audioTrace.y.push_back(((ap[i].y-f0)/m_streamRate-(ap[i].x-x0))*1000.0);
          m_audioTrace.in.push_back(ain[i]);
        }
      m_audioTrace.slope=(m_audioFit.slope/m_streamRate-1.0)*1000.0;
      m_audioTrace.valid=true;
    }
  m_ntpTrace=calTrace();
  if(!m_ntpPts.empty())
    {
      // every server shifted to start at zero (each has its own constant offset)
      std::vector<double> first(32,0.0);
      std::vector<bool> have(32,false);
      const double x0=m_ntpPts.front().x;
      for(size_t i=0;i<m_ntpPts.size();i++)
        {
          const int g=std::min(std::max(m_ntpPts[i].group,0),31);
          if(!have[g]) {have[g]=true;first[g]=m_ntpPts[i].y;}
          m_ntpTrace.x.push_back(m_ntpPts[i].x-x0);
          m_ntpTrace.y.push_back((m_ntpPts[i].y-first[g])*1000.0);
          m_ntpTrace.in.push_back(m_ntpFit.valid ? bool(nin.size()>i && nin[i]) : true);
        }
      m_ntpTrace.slope=m_ntpFit.slope*1000.0;
      m_ntpTrace.valid=m_ntpFit.valid;
    }
}

bool CalibrationController::ntpHasResult() const
{
  if(!m_valid) return false;
  if(m_ntpFit.span<MINSECONDS) return false;
  return m_ppmError<=MAXACCEPTPPM && std::fabs(m_ppm)<=MAXPPM;
}

void CalibrationController::ntpDisplay()
{
  const bool h=ntpHasResult();
  QString problem;
  if(!m_running && !m_valid)
    setStatus(m_problem.isEmpty() ? tr("Not started") : QString());
  else if(!m_lastNtpStatus.isEmpty() && m_ntpPts.size()<size_t(MINNTPSAMPLES))
    {
      setStatus(tr("Waiting for the time servers"));
      problem=m_lastNtpStatus;
    }
  else if(m_ntpPts.size()<size_t(MINNTPSAMPLES))
    setStatus(tr("Contacting the time servers..."));
  else if(m_valid && std::fabs(m_ppm)>MAXPPM)
    {
      setStatus(tr("Collecting data..."));
      problem=tr("Error larger than 0.2%: is the input device the right one? Not accepted.");
    }
  else if(!h)
    setStatus(tr("Collecting data..."));
  else
    setStatus(m_running ? tr("Good enough to save.") : tr("Stopped. Save to use this result."));
  if(m_running || !problem.isEmpty()) setProblem(problem);
  if(m_ntpFit.valid && m_audioFit.valid)
    m_detail=tr("%1 server samples over %2 s, scatter %3 ms; sound device scatter %4 ms; the phone's own clock is %5 ppm from UTC")
             .arg(m_ntpFit.inliers).arg(m_ntpFit.span,0,'f',0).arg(m_ntpFit.rms*1000.0,0,'f',2)
             .arg(m_audioFit.rms/m_streamRate*1000.0,0,'f',2).arg(m_ntpFit.slope*1e6,0,'f',1);
  else
    m_detail=tr("%1 server samples").arg(m_ntpPts.size());
  m_hadResult=h;
}

// ---- radio time signal ----

void CalibrationController::startWwv()
{
  if(!startSound()) return;
  const double tone=m_station==0 ? 1000.0 : 1200.0;
  if(!m_detector || m_streamRate!=m_detectorRate)
    {
      delete m_detector;
      delete m_fit;
      m_detector=new wwvTickDetector(m_streamRate,tone);
      m_fit=new wwvTickFit(m_streamRate);
      m_detectorRate=m_streamRate;
    }
  m_detector->setTone(tone);
  clearResult();
  m_lastOverruns=soundIOPtr->listenOverruns();
  setRunningFlag(m_running,true);
  m_timer.start(100);
  wwvDisplay();
  changed();
  emit plotChanged();
}

void CalibrationController::updateLevel(const double *s,int n)
{
  double peak=0;
  for(int i=0;i<n;i++) peak=std::max(peak,std::fabs(s[i]));
  const double db=20.0*std::log10(peak/32768.0+1e-9);
  const qreal lv=qBound(0.0,(db+60.0)/60.0,1.0);
  if(lv>m_level) m_level=lv;
}

void CalibrationController::wwvTick()
{
  if(soundIOPtr->listenOverruns()!=m_lastOverruns)
    {
      // samples were lost, so the tick positions after the gap don't line up with those before it
      m_lastOverruns=soundIOPtr->listenOverruns();
      m_detector->reset();
      m_fit->reset();
      m_hadResult=false;
    }
  std::vector<double> block(DOWNSAMPLESIZE);
  std::vector<wwvCandidate> found;
  m_level*=0.85;   // the meter falls back between the ticks
  unsigned int avail=soundIOPtr->rawRxBuffer.count();
  while(avail>0)
    {
      const unsigned int n=std::min<unsigned int>(avail,DOWNSAMPLESIZE);
      if(!soundIOPtr->rawRxBuffer.get(block.data(),n)) break;
      updateLevel(block.data(),int(n));
      m_detector->process(block.data(),int(n),found);
      avail-=n;
    }
  emit levelChanged();
  if(!found.empty())
    {
      for(size_t i=0;i<found.size();i++) m_fit->add(found[i]);
      m_fit->solve();
      ++m_plotRevision;
      emit plotChanged();
    }
  wwvDisplay();
  changed();
}

bool CalibrationController::wwvHasResult() const
{
  if(!m_fit) return false;
  const wwvFitResult &r=m_fit->result();
  return r.valid && r.ticks>=GOODTICKS && std::fabs(r.ppm)<=MAXPPM;
}

void CalibrationController::wwvDisplay()
{
  if(!m_fit) {setStatus(tr("Not started")); return;}
  const wwvFitResult &r=m_fit->result();
  QString problem;
  if(!m_running && !r.valid)
    setStatus(m_problem.isEmpty() ? tr("Not started") : QString());
  else if(!r.valid)
    setStatus(tr("Listening: looking for the time ticks..."));
  else if(r.ticks<GOODTICKS)
    setStatus(tr("Ticks found, collecting more..."));
  else if(std::fabs(r.ppm)>MAXPPM)
    {
      setStatus(tr("Collecting data..."));
      problem=tr("Error larger than 0.2%: wrong station or signal? Not accepted.");
    }
  else
    setStatus(m_running ? tr("Good enough to save. Keep it running for more accuracy.") : tr("Stopped. Save to use this result."));
  if(m_running || !problem.isEmpty()) setProblem(problem);
  if(r.valid)
    m_detail=tr("%1 ticks over %2 s, scatter %3 ms").arg(r.ticks).arg(r.spanSeconds,0,'f',0).arg(r.rmsMs,0,'f',2);
  else
    m_detail.clear();
  m_hadResult=wwvHasResult();
}

// ---- another SSTV station ----

QStringList CalibrationController::sstvModes() const
{
  QStringList l;
  for(const esstvMode m:sstvModeList)
    l.append(tr("%1 (%2 s)").arg(SSTVTable[m].name).arg(SSTVTable[m].imageTime,0,'f',0));
  return l;
}

void CalibrationController::setSstvRole(int r)
{
  r=qBound(0,r,1);
  if(r==m_sstvRole) return;
  if(m_running || m_sstvSending) stop();
  m_sstvRole=r;
  if(m_method==3)
    {
      setStatus(r==0 ? tr("Press Listen, then have the reference station send its picture.")
                     : tr("Sends a black picture with one straight white line. Your own picture is put back afterwards."));
      setProblem(QString());
    }
  emit sstvChanged();
  changed();
}

void CalibrationController::setSstvModeIndex(int i)
{
  i=qBound(0,i,int(sizeof(sstvModeList)/sizeof(sstvModeList[0]))-1);
  if(i==m_sstvMode) return;
  m_sstvMode=i;
  QSettings().setValue("Calibration/sstvMode",i);
  emit sstvChanged();
}

void CalibrationController::setSstvPosition(int p)
{
  p=qBound(10,p,90);
  if(p==m_sstvPosition) return;
  m_sstvPosition=p;
  QSettings().setValue("Calibration/sstvPosition",p);
  emit sstvChanged();
}

// the picture is read and sent in analog SSTV: a digital (DRM) page is switched for the time of the calibration
static bool needAnalog(bool &wasDigital,bool &switched)
{
  AppModel *a=AppModel::instance();
  if(!a->digital()) return true;
  if(!switched) {wasDigital=true;switched=true;}
  a->setDigital(false);
  return !a->digital();
}

void CalibrationController::startSstvListening()
{
  if(m_sstvListening || m_sstvSending || !dispatcherPtr) return;
  if(AppModel::instance()->micDenied())
    {
      setProblem(tr("The microphone is not allowed for this app."),1);
      changed();
      return;
    }
  takeSound();
  if(!needAnalog(m_wasDigital,m_modeSwitched))
    {
      setProblem(tr("Switch to Analog SSTV first: a transmission or reception is going on."));
      changed();
      return;
    }
  clearResult();
  m_savedAutoSlant=autoSlantAdjust;
  autoSlantAdjust=false;
  dispatcherPtr->setCalibrationRx(true);
  connect(dispatcherPtr,SIGNAL(sstvImageReceived(int)),this,SLOT(slotSstvImageReceived(int)),Qt::UniqueConnection);
  connect(dispatcherPtr,SIGNAL(sstvLineReceived()),this,SLOT(slotSstvLine()),Qt::UniqueConnection);
  dispatcherPtr->startRX();
  m_sstvListening=true;
  setRunningFlag(m_running,true);
  sstvDisplay();
  emit sstvChanged();
  changed();
  emit plotChanged();
}

void CalibrationController::stopSstvListening()
{
  if(!m_sstvListening) return;
  disconnect(dispatcherPtr,SIGNAL(sstvImageReceived(int)),this,SLOT(slotSstvImageReceived(int)));
  disconnect(dispatcherPtr,SIGNAL(sstvLineReceived()),this,SLOT(slotSstvLine()));
  dispatcherPtr->setCalibrationRx(false);
  dispatcherPtr->idleAll();
  autoSlantAdjust=m_savedAutoSlant;
  m_sstvListening=false;
  setRunningFlag(m_running,false);
  emit sstvChanged();
}

void CalibrationController::slotSstvLine()
{
  static QElapsedTimer clock;
  if(!clock.isValid()) clock.start();
  const qint64 now=clock.elapsed();
  if(now-m_lastLivePaint<200) return;
  m_lastLivePaint=now;
  if(!rxWidgetPtr) return;
  const QImage *im=rxWidgetPtr->getImageViewerPtr()->getDisplayedImage();
  if(im->isNull()) return;
  m_sstvImage=*im;
  m_sstvFitResult=slantFitResult();
  setStatus(tr("Receiving the picture..."));
  ++m_plotRevision;
  emit plotChanged();
  changed();
}

void CalibrationController::slotSstvImageReceived(int m)
{
  const esstvMode mode=esstvMode(m);
  if(mode<0 || mode>=NUMSSTVMODES)
    {
      setStatus(tr("Picture incomplete (too few lines): waiting for the next one."));
      changed();
      return;
    }
  // the line period and the pixel duration are in the same unit (samples at the subsampled clock)
  const double clk=rxClock/SUBSAMPLINGFACTOR;
  const double pixDur=rxSSTVParam.pixelDuration;
  if(pixDur<=0)
    {
      setStatus(tr("Mode %1 cannot be used for calibration.").arg(SSTVTable[mode].name));
      changed();
      return;
    }
  const double pixelsPerPeriod=getLineLength(mode,clk)/pixDur;
  const double rowsPerPeriod=double(SSTVTable[mode].numberOfDisplayLines)/SSTVTable[mode].numberOfDataLines;
  // the received picture is the displayed one; getImagePtr() is the (stale) source image
  const QImage img=*rxWidgetPtr->getImageViewerPtr()->getDisplayedImage();
  const QImage gray=img.convertToFormat(QImage::Format_Grayscale8);
  const slantFitResult r=fitSlant(gray.constBits(),gray.width(),gray.height(),gray.bytesPerLine());
  m_sstvImage=img;
  m_sstvFitResult=r;
  ++m_plotRevision;
  emit plotChanged();
  if(!r.valid || r.used<MINROWS || r.rms>MAXRMS)
    {
      setStatus(tr("%1 received, but no straight line could be measured (%2 of %3 rows). Waiting for the next one.")
                .arg(SSTVTable[mode].name).arg(r.used).arg(r.total));
      changed();
      return;
    }
  const double err=slantToClockError(r.slope,rowsPerPeriod,pixelsPerPeriod);
  const double ppm=err*1e6;
  if(std::fabs(ppm)>MAXPPM)
    {
      setStatus(tr("%1 received, but the slant (%2 ppm) is too large to be a sample rate difference. Is this the calibration picture?")
                .arg(SSTVTable[mode].name).arg(ppm,0,'f',0));
      changed();
      return;
    }
  m_sstvPpms.push_back(ppm);
  m_sstvClocks.push_back(rxClock*(1.0+err));
  m_sstvLast=tr("Last picture: %1 ppm, line fitted on %2 of %3 rows, deviation %4 pixels.")
             .arg(ppm,0,'f',1).arg(r.used).arg(r.total).arg(r.rms,0,'f',2);
  setStatus(tr("%1 received, %n pictures so far. Listening for more.","",int(m_sstvPpms.size())).arg(SSTVTable[mode].name));
  m_detail=m_sstvLast;
  emit sstvChanged();
  changed();
}

void CalibrationController::sstvDisplay()
{
  if(m_sstvSending)
    {
      const esstvMode mode=sstvModeList[m_sstvMode];
      setStatus(tr("Sending the calibration picture in %1: about %2 minutes. Press Stop to end it.")
                .arg(SSTVTable[mode].name).arg(SSTVTable[mode].imageTime/60.0,0,'f',1));
    }
  else if(m_sstvRole==1)
    setStatus(tr("Sends a black picture with one straight white line. Your own picture is put back afterwards."));
  else if(m_sstvListening)
    {
      if(m_sstvPpms.empty()) setStatus(tr("Listening: waiting for the reference picture."));
    }
  else if(m_sstvPpms.empty())
    setStatus(tr("Press Listen, then have the reference station send its picture."));
}

QImage CalibrationController::makeReferenceImage(int modeIndex,int percent) const
{
  const esstvMode mode=sstvModeList[qBound(0,modeIndex,int(sizeof(sstvModeList)/sizeof(sstvModeList[0]))-1)];
  const int w=SSTVTable[mode].numberOfPixels;
  const int h=SSTVTable[mode].numberOfDisplayLines;
  QImage im(w,h,QImage::Format_ARGB32_Premultiplied);
  im.fill(QColor(0,0,0));
  int x0=int(std::lround(percent/100.0*w))-LINEWIDTH/2;
  x0=qBound(0,x0,w-LINEWIDTH);
  for(int y=0;y<h;y++)
    {
      QRgb *line=reinterpret_cast<QRgb *>(im.scanLine(y));
      for(int x=x0;x<x0+LINEWIDTH;x++) line[x]=qRgb(255,255,255);
    }
  return im;
}

void CalibrationController::sendReference()
{
  if(m_test || !txWidgetPtr) return;
  if(m_sstvSending)
    {
      txWidgetPtr->abortCalibrationTx();
      return;
    }
  if(m_sstvListening) stopSstvListening();
  setProblem(QString());
  takeSound();
  if(!needAnalog(m_wasDigital,m_modeSwitched))
    {
      setProblem(tr("Switch to Analog SSTV first: a transmission or reception is going on."));
      changed();
      return;
    }
  const esstvMode mode=sstvModeList[m_sstvMode];
  connect(txWidgetPtr,SIGNAL(calibrationTxFinished()),this,SLOT(slotSstvTxFinished()),Qt::UniqueConnection);
  connect(txWidgetPtr,SIGNAL(progressChanged(int)),this,SLOT(slotSstvTxProgress(int)),Qt::UniqueConnection);
  connect(txWidgetPtr,SIGNAL(calibrationTxFailed(QString)),this,SLOT(slotSstvTxFailed(QString)),Qt::UniqueConnection);
  if(!txWidgetPtr->sendCalibrationImage(mode,makeReferenceImage(m_sstvMode,m_sstvPosition)))
    {
      setProblem(tr("The picture cannot be sent now: the transmitter is busy, or a multi picture grid is chosen on the Transmit screen."));
      changed();
      return;
    }
  m_sstvSending=true;
  m_sstvProgress=0;
  setRunningFlag(m_running,true);
  sstvDisplay();
  emit sstvChanged();
  changed();
}

void CalibrationController::slotSstvTxProgress(int percent)
{
  if(!m_sstvSending) return;
  m_sstvProgress=percent/100.0;
  emit sstvChanged();
}

void CalibrationController::slotSstvTxFailed(const QString &why)
{
  if(!m_sstvSending) return;
  setProblem(why);
}

void CalibrationController::slotSstvTxFinished()
{
  if(!m_sstvSending) return;
  m_sstvSending=false;
  m_sstvProgress=0;
  setRunningFlag(m_running,false);
  sstvDisplay();
  emit sstvChanged();
  changed();
}

// ---- the screenshots of the test harness ----

void CalibrationController::setTestState(int method,int phase)
{
  if(m_running) stopMeasuring();
  m_test=true;
  m_method=method;
  clearResult();
  setProblem(QString());
  m_streamRate=BASESAMPLERATE;
  const bool run=phase==1 || phase==2 || phase==3;
  setRunningFlag(m_running,false);
  m_running=run;   // no screen lock in the test
  if(method==1)
    {
      if(phase==1 || phase==2)
        {
          const double span=phase==1 ? 48.0 : 200.0;
          m_elapsed=span;
          // the sound device: frames against local time, with a rate 12.9 ppm fast; the servers: a flat line with a little scatter
          unsigned int seed=7;
          auto rnd=[&seed]() {seed=seed*1103515245u+12345u; return double((seed>>8)&0xffff)/65535.0-0.5;};
          const int n=int(span/AUDIOWINDOW);
          for(int i=0;i<=n;i++)
            {
              linePoint p;
              p.x=1.0+i*AUDIOWINDOW;
              p.y=(p.x*BASESAMPLERATE)*(1.0+12.9e-6)+rnd()*0.0004*BASESAMPLERATE;
              p.group=0;
              m_audioPts.push_back(p);
            }
          const int k=std::max(3,int(span/64.0)*3+3);
          for(int i=0;i<k;i++)
            {
              linePoint p;
              p.x=2.0+i*span/k;
              p.y=0.0015+rnd()*0.002;
              p.group=i%3;
              m_ntpPts.push_back(p);
            }
          ntpSolve();
        }
      if(phase==3) m_lastNtpStatus=QStringLiteral("time.nist.gov: no reply");
      m_running=run;
      ntpDisplay();
      if(phase==3) setProblem(tr("%1. Is the phone online?").arg(m_lastNtpStatus));
    }
  else if(method==2)
    {
      if(!m_detector) {m_detector=new wwvTickDetector(m_streamRate,1000.0); m_fit=new wwvTickFit(m_streamRate); m_detectorRate=m_streamRate;}
      m_fit->reset();
      if(phase>=1)
        {
          unsigned int seed=3;
          auto rnd=[&seed]() {seed=seed*1103515245u+12345u; return double((seed>>8)&0xffff)/65535.0-0.5;};
          const int ticks=phase==1 ? 12 : 34;
          for(int i=0;i<ticks;i++)
            {
              wwvCandidate c;
              c.position=1000.0+i*BASESAMPLERATE*(1.0+7.3e-6)+rnd()*40.0;
              c.strength=8+rnd()*2;
              m_fit->add(c);
            }
          m_fit->solve();
        }
      m_level=0.52;
      wwvDisplay();
    }
  else if(method==3)
    {
      if(phase>=1)
        {
          // a picture with a slanted line, and the fit that was made on it
          QImage im(320,240,QImage::Format_ARGB32_Premultiplied);
          im.fill(Qt::black);
          for(int y=0;y<im.height();y++)
            {
              QRgb *line=reinterpret_cast<QRgb *>(im.scanLine(y));
              const int x0=int(150+0.06*y);
              for(int x=x0;x<x0+3;x++) line[x]=qRgb(255,255,255);
            }
          const QImage gray=im.convertToFormat(QImage::Format_Grayscale8);
          m_sstvImage=im;
          m_sstvFitResult=fitSlant(gray.constBits(),gray.width(),gray.height(),gray.bytesPerLine());
          if(phase>=2)
            {
              m_sstvPpms={11.8,12.4};
              m_sstvClocks={BASESAMPLERATE*(1+11.8e-6),BASESAMPLERATE*(1+12.4e-6)};
              m_detail=tr("Last picture: %1 ppm, line fitted on %2 of %3 rows, deviation %4 pixels.").arg(12.4,0,'f',1).arg(238).arg(240).arg(0.4,0,'f',2);
              setStatus(tr("%1 received, %n pictures so far. Listening for more.","",2).arg(QStringLiteral("PD120")));
            }
          else setStatus(tr("Receiving the picture..."));
          m_sstvListening=m_sstvRole==0;
        }
      if(m_sstvRole==1 && phase>=1)
        {
          m_sstvSending=true;
          m_sstvProgress=0.62;
          sstvDisplay();
        }
      if(phase==0) sstvDisplay();
    }
  ++m_plotRevision;
  emit sstvChanged();
  changed();
  emit plotChanged();
  emit levelChanged();
}
