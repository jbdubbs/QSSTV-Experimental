#include "qmlui.h"
#include "appmodel.h"
#include "rxcontroller.h"
#include "txcontroller.h"
#include "thumbnailprovider.h"
#include "widgetwindows.h"
#include "uimode.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

namespace qmlUi
{

static QQmlApplicationEngine *engine=nullptr;

void show()
{
  QQuickStyle::setStyle(QStringLiteral("Material"));
  qApp->installEventFilter(new widgetWindows(qApp));   // the Widgets windows that open over the Qt Quick one
  engine=new QQmlApplicationEngine(qApp);
  engine->addImageProvider(QStringLiteral("thumb"),new ThumbnailProvider(480));   // the grid
  engine->addImageProvider(QStringLiteral("full"),new ThumbnailProvider(2048));    // the viewer
  QObject::connect(engine,&QQmlApplicationEngine::objectCreationFailed,qApp,[]{qFatal("the Qt Quick interface did not load");},Qt::QueuedConnection);
  engine->loadFromModule("QSSTV","Main");
}

// the hooks do nothing while the Widgets interface is the one in use (a development build with the Qt Quick one compiled in)
void sendPicture(const QString &file) {TxController::instance()->showPicture(file);}
void notifyModeChanged() {if(uiMode::qml()) AppModel::instance()->modeChanged();}
void rxImageStarted(int w,int h) {if(uiMode::qml()) RxController::instance()->imageStarted(w,h);}
void rxLine(int line) {if(uiMode::qml()) RxController::instance()->lineDrawn(line);}
void rxImageSaved(const QString &file) {if(uiMode::qml()) RxController::instance()->imageSaved(file);}
void micDenied(bool d) {if(uiMode::qml()) AppModel::instance()->setMicDenied(d);}
void rxImageEnded() {if(uiMode::qml()) RxController::instance()->imageEnded();}
void rxImageShown() {if(uiMode::qml()) RxController::instance()->imageShown();}
void rxStatus(bool drm,const QString &text) {if(uiMode::qml()) RxController::instance()->statusReceived(drm,text);}
void rxSync(int quality) {if(uiMode::qml()) RxController::instance()->syncReceived(quality);}
void rxFftInit(int len,int rate) {if(uiMode::qml()) RxController::instance()->fftInit(len,rate);}
void rxFft(const double *fft) {if(uiMode::qml()) RxController::instance()->fftReceived(fft);}

}
