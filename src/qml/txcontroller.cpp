#include "txcontroller.h"
#include "appglobal.h"
#include "appmodel.h"
#include "imageviewer.h"
#include "operatorconfig.h"
#include "sstvparam.h"
#include "txfunctions.h"
#include "txwidget.h"
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
  refreshPicture();
}

void TxController::refreshPicture()
{
  if(!txWidgetPtr) return;
  imageViewer *v=txWidgetPtr->getImageViewerPtr();
  const QImage *im=v->hasValidImage() ? v->getDisplayedImage() : nullptr;
  m_picture=(im && !im->isNull()) ? *im : QImage();
  ++m_pictureRevision;
  emit pictureChanged();
}

int TxController::mode() const {return int(sstvModeIndexTx);}

void TxController::setMode(int m)
{
  if(m<0 || m>=NUMSSTVMODES || !txWidgetPtr) return;
  // the page's own combo box: it does what a choice there does (the grid control, the template, the saving)
  if(QComboBox *box=txWidgetPtr->findChild<QComboBox *>(QStringLiteral("sstvModeComboBox"))) box->setCurrentIndex(m);
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

void TxController::useTemplateFile(const QString &file)
{
  if(!txWidgetPtr) return;
  if(QComboBox *b=txWidgetPtr->findChild<QComboBox *>(QStringLiteral("templatesComboBox")))
    {
      const QString name=QFileInfo(file).completeBaseName();
      for(int i=0;i<b->count();i++)
        if(b->itemText(i)==name) {b->setCurrentIndex(i); break;}
    }
  setUseTemplate(true);
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
  if(m_test) return;
  const bool sending=txWidgetPtr->functionsPtr() && txWidgetPtr->functionsPtr()->txBusy();
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
}
