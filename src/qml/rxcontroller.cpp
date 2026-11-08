#include "rxcontroller.h"
#include "appglobal.h"
#include "configparams.h"
#include "dispatcher.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "rxfunctions.h"
#include "rxwidget.h"
#include "soundbase.h"
#include "sstvrx.h"

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QGuiApplication>
#include <QJSEngine>
#include <QQmlEngine>

RxController *RxController::instance()
{
  static RxController *c=new RxController;
  return c;
}

RxController *RxController::create(QQmlEngine *,QJSEngine *)
{
  RxController *c=instance();
  QJSEngine::setObjectOwnership(c,QJSEngine::CppOwnership);
  return c;
}

RxController::RxController(QObject *parent) : QObject(parent)
{
  m_timer.setInterval(100);
  connect(&m_timer,&QTimer::timeout,this,&RxController::poll);
  m_timer.start();
  // nothing is looked at while the app is out of sight (battery); the receiver itself goes on
  connect(qGuiApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState s)
    {
      if(s==Qt::ApplicationActive)
        {
          if(!m_timer.isActive()) m_timer.start();
          poll();
        }
      else m_timer.stop();
    });
}

QString RxController::levelWord() const
{
  if(!m_listening) return QString();
  if(m_level<0.25) return tr("Too quiet");
  if(m_level<zoneStart()) return tr("Low");
  if(m_level>0.95) return tr("Too loud");
  if(m_level>zoneEnd()) return tr("Loud");
  return tr("Good");
}

QString RxController::signalText() const
{
  if(!m_receiving) return QString();
  if(m_sync>=0.6) return tr("Signal good");
  if(m_sync>=0.3) return tr("Signal fair");
  return tr("Signal weak");
}

// The options are the Receive page's own check boxes (they hold the saving of the settings): a click on them is what a user does there
static void clickBox(const char *name,bool wanted,bool current)
{
  if(wanted==current || !rxWidgetPtr) return;
  if(QCheckBox *box=rxWidgetPtr->findChild<QCheckBox *>(QString::fromLatin1(name))) box->click();
}

bool RxController::autoSave() const {return ::autoSave;}
bool RxController::autoSlant() const {return ::autoSlantAdjust;}

void RxController::setAutoSave(bool on)
{
  clickBox("autoSaveCheckBox",on,::autoSave);
  emit optionsChanged();
}

void RxController::setAutoSlant(bool on)
{
  clickBox("autoSlantAdjustCheckBox",on,::autoSlantAdjust);
  emit optionsChanged();
}

int RxController::sensitivity() const {return ::sensitivity;}
int RxController::minCompletion() const {return ::minCompletion;}

void RxController::setSensitivity(int s)
{
  if(rxWidgetPtr && s>=0 && s<=3)
    if(QComboBox *box=rxWidgetPtr->findChild<QComboBox *>("sensitivityComboBox")) box->setCurrentIndex(s);   // the page saves it
  emit optionsChanged();
}

void RxController::setMinCompletion(int percent)
{
  if(rxWidgetPtr)
    if(QSpinBox *box=rxWidgetPtr->findChild<QSpinBox *>("completeSpinBox")) box->setValue(qBound(0,percent,100));
  emit optionsChanged();
}

void RxController::start()
{
  if(rxWidgetPtr) rxWidgetPtr->startRX(true);
  poll();
}

void RxController::stop()
{
  if(rxWidgetPtr) rxWidgetPtr->startRX(false);
  poll();
}

void RxController::toggle()
{
  if(m_listening) stop(); else start();
}

void RxController::decodeRecording()
{
  if(rxWidgetPtr) QMetaObject::invokeMethod(rxWidgetPtr,"slotDecodeFile",Qt::QueuedConnection);
}

void RxController::clearPicture()
{
  if(rxWidgetPtr) rxWidgetPtr->clearImage();
  m_picture=QImage();
  m_line=m_lines=0;
  ++m_pictureRevision;
  emit pictureChanged();
  emit progressChanged();
}

void RxController::takePicture()
{
  // a copy: the receiver thread writes into the viewer's image through raw line pointers, and the viewer itself may be replaced
  if(!rxWidgetPtr) return;
  const QImage *im=rxWidgetPtr->getImageViewerPtr()->getDisplayedImage();
  m_picture=(im && !im->isNull()) ? im->copy() : QImage();
  ++m_pictureRevision;
  emit pictureChanged();
}

void RxController::imageStarted(int width,int height)
{
  Q_UNUSED(width);
  m_lines=height;
  m_line=0;
  takePicture();
  emit progressChanged();
  poll();
}

void RxController::lineDrawn(int line)
{
  m_line=line;
  takePicture();
  emit progressChanged();
}

void RxController::imageEnded()
{
  takePicture();
  poll();
}

void RxController::imageSaved(const QString &file)
{
  emit pictureSaved(file);
}

void RxController::imageShown()
{
  m_line=m_lines=0;
  takePicture();
  emit progressChanged();
}

void RxController::statusReceived(bool drm,const QString &text)
{
  Q_UNUSED(drm);
  if(text==m_status) return;
  m_status=text;
  m_mode=text.startsWith(QLatin1String("Receiving ")) ? text.mid(10).trimmed() : QString();
  emit statusChanged();
}

void RxController::fftInit(int fftLength,int samplingRate)
{
  m_waterfall.init(fftLength,samplingRate);
}

void RxController::fftReceived(const double *fft)
{
  m_waterfall.add(fft);
  ++m_waterfallRevision;
  emit waterfallChanged();
}

void RxController::syncReceived(int quality)
{
  const qreal sync=qBound(0.0,quality/25.0,1.0);
  if(qFuzzyCompare(sync+1,m_sync+1)) return;
  m_sync=sync;
  emit meterChanged();
}

void RxController::poll()
{
  if(!rxWidgetPtr) return;
  const rxFunctions *f=rxWidgetPtr->functionsPtr();
  const bool listening=m_testListening>=0 ? m_testListening==1 : (f && !f->isIdle());
  const bool receiving=m_testListening>=0 ? (listening && m_line>0 && m_line<m_lines)
                                          : (listening && f->sstvRxPtr && (f->sstvRxPtr->isReceivingImage() || rxWidgetPtr->rxBusy()));
  if(listening!=m_listening || receiving!=m_receiving)
    {
      m_listening=listening;
      m_receiving=receiving;
      emit stateChanged();
      emit meterChanged();
    }
  const qreal level=m_testLevel>=0 ? m_testLevel : soundIOPtr ? qBound(0.0,soundIOPtr->getVolumeDb()/100.0,1.0) : 0.0;
  if(!qFuzzyCompare(level+1,m_level+1))
    {
      m_level=level;
      emit meterChanged();
    }
}
