#include "calibrationwwv.h"
#include "appglobal.h"
#include "soundbase.h"
#include "dispatcher.h"

#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QFont>
#include <cmath>
#include <algorithm>

#define GOODTICKS 20            // ticks needed before the result can be accepted
#define MAXPPM 2000.0           // 0.2%: the same limit soundConfig applies to a stored clock
#define WATERFALLROWS 400       // most recent ticks shown in the waterfall

/*!
  The tick waterfall as in MMSSTV: one row per second, the tick drawn at its offset from where a perfect
  clock would put it. A vertical line is a correct sample rate; every 1 ppm of error moves the line by 1 us
  per second, so the slant gives the error. The red line is the fit.
*/
class wwvWaterfall : public QWidget
{
public:
  explicit wwvWaterfall(QWidget *parent=nullptr) : QWidget(parent),fitPtr(nullptr)
  {
    setMinimumHeight(180);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  }
  void setFit(const wwvTickFit *f) {fitPtr=f; update();}

protected:
  void paintEvent(QPaintEvent *)
  {
    QPainter p(this);
    p.fillRect(rect(),QColor(10,10,40));
    if(fitPtr==nullptr || fitPtr->points().empty())
      {
        p.setPen(QColor(150,150,180));
        p.drawText(rect(),Qt::AlignCenter,tr("waiting for time ticks"));
        return;
      }
    const std::vector<wwvFitPoint> &pts=fitPtr->points();
    const double nominal=fitPtr->nominalRate();
    size_t first=(pts.size()>WATERFALLROWS)? pts.size()-WATERFALLROWS:0;
    // reference: the first tick on the fitted line (or the first tick shown); offsets are relative to a perfect clock
    double ref=pts[first].position;
    for(size_t i=first;i<pts.size();i++) if(pts[i].inlier) {ref=pts[i].position;break;}
    double t0=pts[first].position;
    double t1=pts.back().position;
    double tspan=std::max(t1-t0,10.0*nominal);
    std::vector<double> dx(pts.size());
    double maxDx=0.02*nominal;                  // at least +/- 20 ms
    for(size_t i=first;i<pts.size();i++)
      {
        double d=pts[i].position-ref;
        dx[i]=d-std::round(d/nominal)*nominal;
        if(pts[i].inlier) maxDx=std::max(maxDx,1.3*std::fabs(dx[i]));
      }
    maxDx=std::min(maxDx,0.5*nominal);
    const int w=width();
    const int h=height();
    const int left=4,right=4,top=4,bottom=16;
    const double pw=w-left-right;
    const double ph=h-top-bottom;
    auto xOf=[&](double d){return left+pw*(0.5+0.5*d/maxDx);};
    auto yOf=[&](double pos){return top+ph*(pos-t0)/tspan;};
    // reference line (a perfect clock) and 10 ms grid
    p.setPen(QColor(40,40,90));
    for(double ms=-1000.0*maxDx/nominal;ms<=1000.0*maxDx/nominal;ms+=10.0)
      {
        double m=std::round(ms/10.0)*10.0;
        if(std::fabs(m)>1000.0*maxDx/nominal) continue;
        p.drawLine(QPointF(xOf(m*1e-3*nominal),top),QPointF(xOf(m*1e-3*nominal),top+ph));
      }
    p.setPen(QPen(QColor(0,200,0),1));
    p.drawLine(QPointF(xOf(0),top),QPointF(xOf(0),top+ph));
    for(size_t i=first;i<pts.size();i++)
      {
        QColor c=pts[i].inlier? QColor(255,255,255):QColor(150,60,60);
        p.fillRect(QRectF(xOf(dx[i])-1.5,yOf(pts[i].position)-1.0,3.0,2.0),c);
      }
    if(fitPtr->result().valid)
      {
        // the fitted line, drawn as the offset of fitted tick k from the nominal grid
        double a=fitPtr->intercept();
        double b=fitPtr->slope();
        auto fitDx=[&](double pos){double d=(a+b*std::round((pos-a)/b))-ref;return d-std::round(d/nominal)*nominal;};
        // follow the line through the first and last positions shown (no wrap: both are near the centre)
        double d0=fitDx(t0),d1=fitDx(t1);
        if(std::fabs(d0)<=maxDx && std::fabs(d1)<=maxDx)
          {
            p.setPen(QPen(QColor(255,60,60),1));
            p.drawLine(QPointF(xOf(d0),yOf(t0)),QPointF(xOf(d1),yOf(t1)));
          }
      }
    p.setPen(QColor(150,150,180));
    p.drawText(QRect(0,h-bottom,w,bottom),Qt::AlignCenter,
               tr("+/- %1 ms   |   %2 s   |   green: perfect clock   red: fit").arg(1000.0*maxDx/nominal,0,'f',0).arg(tspan/nominal,0,'f',0));
  }

private:
  const wwvTickFit *fitPtr;
};


calibrationWwv::calibrationWwv(QWidget *parent) : calibrationMethod(parent)
{
  running=false;
  hadResult=false;
  streamRate=BASESAMPLERATE;
  lastOverruns=0;
  detector=new wwvTickDetector(streamRate,1000.0);
  fit=new wwvTickFit(streamRate);

  QVBoxLayout *layout=new QVBoxLayout(this);
  QLabel *info=new QLabel(tr("Tune a receiver to WWV (2.5, 5, 10, 15 or 20 MHz) or WWVH, in AM or USB mode, and feed its audio "
                             "to the soundcard input. Press Start and let it run about 5 minutes. Press Save when the reading is stable."),this);
  info->setWordWrap(true);
  layout->addWidget(info);

  QHBoxLayout *row=new QHBoxLayout;
  row->addWidget(new QLabel(tr("Station"),this));
  stationCombo=new QComboBox(this);
  stationCombo->addItem(tr("WWV (1000 Hz ticks)"),1000.0);
  stationCombo->addItem(tr("WWVH (1200 Hz ticks)"),1200.0);
  row->addWidget(stationCombo);
  row->addStretch(1);
  startButton=new QPushButton(tr("Start"),this);
  row->addWidget(startButton);
  saveButton=new QPushButton(tr("Save"),this);
  saveButton->setToolTip(tr("Use this result and close."));
  row->addWidget(saveButton);
  layout->addLayout(row);

  statusLabel=new QLabel(this);
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
  layout->addWidget(detailLabel);

  waterfall=new wwvWaterfall(this);
  waterfall->setFit(fit);
  layout->addWidget(waterfall,1);

  applyTxCheck=new QCheckBox(tr("Also use this result for the transmit clock"),this);
  applyTxCheck->setChecked(true);
  applyTxCheck->setToolTip(tr("WWV can only be received. Most soundcards and USB interfaces run input and output from the same "
                              "clock, so the receive measurement also applies to transmit. Untick if yours does not."));
  layout->addWidget(applyTxCheck);

  timer=new QTimer(this);
  timer->setInterval(100);
  connect(timer,SIGNAL(timeout()),this,SLOT(slotTimer()));
  connect(startButton,SIGNAL(clicked()),this,SLOT(slotStartStop()));
  connect(saveButton,SIGNAL(clicked()),this,SLOT(slotSave()));
  connect(stationCombo,SIGNAL(currentIndexChanged(int)),this,SLOT(slotToneChanged()));
  connect(applyTxCheck,SIGNAL(toggled(bool)),this,SIGNAL(resultChanged()));
  updateDisplay();
}

calibrationWwv::~calibrationWwv()
{
  stop();
  delete detector;
  delete fit;
}

void calibrationWwv::slotStartStop()
{
  if(running) stop();
  else start();
}

void calibrationWwv::slotSave()
{
  if(!hasResult())
    {
      QMessageBox::information(this,tr("Calibration"),tr("No valid result yet. Keep the measurement running."));
      return;
    }
  stop();
  emit saveRequested();
}

void calibrationWwv::slotToneChanged()
{
  detector->setTone(stationCombo->currentData().toDouble());
  restartMeasurement();
}

void calibrationWwv::start()
{
  // the soundcard is needed exclusively: stop receive/transmit (the caller restarts receive afterwards)
  dispatcherPtr->idleAll();
  double sr=soundIOPtr->streamSampleRate();
  if(sr<=0) sr=BASESAMPLERATE;
  if(sr!=streamRate)
    {
      streamRate=sr;
      delete detector;
      delete fit;
      detector=new wwvTickDetector(streamRate,stationCombo->currentData().toDouble());
      fit=new wwvTickFit(streamRate);
      waterfall->setFit(fit);
    }
  restartMeasurement();
  if(!soundIOPtr->startListen())
    {
      QMessageBox::critical(this,tr("Calibration error"),tr("Soundcard not active: %1").arg(soundIOPtr->getLastError()));
      return;
    }
  lastOverruns=soundIOPtr->listenOverruns();
  running=true;
  startButton->setText(tr("Stop"));
  timer->start();
  updateDisplay();
}

void calibrationWwv::stop()
{
  if(!running) return;
  timer->stop();
  soundIOPtr->stopListen();
  running=false;
  startButton->setText(tr("Start"));
  updateDisplay();
}

void calibrationWwv::restartMeasurement()
{
  detector->reset();
  fit->reset();
  waterfall->update();
  if(hadResult)
    {
      hadResult=false;
      emit resultChanged();
    }
  updateDisplay();
}

void calibrationWwv::slotTimer()
{
  if(!running) return;
  if(soundIOPtr->listenOverruns()!=lastOverruns)
    {
      // samples were lost, so the tick positions after the gap don't line up with those before it
      lastOverruns=soundIOPtr->listenOverruns();
      restartMeasurement();
    }
  std::vector<double> block(DOWNSAMPLESIZE);
  std::vector<wwvCandidate> found;
  unsigned int avail=soundIOPtr->rawRxBuffer.count();
  while(avail>0)
    {
      unsigned int n=std::min<unsigned int>(avail,DOWNSAMPLESIZE);
      if(!soundIOPtr->rawRxBuffer.get(block.data(),n)) break;
      detector->process(block.data(),(int)n,found);
      avail-=n;
    }
  if(!found.empty())
    {
      for(size_t i=0;i<found.size();i++) fit->add(found[i]);
      fit->solve();
      bool has=hasResult();
      if(has!=hadResult) hadResult=has;
      emit resultChanged();
    }
  updateDisplay();
}

bool calibrationWwv::hasResult() const
{
  const wwvFitResult &r=fit->result();
  return r.valid && r.ticks>=GOODTICKS && std::fabs(r.ppm)<=MAXPPM;
}

double calibrationWwv::rxClockResult() const
{
  if(!hasResult()) return 0;
  return BASESAMPLERATE*(1.0+fit->result().ppm*1e-6);
}

double calibrationWwv::txClockResult() const
{
  if(!applyTxCheck->isChecked()) return 0;
  return rxClockResult();
}

void calibrationWwv::updateDisplay()
{
  const wwvFitResult &r=fit->result();
  if(!running && !r.valid)
    statusLabel->setText(tr("Press Start."));
  else if(!r.valid)
    statusLabel->setText(tr("Listening: looking for the time ticks..."));
  else if(r.ticks<GOODTICKS)
    statusLabel->setText(tr("Ticks found, collecting more..."));
  else if(std::fabs(r.ppm)>MAXPPM)
    statusLabel->setText(tr("Error larger than 0.2%: wrong station or signal? Not accepted."));
  else
    statusLabel->setText(running? tr("Press Save when the reading is stable, or keep running for more accuracy.") : tr("Stopped. Press Save to use this result."));
  if(r.valid)
    {
      rateLabel->setText(tr("%1 Hz").arg(streamRate*(1.0+r.ppm*1e-6),0,'f',2));
      ppmLabel->setText(tr("%1%2 ppm").arg(r.ppm>=0? "+":"").arg(r.ppm,0,'f',1)+(r.ppmError>0? QString(" (±%1)").arg(r.ppmError,0,'f',1):QString()));
      detailLabel->setText(tr("%1 ticks over %2 s, scatter %3 ms").arg(r.ticks).arg(r.spanSeconds,0,'f',0).arg(r.rmsMs,0,'f',2));
    }
  else
    {
      rateLabel->setText("--");
      ppmLabel->setText("--");
      detailLabel->setText(QString());
    }
  waterfall->update();
}
