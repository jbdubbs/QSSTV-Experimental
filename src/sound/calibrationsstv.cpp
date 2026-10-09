#include "calibrationsstv.h"
#include "appglobal.h"
#include "dispatcher.h"
#include "txwidget.h"
#include "rxwidget.h"
#include "soundconfig.h"

#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QCheckBox>
#include <QProgressBar>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QMessageBox>
#include <cmath>
#include <numeric>

#define LINEWIDTH 3             // pixels, of the line in the reference picture
#define MINROWS 40              // fewest rows that took part in the fit of an acceptable picture
#define MAXRMS 1.5              // pixels, largest deviation from the fitted line that is acceptable
#define MAXPPM 2000.0           // 0.2%: the same limit soundConfig applies to a stored clock

double slantToClockError(double slopePerRow,double rowsPerPeriod,double pixelsPerPeriod)
{
  if(pixelsPerPeriod<=0) return 0;
  return slopePerRow*rowsPerPeriod/pixelsPerPeriod;
}

/*!
  The received picture with the fitted line in red, so the user can see whether the reception was clean
*/
class slantPreview : public QWidget
{
public:
  explicit slantPreview(QWidget *parent=nullptr) : QWidget(parent),valid(false)
  {
    setMinimumHeight(150);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
  }
  void setPicture(const QImage &im,const slantFitResult &r)
  {
    image=im;
    fit=r;
    valid=r.valid;
    update();
  }
  void clearPicture()
  {
    image=QImage();
    valid=false;
    update();
  }

protected:
  void paintEvent(QPaintEvent *)
  {
    QPainter p(this);
    p.fillRect(rect(),QColor(10,10,40));
    if(image.isNull())
      {
        p.setPen(QColor(150,150,180));
        p.drawText(rect(),Qt::AlignCenter,tr("no picture received yet"));
        return;
      }
    QSize sz=image.size().scaled(size(),Qt::KeepAspectRatio);
    QRect r(QPoint((width()-sz.width())/2,(height()-sz.height())/2),sz);
    p.drawImage(r,image);
    if(valid)
      {
        p.setPen(QPen(QColor(255,60,60),1));
        double sx=(double)r.width()/image.width(),sy=(double)r.height()/image.height();
        double y0=0,y1=image.height()-1;
        p.drawLine(QPointF(r.left()+(fit.intercept+0.5)*sx,r.top()+(y0+0.5)*sy),
                   QPointF(r.left()+(fit.intercept+fit.slope*y1+0.5)*sx,r.top()+(y1+0.5)*sy));
      }
  }

private:
  QImage image;
  slantFitResult fit;
  bool valid;
};

calibrationSstv::calibrationSstv(QWidget *parent) : calibrationMethod(parent)
{
  sending=false;
  listening=false;
  savedAutoSlant=false;
  lastLivePaint=0;

  QVBoxLayout *layout=new QVBoxLayout(this);
  QLabel *info=new QLabel(tr("Calibrates against another SSTV station, for when neither WWV nor an internet time server is available. "
                             "The sending station must be calibrated in one of those other ways in order to be accurate. "
                             "The result is relative: it's only as good as the calibration of the other station. "
                             "One station sends a calibrated image, the other receives it and calculates its deviation from reference."),this);
  info->setWordWrap(true);
  layout->addWidget(info);

  // reference station
  QGroupBox *txBox=new QGroupBox(tr("Reference station: send the calibration picture"),this);
  QVBoxLayout *txLayout=new QVBoxLayout(txBox);
  QHBoxLayout *txRow=new QHBoxLayout;
  txRow->addWidget(new QLabel(tr("Mode"),txBox));
  modeCombo=new QComboBox(txBox);
  // 2 to 3 minute modes, the wider the better: the resolution grows with the number of pixels and lines
  static const esstvMode modes[]={PD120W,PD120,PD160,MP175,MP140,SC2_120};
  for(unsigned int i=0;i<sizeof(modes)/sizeof(modes[0]);i++)
    modeCombo->addItem(QString("%1  (%2 s)").arg(SSTVTable[modes[i]].name).arg(SSTVTable[modes[i]].imageTime,0,'f',0),(int)modes[i]);
  modeCombo->setCurrentIndex(modeCombo->findData((int)PD120));
  modeCombo->setToolTip(tr("The longer the transmission and the more pixels, the more accurate the measurement. "
                           "The receiving station needs no setting: it detects the mode."));
  txRow->addWidget(modeCombo,1);
  txRow->addWidget(new QLabel(tr("Line at"),txBox));
  positionSpin=new QSpinBox(txBox);
  positionSpin->setRange(10,90);
  positionSpin->setValue(50);
  positionSpin->setSuffix(" %");
  positionSpin->setToolTip(tr("Horizontal position of the line. Leave it in the middle: the line slants to either side."));
  txRow->addWidget(positionSpin);
  sendButton=new QPushButton(tr("Send"),txBox);
  txRow->addWidget(sendButton);
  txLayout->addLayout(txRow);
  txStatusLabel=new QLabel(txBox);
  txStatusLabel->setWordWrap(true);
  txLayout->addWidget(txStatusLabel);
  txProgress=new QProgressBar(txBox);
  txProgress->setRange(0,100);
  txProgress->setValue(0);
  txProgress->setFormat(tr("Sent: %p%"));
  txLayout->addWidget(txProgress);
  layout->addWidget(txBox);

  // station to calibrate
  QGroupBox *rxBox=new QGroupBox(tr("Station to calibrate: receive the calibration picture"),this);
  QVBoxLayout *rxLayout=new QVBoxLayout(rxBox);
  QHBoxLayout *rxRow=new QHBoxLayout;
  listenButton=new QPushButton(tr("Listen"),rxBox);
  rxRow->addWidget(listenButton);
  saveButton=new QPushButton(tr("Save"),rxBox);
  saveButton->setToolTip(tr("Use this result as the receive clock."));
  rxRow->addWidget(saveButton);
  rxStatusLabel=new QLabel(rxBox);
  rxStatusLabel->setWordWrap(true);
  rxRow->addWidget(rxStatusLabel,1);
  rxLayout->addLayout(rxRow);
  preview=new slantPreview(rxBox);
  rxLayout->addWidget(preview,1);
  QHBoxLayout *readout=new QHBoxLayout;
  ppmLabel=new QLabel("--",rxBox);
  QFont big=ppmLabel->font();
  big.setPointSize(big.pointSize()*2);
  big.setBold(true);
  ppmLabel->setFont(big);
  clockLabel=new QLabel("--",rxBox);
  clockLabel->setFont(big);
  readout->addWidget(clockLabel);
  readout->addStretch(1);
  readout->addWidget(ppmLabel);
  rxLayout->addLayout(readout);
  detailLabel=new QLabel(rxBox);
  detailLabel->setWordWrap(true);
  rxLayout->addWidget(detailLabel);
  layout->addWidget(rxBox,1);

  applyTxCheck=new QCheckBox(tr("Also use this result for the transmit clock"),this);
  applyTxCheck->setChecked(true);
  applyTxCheck->setToolTip(tr("Most soundcards and USB interfaces run input and output from the same clock, so the receive "
                              "measurement also applies to transmit. Untick if yours does not."));
  layout->addWidget(applyTxCheck);
  QLabel *note=new QLabel(tr("Every picture adds to the average. Click Save to apply the result, then listen again. "
                             "The line should be straight if the previous result was accurate."),this);
  note->setWordWrap(true);
  layout->addWidget(note);

  connect(sendButton,SIGNAL(clicked()),this,SLOT(slotSendStop()));
  connect(listenButton,SIGNAL(clicked()),this,SLOT(slotListenStop()));
  connect(saveButton,SIGNAL(clicked()),this,SLOT(slotSave()));
  connect(applyTxCheck,SIGNAL(toggled(bool)),this,SIGNAL(resultChanged()));
  connect(txWidgetPtr,SIGNAL(calibrationTxFinished()),this,SLOT(slotTxFinished()));
  connect(txWidgetPtr,SIGNAL(progressChanged(int)),this,SLOT(slotTxProgress(int)));
  updateDisplay();
}

calibrationSstv::~calibrationSstv()
{
  stop();
}

QImage calibrationSstv::makeReferenceImage(esstvMode mode,int percent) const
{
  const int w=SSTVTable[mode].numberOfPixels;
  const int h=SSTVTable[mode].numberOfDisplayLines;
  QImage im(w,h,QImage::Format_ARGB32_Premultiplied);
  im.fill(QColor(0,0,0));
  int x0=(int)std::lround(percent/100.0*w)-LINEWIDTH/2;
  x0=qBound(0,x0,w-LINEWIDTH);
  for(int y=0;y<h;y++)
    {
      QRgb *line=(QRgb *)im.scanLine(y);
      for(int x=x0;x<x0+LINEWIDTH;x++) line[x]=qRgb(255,255,255);
    }
  return im;
}

void calibrationSstv::slotSendStop()
{
  if(sending)
    {
      txWidgetPtr->abortCalibrationTx();
      return;
    }
  stopListening();
  esstvMode mode=(esstvMode)modeCombo->currentData().toInt();
  if(transmissionModeIndex!=TRXSSTV)
    {
      QMessageBox::warning(this,tr("Calibration"),tr("Switch the main window to SSTV first."));
      return;
    }
  if(!txWidgetPtr->sendCalibrationImage(mode,makeReferenceImage(mode,positionSpin->value())))
    {
      QMessageBox::warning(this,tr("Calibration"),tr("The picture cannot be sent now: the transmitter is busy or a multi picture grid is selected in the TX tab."));
      return;
    }
  sending=true;
  sendButton->setText(tr("Stop"));
  listenButton->setEnabled(false);
  modeCombo->setEnabled(false);
  positionSpin->setEnabled(false);
  updateDisplay();
}

void calibrationSstv::slotTxProgress(int percent)
{
  if(sending) txProgress->setValue(percent);
}

void calibrationSstv::slotTxFinished()
{
  if(!sending) return;
  txProgress->setValue(0);
  sending=false;
  sendButton->setText(tr("Send"));
  listenButton->setEnabled(true);
  modeCombo->setEnabled(true);
  positionSpin->setEnabled(true);
  updateDisplay();
}

void calibrationSstv::slotSave()
{
  emit saveRequested();
  rxStatusLabel->setText(tr("Saved: receive clock %1 Hz. Listening for another image to check the line.").arg(rxClockResult(),0,'f',2));
}

void calibrationSstv::slotListenStop()
{
  if(listening) stopListening();
  else startListening();
}

void calibrationSstv::startListening()
{
  if(listening || sending) return;
  clearResults();
  savedAutoSlant=autoSlantAdjust;
  autoSlantAdjust=false;
  dispatcherPtr->setCalibrationRx(true);
  qDebug() << "CALDBG startListening";
  connect(dispatcherPtr,SIGNAL(sstvImageReceived(int)),this,SLOT(slotImageReceived(int)));
  connect(dispatcherPtr,SIGNAL(sstvLineReceived()),this,SLOT(slotLineReceived()));
  dispatcherPtr->startRX();
  listening=true;
  listenButton->setText(tr("Stop"));
  sendButton->setEnabled(false);
  updateDisplay();
}

void calibrationSstv::stopListening()
{
  if(!listening) return;
  qDebug() << "CALDBG stopListening";
  disconnect(dispatcherPtr,SIGNAL(sstvImageReceived(int)),this,SLOT(slotImageReceived(int)));
  disconnect(dispatcherPtr,SIGNAL(sstvLineReceived()),this,SLOT(slotLineReceived()));
  dispatcherPtr->setCalibrationRx(false);
  dispatcherPtr->idleAll();
  autoSlantAdjust=savedAutoSlant;
  listening=false;
  listenButton->setText(tr("Listen"));
  sendButton->setEnabled(true);
  updateDisplay();
}

void calibrationSstv::stop()
{
  stopListening();
  if(sending)
    {
      txWidgetPtr->abortCalibrationTx();
      slotTxFinished();
    }
}

void calibrationSstv::clearResults()
{
  bool had=hasResult();
  clocks.clear();
  ppms.clear();
  preview->clearPicture();
  detailLabel->clear();
  saveButton->setEnabled(false);
  if(had) emit resultChanged();
}

/*!
  Paints the picture while it is being received (the dialog covers the RX tab), at most about 5 times a second
*/
void calibrationSstv::slotLineReceived()
{
  static QElapsedTimer clock;
  if(!clock.isValid()) clock.start();
  qint64 now=clock.elapsed();
  if(now-lastLivePaint<200) return;
  lastLivePaint=now;
  const QImage *im=rxWidgetPtr->getImageViewerPtr()->getDisplayedImage();
  if(im->isNull()) return;
  preview->setPicture(*im,slantFitResult());
  rxStatusLabel->setText(tr("Receiving the picture..."));
}

void calibrationSstv::slotImageReceived(int m)
{
  esstvMode mode=(esstvMode)m;
  qDebug() << "CALDBG slotImageReceived mode=" << m;
  if(mode<0 || mode>=NUMSSTVMODES)
    {
      rxStatusLabel->setText(tr("Picture incomplete (too few lines): waiting for the next one."));
      return;
    }
  // the line period and the pixel duration are in the same unit (samples at the subsampled clock)
  const double clk=rxClock/SUBSAMPLINGFACTOR;
  const double pixDur=rxSSTVParam.pixelDuration;
  if(pixDur<=0)
    {
      rxStatusLabel->setText(tr("Mode %1 cannot be used for calibration.").arg(SSTVTable[mode].name));
      return;
    }
  const double pixelsPerPeriod=getLineLength(mode,clk)/pixDur;
  const double rowsPerPeriod=(double)SSTVTable[mode].numberOfDisplayLines/SSTVTable[mode].numberOfDataLines;

  // the received picture is the displayed one; getImagePtr() is the (stale) source image
  QImage img=*rxWidgetPtr->getImageViewerPtr()->getDisplayedImage();
  QImage gray=img.convertToFormat(QImage::Format_Grayscale8);
  slantFitResult r=fitSlant(gray.constBits(),gray.width(),gray.height(),gray.bytesPerLine());
  preview->setPicture(img,r);
  if(!r.valid || r.used<MINROWS || r.rms>MAXRMS)
    {
      rxStatusLabel->setText(tr("%1 received, but no straight line could be measured (%2 of %3 rows). Waiting for the next one.")
                             .arg(SSTVTable[mode].name).arg(r.used).arg(r.total));
      return;
    }
  double err=slantToClockError(r.slope,rowsPerPeriod,pixelsPerPeriod);
  double ppm=err*1e6;
  if(std::fabs(ppm)>MAXPPM)
    {
      rxStatusLabel->setText(tr("%1 received, but the slant (%2 ppm) is too large to be a sample rate difference. Is this the calibration picture?")
                             .arg(SSTVTable[mode].name).arg(ppm,0,'f',0));
      return;
    }
  ppms.push_back(ppm);
  clocks.push_back(rxClock*(1.0+err));
  rxStatusLabel->setText(tr("%1 received (%2 pictures so far). Listening for more.").arg(SSTVTable[mode].name).arg(ppms.size()));
  detailLabel->setText(tr("Last picture: %1 ppm, line fitted on %2 of %3 rows, deviation %4 pixels.")
                       .arg(ppm,0,'f',1).arg(r.used).arg(r.total).arg(r.rms,0,'f',2));
  updateDisplay();
  emit resultChanged();
}

bool calibrationSstv::hasResult() const
{
  return !clocks.empty();
}

double calibrationSstv::rxClockResult() const
{
  if(clocks.empty()) return 0;
  return std::accumulate(clocks.begin(),clocks.end(),0.0)/clocks.size();
}

double calibrationSstv::txClockResult() const
{
  if(!applyTxCheck->isChecked()) return 0;
  return rxClockResult();
}

void calibrationSstv::updateDisplay()
{
  saveButton->setEnabled(hasResult());
  if(sending)
    {
      esstvMode mode=(esstvMode)modeCombo->currentData().toInt();
      txStatusLabel->setText(tr("Sending the calibration picture in %1: about %2 minutes. Press Stop to abort.")
                             .arg(SSTVTable[mode].name).arg(SSTVTable[mode].imageTime/60.0,0,'f',1));
    }
  else
    txStatusLabel->setText(tr("Sends a black picture with one straight white line. Your own TX picture is put back afterwards."));
  if(listening)
    {
      if(ppms.empty()) rxStatusLabel->setText(tr("Listening: waiting for the reference picture."));
    }
  else if(ppms.empty()) rxStatusLabel->setText(tr("Press Listen, then have the reference station send its picture."));
  if(ppms.empty())
    {
      ppmLabel->setText("--");
      clockLabel->setText("--");
      return;
    }
  double mean=std::accumulate(ppms.begin(),ppms.end(),0.0)/ppms.size();
  ppmLabel->setText(tr("%1 ppm").arg(mean,0,'f',1));
  clockLabel->setText(tr("%1 Hz").arg(rxClockResult(),0,'f',2));
}
