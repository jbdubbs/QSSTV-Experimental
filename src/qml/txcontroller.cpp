#include "txcontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "imageviewer.h"
#include "operatorconfig.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "txwidget.h"
#include "gallerywidget.h"
#include "thumbnailprovider.h"
#include "dispatcher.h"
#include "waterfalltext.h"
#ifdef Q_OS_ANDROID
#include "photopicker.h"
#endif

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QJSEngine>
#include <QLineEdit>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QPermission>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QSettings>
#include <QSlider>

TxController *TxController::instance()
{
  static TxController *c=new TxController;
  return c;
}

TxController *TxController::create(QQmlEngine *,QJSEngine *)
{
  TxController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

TxController::TxController(QObject *parent) : QObject(parent)
{
  m_timer.setInterval(200);
  connect(&m_timer,&QTimer::timeout,this,&TxController::poll);
  m_timer.start();
  connectToPage();
}

void TxController::connectToPage()
{
  if(m_connected || !txWidgetPtr) return;
  m_connected=true;
  connect(txWidgetPtr,&txWidget::progressChanged,this,&TxController::progressTick);
  connect(txWidgetPtr,&txWidget::txTimeChanged,this,[this]{refreshPicture(); emit timeChanged();});
  connect(txWidgetPtr->getImageViewerPtr(),&imageViewer::imageChanged,this,&TxController::refreshPicture);
  connect(txWidgetPtr,&txWidget::calibrationTxFailed,this,&TxController::sampleFailed);
  refreshPicture();
}

void TxController::refreshPicture()
{
  if(!txWidgetPtr) return;
  if(m_framingGesture && transmissionModeIndex==TRXDRM) return;   // the preview of a digital picture stays until the gesture is over
  imageViewer *v=txWidgetPtr->getImageViewerPtr();
  const QImage *im=v->hasValidImage() ? v->getDisplayedImage() : nullptr;
  m_picture=(im && !im->isNull()) ? *im : QImage();
  ++m_pictureRevision;
  emit pictureChanged();
}

// How the picture is fitted to the frame (#177): the page's Framing box, and the zoom and pan of Free, are the imageViewer's
static imageViewer *viewer()
{
  return txWidgetPtr ? txWidgetPtr->getImageViewerPtr() : nullptr;
}

int TxController::framing() const
{
  const imageViewer *v=viewer();
  return v ? v->activeFit() : 0;
}

void TxController::setFraming(int mode)
{
  if(!txWidgetPtr || m_sending || mode<0 || mode>3 || !framingChoice()) return;
  // the page's own box and what a choice there does (both boxes, the viewer, the template, the saving): the box emits no activated()
  // for a change made in code, so the page's slot is run here, as for the mode (#170)
  QMetaObject::invokeMethod(txWidgetPtr,"slotResizeChanged",Q_ARG(int,mode));   // draws the picture again and tells (txTimeChanged)
  poll();
}

bool TxController::framingChoice() const
{
  const imageViewer *v=viewer();
  return v && !v->drmSingleImage();   // a single DRM picture is always Free: the page disables the box
}

bool TxController::framingFree() const
{
  const imageViewer *v=viewer();
  return v && v->freeFraming();
}

bool TxController::framingMoved() const
{
  const imageViewer *v=viewer();
  return v && v->freeFraming() && v->framingMoved();
}

void TxController::beginFraming()
{
  imageViewer *v=viewer();
  if(!v || m_sending || !v->freeFraming()) return;
  v->beginFreeGesture();
  m_framingGesture=true;
}

bool TxController::moveFraming(double scale,double focusX,double focusY,double shiftX,double shiftY)
{
  imageViewer *v=viewer();
  if(!v || !m_framingGesture || !v->setFreeGesture(scale,QPointF(focusX,focusY),QPointF(shiftX,shiftY))) return false;
  if(transmissionModeIndex==TRXDRM)
    {
      // a digital picture is encoded once, at the end: the steps show a smaller one drawn from the whole picture
      const QImage preview=v->freePreview(640);
      if(preview.isNull()) return false;
      m_picture=preview;
      ++m_pictureRevision;
      emit pictureChanged();
    }
  else refreshPicture();
  return true;
}

void TxController::endFraming()
{
  imageViewer *v=viewer();
  if(!m_framingGesture) return;
  m_framingGesture=false;
  if(v) v->endFreeGesture();   // the full size picture (a digital one is encoded now and comes back through imageChanged)
  if(v && transmissionModeIndex!=TRXDRM) refreshPicture();
  poll();
}

bool TxController::adjustFraming(double scale,double focusX,double focusY,double shiftX,double shiftY)
{
  beginFraming();
  if(!m_framingGesture) return false;
  const bool changed=moveFraming(scale,focusX,focusY,shiftX,shiftY);
  endFraming();
  return changed;
}

void TxController::resetFraming()
{
  imageViewer *v=viewer();
  if(!v || m_sending || !v->freeFraming()) return;
  v->resetFreeFraming();
  if(transmissionModeIndex!=TRXDRM) refreshPicture();
  poll();
}

int TxController::mode() const {return int(sstvModeIndexTx);}

void TxController::setMode(int m)
{
  if(m<0 || m>=NUMSSTVMODES || !txWidgetPtr) return;
  // the page's own combo box, then what a choice there does (the grid control, the template, the time, the saving): the box emits no
  // activated() for a change made in code, so the page's slot is run here, as for the DRM profile (#170)
  if(QComboBox *box=txWidgetPtr->findChild<QComboBox *>(QStringLiteral("sstvModeComboBox"))) box->setCurrentIndex(m);
  QMetaObject::invokeMethod(txWidgetPtr,"slotModeChanged",Q_ARG(int,m));
  m_cachedMode=int(sstvModeIndexTx);
  emit modeChanged();
  emit timeChanged();
}

QString TxController::modeName() const
{
  return SSTVTable[int(sstvModeIndexTx)].name;
}

QString TxController::pictureSize() const
{
  const sSSTVParam &p=SSTVTable[int(sstvModeIndexTx)];
  return QString("%1 x %2").arg(p.numberOfPixels).arg(p.numberOfDisplayLines);
}

QVariantList TxController::favorites() const
{
  // Scottie 1, Martin 1, PD120 and Robot 36 (the user's choice at the design review); the other modes are under "All modes"
  QVariantList l;
  for(esstvMode m:{S1,M1,PD120,R36})
    {
      QVariantMap e;
      e["mode"]=int(m);
      e["name"]=SSTVTable[int(m)].name;
      l.append(e);
    }
  return l;
}

static QString timeString(int seconds)
{
  if(seconds<=0) return QString();
  const int minutes=seconds/60;
  seconds-=minutes*60;
  if(minutes==0) return QObject::tr("%1 s").arg(seconds);
  return QObject::tr("%1 min %2 s").arg(minutes).arg(seconds);
}

QString TxController::timeText() const
{
  return txWidgetPtr ? timeString(txWidgetPtr->txSeconds()) : QString();
}

QVariantList TxController::allModes() const
{
  QVariantList l;
  for(int i=0;i<NUMSSTVMODES;i++)
    {
      const sSSTVParam &p=SSTVTable[i];
      if(p.name.isEmpty()) continue;
      const int seconds=int(p.imageTime+0.5);
      QVariantMap e;
      e["mode"]=i;
      e["name"]=p.name;
      e["size"]=QString("%1 x %2").arg(p.numberOfPixels).arg(p.numberOfDisplayLines);
      e["time"]=timeString(seconds);
      e["seconds"]=seconds;
      e["group"]=seconds<=75 ? tr("Quick") : seconds<=150 ? tr("Standard") : tr("Long");
      l.append(e);
    }
  // quick ones first, each group by length
  std::stable_sort(l.begin(),l.end(),[](const QVariant &a,const QVariant &b)
    {
      return a.toMap().value("seconds").toInt()<b.toMap().value("seconds").toInt();
    });
  return l;
}

QString TxController::remainingText() const
{
  const int total=m_toolName.isEmpty() ? (txWidgetPtr ? txWidgetPtr->txSeconds() : 0) : m_toolSeconds;
  if(total<=0) return QString();
  const int left=qMax(0,int(total*(1.0-m_progress)+0.5));
  return QString("%1:%2").arg(left/60).arg(left%60,2,10,QLatin1Char('0'));
}

QString TxController::totalText() const
{
  return m_toolName.isEmpty() ? timeText() : timeString(m_toolSeconds);
}

void TxController::startTool(const QString &name,int seconds)
{
  m_toolName=name;
  m_toolSeconds=seconds;
  emit toolChanged();
  emit progressChanged();
}

bool TxController::useTemplateOn() const {return ::useTemplate;}

void TxController::setUseTemplate(bool on)
{
  if(!txWidgetPtr) return;
  if(QCheckBox *box=txWidgetPtr->findChild<QCheckBox *>(QStringLiteral("templateCheckBox"))) box->setChecked(on);
  emit detailsChanged();
}

static QLineEdit *lineEdit(const char *name)
{
  return txWidgetPtr ? txWidgetPtr->findChild<QLineEdit *>(QString::fromLatin1(name)) : nullptr;
}

static void setLine(const char *name,const QString &s)
{
  if(QLineEdit *e=lineEdit(name))
    {
      e->setText(s);
      emit e->editingFinished();   // the page takes the text over when editing finishes
    }
}

QString TxController::toCall() const {QLineEdit *e=lineEdit("toCallLineEdit"); return e ? e->text() : QString();}
void TxController::setToCall(const QString &s) {setLine("toCallLineEdit",s); emit detailsChanged();}
QString TxController::toOperator() const {QLineEdit *e=lineEdit("operatorLineEdit"); return e ? e->text() : QString();}
void TxController::setToOperator(const QString &s) {setLine("operatorLineEdit",s); emit detailsChanged();}
QString TxController::report() const {QLineEdit *e=lineEdit("rsvLineEdit"); return e ? e->text() : QString();}
void TxController::setReport(const QString &s) {setLine("rsvLineEdit",s); emit detailsChanged();}

QString TxController::detailsSummary() const
{
  QStringList parts;
  if(!myCallsign.isEmpty()) parts<<myCallsign;
  if(!toCall().isEmpty()) parts<<tr("To %1").arg(toCall());
  if(!report().isEmpty()) parts<<report();
  return parts.join(QStringLiteral(" · "));
}

// the digital settings are the page's own boxes and slider: what a user does there is what is done here
static QComboBox *box(const char *name)
{
  return txWidgetPtr ? txWidgetPtr->findChild<QComboBox *>(QString::fromLatin1(name)) : nullptr;
}

static QSlider *sizeSlider()
{
  return txWidgetPtr ? txWidgetPtr->findChild<QSlider *>(QStringLiteral("sizeSlider")) : nullptr;
}

QStringList TxController::drmProfiles() const
{
  QStringList l;
  if(QComboBox *b=box("drmProfileComboBox")) for(int i=0;i<b->count();i++) l<<b->itemText(i);
  return l;
}

int TxController::drmProfile() const
{
  QComboBox *b=box("drmProfileComboBox");
  return b ? b->currentIndex() : -1;
}

void TxController::setDrmProfile(int index)
{
  QComboBox *b=box("drmProfileComboBox");
  if(!b || index<0 || index>=b->count()) return;
  b->setCurrentIndex(index);
  QMetaObject::invokeMethod(txWidgetPtr,"slotProfileChanged",Q_ARG(int,index));   // what choosing it in the box does
  poll();
}

int TxController::drmSize() const {QSlider *s=sizeSlider(); return s ? s->value() : 0;}
int TxController::drmSizeMin() const {QSlider *s=sizeSlider(); return s ? s->minimum() : 0;}
int TxController::drmSizeMax() const {QSlider *s=sizeSlider(); return s ? s->maximum() : 0;}

void TxController::setDrmSize(int bytes)
{
  if(QSlider *s=sizeSlider()) s->setValue(qBound(s->minimum(),bytes,s->maximum()));   // the page re-encodes in the background
  poll();
}

QString TxController::drmSummary() const
{
  auto text=[](const char *name){QComboBox *b=box(name); return b ? b->currentText() : QString();};
  const QString qam=text("drmTxQAMComboBox");
  return tr("Mode %1, %2-QAM, %3").arg(text("drmTxModeComboBox"),qam,text("drmTxBandwidthComboBox"));
}

namespace
{
struct drmOption {const char *key; const char *title; const char *box;};
const drmOption drmOptionTable[]={
  {"mode","Robustness","drmTxModeComboBox"},
  {"qam","QAM","drmTxQAMComboBox"},
  {"bandwidth","Bandwidth","drmTxBandwidthComboBox"},
  {"interleave","Interleaving","drmTxInterleaveComboBox"},
  {"protection","Error protection","drmTxProtectionComboBox"},
  {"rs","Reed-Solomon (files)","drmTxReedSolomonComboBox"},
  {"encoding","Picture format","drmEncodingComboBox"},
};
}

QVariantList TxController::drmOptions() const
{
  QVariantList l;
  for(const drmOption &o:drmOptionTable)
    {
      QComboBox *b=box(o.box);
      if(!b) continue;
      QStringList items;
      for(int i=0;i<b->count();i++) items<<b->itemText(i);
      QVariantMap e;
      e["key"]=o.key;
      e["title"]=tr(o.title);
      e["items"]=items;
      e["index"]=b->currentIndex();
      l.append(e);
    }
  return l;
}

void TxController::setDrmOption(const QString &key,int index)
{
  for(const drmOption &o:drmOptionTable)
    {
      if(key!=QLatin1String(o.key)) continue;
      QComboBox *b=box(o.box);
      if(!b || index<0 || index>=b->count()) return;
      b->setCurrentIndex(index);
      if(key==QLatin1String("encoding")) QMetaObject::invokeMethod(txWidgetPtr,"slotEncodingChanged",Q_ARG(int,index));
      else QMetaObject::invokeMethod(txWidgetPtr,"slotGetTXParams");   // takes the boxes over and updates the time
      poll();
      return;
    }
}

void TxController::pickPhoto()
{
#ifdef Q_OS_ANDROID
  androidPickPhoto();   // the system's photo picker
#else
  pickFile();
#endif
}

void TxController::testPattern(int which)
{
  if(!txWidgetPtr) return;
  const etpSelect sel=which==1 ? TPPM5544 : which==2 ? TPINDIANHEAD : TPSMPTE;
  txWidgetPtr->txTestPattern(sel);
}

void TxController::sendWaterfallId()
{
  startTool(tr("Waterfall call sign"),0);
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"sendWFID",Qt::QueuedConnection);
}

void TxController::sendWaterfallText(const QString &text)
{
  if(text.isEmpty() || !waterfallPtr) return;
  waterfallPtr->setText(text);
  startTool(tr("Waterfall text"),0);
  dispatcherPtr->startTX(txFunctions::TXSENDWFID);
}

void TxController::sendCwId()
{
  startTool(tr("CW call sign"),0);
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"sendCWID",Qt::QueuedConnection);
}

QVariantMap TxController::toolDefaults() const
{
  QSettings s;
  s.beginGroup("TX");
  QVariantMap m;
  m["tone"]=s.value("toneFreq",1000).toInt();
  m["duration"]=s.value("toneDuration",5).toInt();
  m["low"]=s.value("sweepLowerFreq",300).toInt();
  m["high"]=s.value("sweepUpperFreq",3000).toInt();
  m["sweepDuration"]=s.value("sweepDuration",10).toInt();
  s.endGroup();
  return m;
}

void TxController::sendTone(int frequency,int seconds)
{
  QSettings s;
  s.beginGroup("TX");
  s.setValue("toneFreq",frequency);
  s.setValue("toneDuration",seconds);
  s.endGroup();
  txWidgetPtr->functionsPtr()->setToneParam(double(seconds),double(frequency));
  startTool(tr("Test tone, %1 Hz").arg(frequency),seconds);
  dispatcherPtr->startTX(txFunctions::TXSENDTONE);
}

void TxController::sendSweep(int lowerFrequency,int upperFrequency,int seconds)
{
  QSettings s;
  s.beginGroup("TX");
  s.setValue("sweepLowerFreq",lowerFrequency);
  s.setValue("sweepUpperFreq",upperFrequency);
  s.setValue("sweepDuration",seconds);
  s.endGroup();
  txWidgetPtr->functionsPtr()->setToneParam(double(seconds),double(lowerFrequency),double(upperFrequency));
  startTool(tr("Frequency sweep, %1 to %2 Hz").arg(lowerFrequency).arg(upperFrequency),seconds);
  dispatcherPtr->startTX(txFunctions::TXSENDTONE);
}

// The picture of the test sound (#185). B/W 8 is the shortest mode and carries gray only: eight steps over a ramp, so that a sound which is
// distorted or clipped shows in the steps and a weak or noisy one in the ramp.
static QImage sampleImage()
{
  const int w=SSTVTable[BW8].numberOfPixels;
  const int h=SSTVTable[BW8].numberOfDisplayLines;
  QImage im(w,h,QImage::Format_ARGB32_Premultiplied);
  for(int y=0;y<h;y++)
    {
      QRgb *line=reinterpret_cast<QRgb *>(im.scanLine(y));
      for(int x=0;x<w;x++)
        {
          const int g=y<h/2 ? (x*8/w)*255/7 : x*255/(w-1);
          line[x]=qRgb(g,g,g);
        }
    }
  return im;
}

/*!
  Settings > Audio and radio > Test sound: the picture goes through the normal SSTV send path (the way the Calibration screen sends its reference
  picture), so the VIS header, the picture and the call sign ID sound as a real transmission does. txWidget puts the user's picture, mode and
  framing back when it is over or stopped. SSTV cannot be sent from the digital page: that is switched to analog for the time, and poll() switches
  it back once the transmitter is idle again (endSample()).
*/
void TxController::sendSampleImage()
{
  if(!txWidgetPtr || m_sending || m_sample) return;
  AppModel *app=AppModel::instance();
  const bool wasDigital=app->digital();
  if(wasDigital)
    {
      app->setDigital(false);
      if(app->digital())
        {
          emit app->noticeShown(tr("The test picture cannot be sent now: a picture is being received or sent."));
          return;
        }
    }
  if(!txWidgetPtr->sendCalibrationImage(BW8,sampleImage()))
    {
      if(wasDigital) app->setDigital(true);
      emit app->noticeShown(tr("The test picture cannot be sent now: a picture is being received or sent."));
      return;
    }
  m_sample=true;
  m_sampleWasDigital=wasDigital;
  // what the page says it takes, with the header and the call sign ID (sendCalibrationImage() has worked it out for B/W 8 by now)
  const int seconds=txWidgetPtr->txSeconds();
  startTool(tr("Test picture, %1").arg(SSTVTable[BW8].name),seconds>0 ? seconds : int(SSTVTable[BW8].imageTime+0.5)+4);
  poll();   // the overlay now, not at the next tick
}

// the test picture could not be started or went wrong (no sound output, ...); the page is unlocked and restored by txWidget
void TxController::sampleFailed(const QString &why)
{
  if(!m_sample) return;
  QString reason=why.trimmed();
  while(reason.endsWith('.')) reason.chop(1);
  emit AppModel::instance()->problemShown(tr("Test sound"),tr("The test picture was not sent: %1.\n\nCheck the speaker in Settings > Audio and radio.").arg(reason));
}

// the transmitter is idle again: the digital page, if that is where the test began, is switched back
void TxController::endSample()
{
  m_sample=false;
  if(!m_sampleWasDigital) return;
  m_sampleWasDigital=false;
  AppModel::instance()->setDigital(true);
}


void TxController::pickFile()
{
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"slotFileOpen",Qt::QueuedConnection);
}

bool TxController::cameraAvailable() const
{
  return !QMediaDevices::videoInputs().isEmpty();
}

void TxController::setCameraOpen(bool open)
{
  if(open==m_cameraOpen) return;
  m_cameraOpen=open;
  emit cameraOpenChanged();
}

void TxController::camera()
{
  // asked in context: when the user taps Camera, with the system's own question
  QCameraPermission permission;
  switch(qApp->checkPermission(permission))
    {
    case Qt::PermissionStatus::Granted:
      setCameraOpen(true);
      break;
    case Qt::PermissionStatus::Undetermined:
      qApp->requestPermission(permission,this,[this](const QPermission &answer)
        {
          if(answer.status()==Qt::PermissionStatus::Granted) setCameraOpen(true);
          else emit cameraBlocked();
        });
      break;
    case Qt::PermissionStatus::Denied:
      emit cameraBlocked();
      break;
    }
}

QString TxController::tempPicturePath() const
{
  const QString dir=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QDir().mkpath(dir);
  return dir+QStringLiteral("/camera.jpg");
}

void TxController::usePicture(const QString &file)
{
  setCameraOpen(false);
  showPicture(file);
}

void TxController::showPicture(const QString &file)
{
  if(!txWidgetPtr) return;
  txWidgetPtr->setImage(file);
  AppModel::instance()->setScreen(1);
}

void TxController::showPictures(const QStringList &files)
{
  if(!txWidgetPtr || files.isEmpty()) return;
  txWidgetPtr->setImages(files);
  AppModel::instance()->setScreen(1);
}

static QComboBox *templateBox()
{
  return txWidgetPtr ? txWidgetPtr->findChild<QComboBox *>(QStringLiteral("templatesComboBox")) : nullptr;
}

QString TxController::templateName() const
{
  const QComboBox *b=templateBox();
  return b ? b->currentText() : QString();
}

bool TxController::templateOn() const
{
  return ::useTemplate && !templateName().isEmpty();   // a saved "on" with no template left to use is not a template on the picture
}

QString TxController::templateFile() const
{
  const QComboBox *b=templateBox();
  return (b && galleryWidgetPtr && b->currentIndex()>=0) ? galleryWidgetPtr->getTemplateFileName(b->currentIndex()) : QString();
}

QVariantList TxController::templates() const
{
  QVariantList l;
  if(!galleryWidgetPtr) return l;
  const QString current=templateFile();
  const int n=galleryWidgetPtr->getFilenames().count();
  for(int i=0;i<n;i++)
    {
      const QString path=galleryWidgetPtr->getTemplateFileName(i);
      if(path.isEmpty()) continue;
      QVariantMap m;
      m["name"]=QFileInfo(path).completeBaseName();
      m["path"]=path;
      m["thumb"]=QStringLiteral("image://thumb/")+ThumbnailProvider::idFor(path);
      m["current"]=(path==current);
      l.append(m);
    }
  return l;
}

void TxController::chooseTemplate(const QString &file)
{
  if(!txWidgetPtr) return;
  if(QComboBox *b=templateBox())
    {
      const QString name=QFileInfo(file).completeBaseName();
      for(int i=0;i<b->count();i++)
        if(b->itemText(i)==name) {b->setCurrentIndex(i); break;}
    }
  setUseTemplate(true);   // the choice of a template is the wish to have one on the picture
}

void TxController::useTemplateFile(const QString &file)
{
  chooseTemplate(file);
  AppModel::instance()->setScreen(1);
}

void TxController::send()
{
  if(txWidgetPtr && hasPicture()) QMetaObject::invokeMethod(txWidgetPtr,"slotStart",Qt::QueuedConnection);
}

void TxController::stop()
{
  if(txWidgetPtr) QMetaObject::invokeMethod(txWidgetPtr,"slotStop",Qt::QueuedConnection);
}

void TxController::progressTick(int percent)
{
  if(m_test) return;
  m_progress=qBound(0.0,percent/100.0,1.0);
  emit progressChanged();
}

void TxController::setTestSending(qreal progress,const QString &tool,int toolSeconds)
{
  m_toolName=tool;
  m_toolSeconds=toolSeconds;
  emit toolChanged();
  m_test=progress>=0;
  m_sending=m_test;
  m_progress=m_test ? progress : 0;
  emit sendingChanged();
  emit progressChanged();
}

void TxController::poll()
{
  connectToPage();
  if(!txWidgetPtr) return;
  if(m_cachedMode!=int(sstvModeIndexTx))
    {
      m_cachedMode=int(sstvModeIndexTx);
      emit modeChanged();
    }
  QString sig=QString("%1|%2|%3|%4|%5").arg(drmProfile()).arg(drmSize()).arg(drmSizeMax()).arg(drmSummary()).arg(txWidgetPtr->txSeconds());
  for(const QVariant &o:drmOptions()) sig+="|"+QString::number(o.toMap().value("index").toInt());
  if(sig!=m_drmSignature)
    {
      m_drmSignature=sig;
      ++m_drmRevision;
      emit drmChanged();
    }
  const QString framingSig=QString("%1|%2|%3|%4").arg(framing()).arg(framingChoice()).arg(framingFree()).arg(framingMoved());
  if(framingSig!=m_framingSignature)
    {
      m_framingSignature=framingSig;
      emit framingChanged();   // from the page itself, from the digital switch, a new picture, a reset
    }
  if(m_test) return;
  // the test picture counts as sending from the call until the page is restored: between its preparation and the start of the transmitter
  // there is a moment with an idle thread, which would end the overlay and clear the name of the tool
  const bool sending=(txWidgetPtr->functionsPtr() && txWidgetPtr->functionsPtr()->txBusy()) || (m_sample && txWidgetPtr->calibrationTxActive());
  if(sending!=m_sending)
    {
      m_sending=sending;
      if(!sending)
        {
          m_progress=0;
          if(!m_toolName.isEmpty()) {m_toolName.clear(); m_toolSeconds=0; emit toolChanged();}
          emit progressChanged();
        }
      emit sendingChanged();
    }
  if(m_sample && !sending) endSample();
}
