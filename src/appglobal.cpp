#include "filedecoder.h"
#include "kgstvcontroller.h"
#include "appglobal.h"
#include "logging.h"
#include "soundbase.h"
#include <QPixmap>
#include <QCursor>
#include <QSplashScreen>
#include <QPainter>
#include <QFontMetrics>


const QString MAJORVERSION  = "10.1";
const QString CONFIGVERSION = "9.0";
const QString MINORVERSION  = "";
// QSSTV-Experimental is built upon the original QSSTV 9.5.11 by Johan Maes, ON4QZ
// (https://www.qsl.net/o/on4qz). CONFIGVERSION is the settings-format version written into
// saved editor files; it no longer names the QSettings storage location.
const QString LOGVERSION = ("qsstv."+MAJORVERSION+MINORVERSION+".log");
// QSettings location: ~/.config/qsstv-experimental/qsstv-experimental.conf on Linux
const QString ORGANIZATION = "qsstv-experimental";
const QString APPLICATION  = "qsstv-experimental";
const QString qsstvVersion=QString("QSSTV-Experimental " + MAJORVERSION+MINORVERSION);
const QString APPNAME=QString("QSSTV-Experimental");


QSplashScreen *splashPtr;

// Shows a single status line on the splash screen, centred just below the
// pixel bar of the logo. The previous line is replaced (the text is painted
// onto a fresh copy of the base image each time).
void splashMessage(const QString &msg)
{
  static const int textTop=250;     // first pixel row under the pixel bar (image is 736x330)
  static const int textHeight=40;
  if(!splashPtr) return;
  static const QPixmap base(":/icons/qsstvsplash.png");
  QPixmap pm(base);
  QPainter painter(&pm);
  QFont f;
  f.setBold(true);
  f.setPixelSize(20);
  painter.setFont(f);
  painter.setPen(Qt::black);
  QString text=QFontMetrics(f).elidedText(msg,Qt::ElideRight,pm.width()-20);
  painter.drawText(QRect(0,textTop,pm.width(),textHeight),Qt::AlignHCenter|Qt::AlignTop,text);
  painter.end();
  splashPtr->setPixmap(pm);
}

Kgstv::Controller *kgstvControllerPtr=nullptr;
mainWindow *mainWindowPtr;
soundBase *soundIOPtr;
logFile *logFilePtr;
configDialog *configDialogPtr;






dispatcher *dispatcherPtr;
QStatusBar *statusBarPtr;
rxWidget *rxWidgetPtr;
txWidget *txWidgetPtr;
galleryWidget *galleryWidgetPtr;
waterfallText *waterfallPtr;
rigControl *rigControllerPtr;
gpsReader *gpsReaderPtr;
placeLookup *placeLookupPtr;
xmlInterface *xmlIntfPtr;
logBook *logBookPtr;
fileWatcher *fileWatcherPtr;


int fftNumBlocks=2;
bool useHybrid;
bool inStartup;






etransmissionMode transmissionModeIndex;  // SSTV , DRM



QPixmap *greenPXMPtr;
QPixmap *redPXMPtr;

#ifndef QT_NO_DEBUG
scopeView *scopeViewerData;
scopeView *scopeViewerSyncNarrow;
scopeView *scopeViewerSyncWide;
#endif


void globalInit()
{
  logFilePtr=new logFile();
  logFilePtr->open(LOGVERSION);
  QSettings qSettings;
  qSettings.beginGroup("MAIN");
  logFilePtr->readSettings();
  greenPXMPtr=new QPixmap(16,16);
  greenPXMPtr->fill(Qt::green);
  redPXMPtr=new QPixmap(16,16);
  redPXMPtr->fill(Qt::red);
  qSettings.endGroup();
  kgstvControllerPtr=new Kgstv::Controller;
  qSettings.beginGroup("KGSTV");
  Kgstv::Options options;
  options.quality=qBound(0,qSettings.value("quality",8).toInt(),15);
  options.fec=qSettings.value("fec",true).toBool();options.fourLevel=qSettings.value("fourLevel",false).toBool();
  kgstvControllerPtr->configure(options);
  qSettings.endGroup();
}

void globalEnd(void)
{
  if(kgstvControllerPtr) {
    if(!soundBase::fileOnly && (!fileDecoderPtr || !fileDecoderPtr->isBatch())) {
      QSettings settings;settings.beginGroup("KGSTV");const auto options=kgstvControllerPtr->options();
      settings.setValue("quality",options.quality);settings.setValue("fec",options.fec);settings.setValue("fourLevel",options.fourLevel);settings.endGroup();
    }
    delete kgstvControllerPtr;kgstvControllerPtr=nullptr;
  }
  if(!soundBase::fileOnly)logFilePtr->writeSettings();
  logFilePtr->close();
}

