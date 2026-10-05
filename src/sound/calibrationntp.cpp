#include "calibrationntp.h"
#include "sntpclient.h"
#include "rawclock.h"
#include "appglobal.h"
#include "dispatcher.h"

#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QFont>
#include <QRegularExpression>
#include <cmath>
#include <algorithm>

#define DEFAULTSERVERS "time.nist.gov, time.cloudflare.com, 0.pool.ntp.org"
#define AUDIOWINDOW 10.0        // seconds; block arrival is only ever late, so the earliest block of a window is kept
#define MINNTPSAMPLES 5
#define MINAUDIOPOINTS 5
#define MINSECONDS 120.0        // shortest measurement that can be accepted
#define MAXACCEPTPPM 10.0       // largest standard error that can be accepted
#define MAXPPM 2000.0           // 0.2%: the same limit soundConfig applies to a stored clock
#define CLIPSIGMA 3.0
#define NTPMINCLIP 0.001        // s, never clip NTP samples closer than this to the line
#define AUDIOMINCLIP 0.002      // s

struct plotTrace
{
  std::vector<double> x;        // s
  std::vector<double> y;        // ms
  std::vector<bool> in;
  double slope;                 // ms per s, of the fit
  bool valid;
  plotTrace() : slope(0),valid(false) {}
};

/*!
  Two stacked traces against time, each point as an offset in ms with the fit as a red line; the slope of the
  line is the clock error (1 ppm = 1 us per second). Top: the NTP servers (server time minus local time, every
  server shifted to start at zero). Bottom: the soundcard (frames received converted to seconds, minus local
  time). The difference of the two slopes is the sample rate error.
*/
class ntpPlot : public QWidget
{
public:
  explicit ntpPlot(QWidget *parent=nullptr) : QWidget(parent)
  {
    setMinimumHeight(170);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  }
  plotTrace ntp,audio;

protected:
  void paintEvent(QPaintEvent *)
  {
    QPainter p(this);
    p.fillRect(rect(),QColor(10,10,40));
    const int h=height()/2;
    drawTrace(p,QRect(0,0,width(),h),ntp,tr("NTP servers"));
    drawTrace(p,QRect(0,h,width(),height()-h),audio,tr("soundcard"));
  }

private:
  void drawTrace(QPainter &p,const QRect &r,const plotTrace &t,const QString &name)
  {
    p.setPen(QColor(40,40,90));
    p.drawRect(r.adjusted(0,0,-1,-1));
    if(t.x.size()<2)
      {
        p.setPen(QColor(150,150,180));
        p.drawText(r,Qt::AlignCenter,tr("%1: waiting for samples").arg(name));
        return;
      }
    const int left=4,right=4,top=4,bottom=14;
    double x0=t.x.front(),x1=t.x.back();
    double span=std::max(x1-x0,1.0);
    // centre on the mean of the inliers
    double mean=0;
    int n=0;
    for(size_t i=0;i<t.x.size();i++) if(t.in[i]) {mean+=t.y[i];n++;}
    mean=(n>0)? mean/n:0;
    double ymax=1.0;                      // at least +/- 1 ms
    for(size_t i=0;i<t.x.size();i++) if(t.in[i]) ymax=std::max(ymax,1.3*std::fabs(t.y[i]-mean));
    const double pw=r.width()-left-right;
    const double ph=r.height()-top-bottom;
    auto xOf=[&](double x){return r.left()+left+pw*(x-x0)/span;};
    auto yOf=[&](double y){return r.top()+top+ph*(0.5-0.5*(y-mean)/ymax);};
    p.setPen(QPen(QColor(0,200,0),1));
    p.drawLine(QPointF(xOf(x0),yOf(mean)),QPointF(xOf(x1),yOf(mean)));
    for(size_t i=0;i<t.x.size();i++)
      {
        QColor c=t.in[i]? QColor(255,255,255):QColor(150,60,60);
        p.fillRect(QRectF(xOf(t.x[i])-1.5,yOf(t.y[i])-1.5,3.0,3.0),c);
      }
    if(t.valid)
      {
        double xc=0;
        int m=0;
        for(size_t i=0;i<t.x.size();i++) if(t.in[i]) {xc+=t.x[i];m++;}
        xc=(m>0)? xc/m:x0;
        p.setPen(QPen(QColor(255,60,60),1));
        p.drawLine(QPointF(xOf(x0),yOf(mean+t.slope*(x0-xc))),QPointF(xOf(x1),yOf(mean+t.slope*(x1-xc))));
      }
    p.setPen(QColor(150,150,180));
    p.drawText(QRect(r.left(),r.bottom()-bottom,r.width(),bottom),Qt::AlignCenter,
               tr("%1   +/- %2 ms   |   %3 s   |   green: flat   red: fit").arg(name).arg(ymax,0,'f',1).arg(span,0,'f',0));
  }
};


calibrationNtp::calibrationNtp(QWidget *parent) : calibrationMethod(parent)
{
  running=false;
  hadResult=false;
  streamRate=BASESAMPLERATE;
  t0=0;
  haveWindow=false;
  currentWindow=0;
  windowBest.x=windowBest.y=0;
  windowBest.group=0;
  windowLate=0;
  ppm=ppmError=0;
  valid=false;

  QVBoxLayout *layout=new QVBoxLayout(this);
  QLabel *info=new QLabel(tr("Press Start and let it run about 30 minutes. Press Save when the reading is stable. Use a direct "
                             "soundcard device (not one the system resamples)."),this);
  info->setWordWrap(true);
  layout->addWidget(info);

  QHBoxLayout *row=new QHBoxLayout;
  row->addWidget(new QLabel(tr("Servers"),this));
  serversEdit=new QLineEdit(DEFAULTSERVERS,this);
  serversEdit->setToolTip(tr("Comma separated host names or addresses. Three or more independent servers are best: a server "
                             "that disagrees with the others is ignored."));
  row->addWidget(serversEdit,1);
  startButton=new QPushButton(tr("Start"),this);
  row->addWidget(startButton);
  saveButton=new QPushButton(tr("Save"),this);
  saveButton->setToolTip(tr("Use this result and close."));
  row->addWidget(saveButton);
  layout->addLayout(row);

  statusLabel=new QLabel(this);
  statusLabel->setWordWrap(true);
  layout->addWidget(statusLabel);

  QHBoxLayout *readout=new QHBoxLayout;
  rateLabel=new QLabel("--",this);
  QFont big=rateLabel->font();
  big.setPointSize(big.pointSize()*2);
  big.setBold(true);
  rateLabel->setFont(big);
  ppmLabel=new QLabel("--",this);
  ppmLabel->setFont(big);
  readout->addWidget(rateLabel);
  readout->addStretch(1);
  readout->addWidget(ppmLabel);
  layout->addLayout(readout);
  detailLabel=new QLabel(this);
  detailLabel->setWordWrap(true);
  layout->addWidget(detailLabel);

  plot=new ntpPlot(this);
  layout->addWidget(plot,1);
  plot->setMinimumWidth(300);

  applyTxCheck=new QCheckBox(tr("Also use this result for the transmit clock"),this);
  applyTxCheck->setChecked(true);
  applyTxCheck->setToolTip(tr("This method listens to the soundcard input. Most soundcards and USB interfaces run input and output "
                              "from the same clock, so the receive measurement also applies to transmit. Untick if yours does not."));
  layout->addWidget(applyTxCheck);

  sntp=new sntpClient(this);
  connect(sntp,SIGNAL(sampleReady(int,double,double,double)),this,SLOT(slotSample(int,double,double,double)));
  connect(sntp,SIGNAL(statusChanged(QString)),this,SLOT(slotStatus(QString)));
  timer=new QTimer(this);
  timer->setInterval(1000);
  connect(timer,SIGNAL(timeout()),this,SLOT(slotTimer()));
  connect(startButton,SIGNAL(clicked()),this,SLOT(slotStartStop()));
  connect(saveButton,SIGNAL(clicked()),this,SLOT(slotSave()));
  connect(applyTxCheck,SIGNAL(toggled(bool)),this,SIGNAL(resultChanged()));
  updateDisplay();
}

calibrationNtp::~calibrationNtp()
{
  stop();
}

void calibrationNtp::slotStartStop()
{
  if(running) stop();
  else start();
}

void calibrationNtp::slotSave()
{
  if(!hasResult())
    {
      QMessageBox::information(this,tr("Calibration"),tr("No valid result yet. Keep the measurement running."));
      return;
    }
  stop();
  emit saveRequested();
}

void calibrationNtp::start()
{
  // the soundcard is needed exclusively: stop receive/transmit (the caller restarts receive afterwards)
  dispatcherPtr->idleAll();
  double sr=soundIOPtr->streamSampleRate();
  streamRate=(sr>0)? sr:BASESAMPLERATE;
  restartMeasurement();
  if(!soundIOPtr->startListen())
    {
      QMessageBox::critical(this,tr("Calibration error"),tr("Soundcard not active: %1").arg(soundIOPtr->getLastError()));
      return;
    }
  sntp->setServers(serversEdit->text().split(QRegularExpression("[,;\\s]+"),Qt::SkipEmptyParts));
  sntp->start();
  running=true;
  serversEdit->setEnabled(false);
  startButton->setText(tr("Stop"));
  timer->start();
  updateDisplay();
}

void calibrationNtp::stop()
{
  if(!running) return;
  timer->stop();
  sntp->stop();
  soundIOPtr->stopListen();
  running=false;
  serversEdit->setEnabled(true);
  startButton->setText(tr("Start"));
  updateDisplay();
}

void calibrationNtp::restartMeasurement()
{
  t0=rawMonotonicSeconds();
  stampBuf.clear();
  audioPts.clear();
  ntpPts.clear();
  haveWindow=false;
  lastStatus.clear();
  audioFit=lineFitResult();
  ntpFit=lineFitResult();
  valid=false;
  plot->ntp=plotTrace();
  plot->audio=plotTrace();
  if(hadResult)
    {
      hadResult=false;
      emit resultChanged();
    }
  updateDisplay();
}

void calibrationNtp::slotStatus(const QString &text)
{
  lastStatus=text;
  updateDisplay();
}

void calibrationNtp::slotSample(int server,double localTime,double theta,double)
{
  linePoint p;
  p.x=localTime-t0;
  p.y=theta;
  p.group=server;
  ntpPts.push_back(p);
  lastStatus.clear();
  solve();
  emit resultChanged();
  updateDisplay();
}

void calibrationNtp::slotTimer()
{
  if(!running) return;
  // nothing uses the raw samples here, just keep the buffer from filling up
  std::vector<double> block(DOWNSAMPLESIZE);
  unsigned int avail=soundIOPtr->rawRxBuffer.count();
  while(avail>0)
    {
      unsigned int n=std::min<unsigned int>(avail,DOWNSAMPLESIZE);
      if(!soundIOPtr->rawRxBuffer.get(block.data(),n)) break;
      avail-=n;
    }
  stampBuf.clear();
  soundIOPtr->takeListenStamps(stampBuf);
  for(size_t i=0;i<stampBuf.size();i++)
    {
      double x=stampBuf[i].time-t0;
      double frames=double(stampBuf[i].frames);
      if(x<0) continue;
      long w=long(std::floor(x/AUDIOWINDOW));
      // lateness of this block: how far its arrival is behind the arrival a perfect clock would give
      double late=x-frames/streamRate;
      if(!haveWindow || w!=currentWindow)
        {
          if(haveWindow) audioPts.push_back(windowBest);
          haveWindow=true;
          currentWindow=w;
          windowBest.x=x;
          windowBest.y=frames;
          windowBest.group=0;
          windowLate=late;
        }
      else if(late<windowLate)
        {
          windowBest.x=x;
          windowBest.y=frames;
          windowLate=late;
        }
    }
  solve();
  emit resultChanged();
  updateDisplay();
}

void calibrationNtp::solve()
{
  // soundcard (the open window counts too)
  std::vector<linePoint> ap=audioPts;
  if(haveWindow) ap.push_back(windowBest);
  std::vector<double> ares;
  std::vector<bool> ain;
  audioFit=fitLine(ap,CLIPSIGMA,AUDIOMINCLIP*streamRate,&ares,&ain);
  std::vector<double> nres;
  std::vector<bool> nin;
  ntpFit=fitLine(ntpPts,CLIPSIGMA,NTPMINCLIP,&nres,&nin);

  valid=audioFit.valid && ntpFit.valid && audioFit.inliers>=MINAUDIOPOINTS && ntpFit.inliers>=MINNTPSAMPLES;
  if(audioFit.valid && ntpFit.valid)
    {
      double rateLocal=audioFit.slope;                       // frames per local second
      double trueRate=rateLocal/(1.0+ntpFit.slope);
      ppm=(trueRate/streamRate-1.0)*1e6;
      double ea=audioFit.slopeError/rateLocal*1e6;
      double en=ntpFit.slopeError*1e6;
      ppmError=std::sqrt(ea*ea+en*en);
    }

  // plot data
  plot->audio=plotTrace();
  if(audioFit.valid)
    {
      double f0=ap.front().y,x0=ap.front().x;
      for(size_t i=0;i<ap.size();i++)
        {
          plot->audio.x.push_back(ap[i].x-x0);
          plot->audio.y.push_back(((ap[i].y-f0)/streamRate-(ap[i].x-x0))*1000.0);
          plot->audio.in.push_back(ain[i]);
        }
      plot->audio.slope=(audioFit.slope/streamRate-1.0)*1000.0;
      plot->audio.valid=true;
    }
  plot->ntp=plotTrace();
  if(!ntpPts.empty())
    {
      // every server shifted to start at zero (each has its own constant offset)
      std::vector<double> first(32,0.0);
      std::vector<bool> have(32,false);
      double x0=ntpPts.front().x;
      for(size_t i=0;i<ntpPts.size();i++)
        {
          int g=std::min(std::max(ntpPts[i].group,0),31);
          if(!have[g]) {have[g]=true;first[g]=ntpPts[i].y;}
          plot->ntp.x.push_back(ntpPts[i].x-x0);
          plot->ntp.y.push_back((ntpPts[i].y-first[g])*1000.0);
          plot->ntp.in.push_back(ntpFit.valid? bool(nin.size()>i && nin[i]):true);
        }
      plot->ntp.slope=ntpFit.slope*1000.0;
      plot->ntp.valid=ntpFit.valid;
    }
}

bool calibrationNtp::hasResult() const
{
  if(!valid) return false;
  if(ntpFit.span<MINSECONDS) return false;
  return ppmError<=MAXACCEPTPPM && std::fabs(ppm)<=MAXPPM;
}

double calibrationNtp::rxClockResult() const
{
  if(!hasResult()) return 0;
  return BASESAMPLERATE*(1.0+ppm*1e-6);
}

double calibrationNtp::txClockResult() const
{
  if(!applyTxCheck->isChecked()) return 0;
  return rxClockResult();
}

void calibrationNtp::updateDisplay()
{
  bool h=hasResult();
  if(!running && !valid)
    statusLabel->setText(tr("Press Start. Internet connection required."));
  else if(!lastStatus.isEmpty() && ntpPts.size()<(size_t)MINNTPSAMPLES)
    statusLabel->setText(lastStatus);
  else if(ntpPts.size()<(size_t)MINNTPSAMPLES)
    statusLabel->setText(tr("Contacting the time servers..."));
  else if(valid && std::fabs(ppm)>MAXPPM)
    statusLabel->setText(tr("Error larger than 0.2%: is the input device the right one? Not accepted."));
  else if(!h)
    statusLabel->setText(tr("Collecting data..."));
  else
    statusLabel->setText(running? tr("Press Save when the reading is stable, or keep running for more accuracy.") : tr("Stopped. Press Save to use this result."));
  if(valid)
    {
      rateLabel->setText(tr("%1 Hz").arg(streamRate*(1.0+ppm*1e-6),0,'f',2));
      ppmLabel->setText(tr("%1%2 ppm").arg(ppm>=0? "+":"").arg(ppm,0,'f',1)+QString(" (±%1)").arg(ppmError,0,'f',1));
    }
  else
    {
      rateLabel->setText("--");
      ppmLabel->setText("--");
    }
  if(ntpFit.valid && audioFit.valid)
    detailLabel->setText(tr("%1 server samples over %2 s, scatter %3 ms; soundcard scatter %4 ms; local clock %5 ppm from UTC")
                         .arg(ntpFit.inliers).arg(ntpFit.span,0,'f',0).arg(ntpFit.rms*1000.0,0,'f',2)
                         .arg(audioFit.rms/streamRate*1000.0,0,'f',2).arg(ntpFit.slope*1e6,0,'f',1));
  else
    detailLabel->setText(tr("%1 server samples").arg(ntpPts.size()));
  plot->update();
}
