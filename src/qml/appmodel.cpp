#include "appmodel.h"
#include "appglobal.h"
#include "configdialog.h"
#include "mainwindow.h"
#include "platformservice.h"
#include "settingscontroller.h"
#include "soundbase.h"
#include "soundconfig.h"

#ifdef Q_OS_ANDROID
#include "androidaudio.h"
#include "androidshell.h"
#endif
#include "operatorconfig.h"
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QJSEngine>
#include <QQmlEngine>
#include <QTimer>

AppModel *AppModel::instance()
{
  static AppModel *m=new AppModel;
  return m;
}

AppModel *AppModel::create(QQmlEngine *,QJSEngine *)
{
  AppModel *m=instance();
  QJSEngine::setObjectOwnership(m,QJSEngine::CppOwnership);
  return m;
}

AppModel::AppModel(QObject *parent) : QObject(parent)
{
  // the look: saved by the Appearance page; QSSTV_QML_THEME=light|dark|dim|night is for looking at all of them on the desktop and in the tests
  const QSettings settings;
  m_theme=qBound(0,settings.value("Appearance/theme",0).toInt(),4);
  m_tintPictures=settings.value("Appearance/tintPictures",false).toBool();
  const QByteArray t=qgetenv("QSSTV_QML_THEME");
  if(t=="light") m_theme=1;
  else if(t=="dark") m_theme=2;
  else if(t=="dim") m_theme=3;
  else if(t=="night") m_theme=4;
  if(qEnvironmentVariableIsSet("QSSTV_QML_TINT")) m_tintPictures=qgetenv("QSSTV_QML_TINT")=="1";
  // "Remove animations" of the phone (and QSSTV_QML_REDUCED_MOTION=1 on the desktop); it is looked at again when the program comes to the front
  m_reducedMotion=systemAnimationsRemoved();
  // the volume keys change the media volume behind our back, also while the picture is being sent (the screen that shows it is covered then):
  // the program looks at it by itself while it is in front (#182)
  m_volumeTimer=new QTimer(this);
  m_volumeTimer->setInterval(400);
  QObject::connect(m_volumeTimer,&QTimer::timeout,this,&AppModel::pollVolume);
  QObject::connect(this,&AppModel::routeChanged,this,&AppModel::refreshVolumeDevice);   // the device was chosen: the keys may point elsewhere now
  QObject::connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state)
    {
#ifdef Q_OS_ANDROID
      if(state==Qt::ApplicationActive) {m_polls=0; pollVolume(); m_volumeTimer->start();}
      else m_volumeTimer->stop();
#endif
      if(state!=Qt::ApplicationActive) return;
      setReducedMotion(systemAnimationsRemoved());
      checkShortcut();   // a launcher shortcut brought the program to the front
    });
#ifdef Q_OS_ANDROID
  androidShellInit();
  if(qApp->applicationState()==Qt::ApplicationActive)
    {
      m_volumeTimer->start();   // the program is in front already: no state change tells
      QTimer::singleShot(0,this,&AppModel::pollVolume);
    }
  QTimer::singleShot(1500,this,[this] {checkShortcut();});   // the shortcut that started the program (the screens are up by then)
#endif
  bool ok=false;
  const double fs=qEnvironmentVariable("QSSTV_QML_FONTSCALE").toDouble(&ok);   // the same for the text size
  if(ok && fs>0.5 && fs<4.0) m_fontScale=fs;
  // the short setup comes once, and not to somebody who has set their call sign already
  m_firstRun=!QSettings().value("FirstRun/done",false).toBool() && (myCallsign.isEmpty() || myCallsign==QLatin1String("NOCALL"));
}

bool AppModel::systemAnimationsRemoved()
{
  if(qEnvironmentVariableIsSet("QSSTV_QML_REDUCED_MOTION")) return qgetenv("QSSTV_QML_REDUCED_MOTION")=="1";
#ifdef Q_OS_ANDROID
  return androidAnimationsRemoved();
#else
  return false;
#endif
}

void AppModel::checkShortcut()
{
#ifdef Q_OS_ANDROID
  const QString s=androidTakeRequestedScreen();
  if(s.isEmpty() || m_firstRun) return;
  emit screenRequested(s=="transmit" ? 1 : s=="gallery" ? 2 : 0);
#endif
}

void AppModel::setTheme(int t)
{
  t=qBound(0,t,4);
  if(t==m_theme) return;
  m_theme=t;
  QSettings().setValue("Appearance/theme",t);
  emit themeChanged();
}

void AppModel::setTintPictures(bool on)
{
  if(on==m_tintPictures) return;
  m_tintPictures=on;
  QSettings().setValue("Appearance/tintPictures",on);
  emit tintPicturesChanged();
}

void AppModel::setReducedMotion(bool on)
{
  if(on==m_reducedMotion) return;
  m_reducedMotion=on;
  qInfo("reduced motion: %s",on ? "on (the phone's animations are removed)" : "off");
  emit reducedMotionChanged();
}

void AppModel::setFirstRun(bool on)
{
  if(m_firstRun==on) return;
  m_firstRun=on;
  emit firstRunChanged();
}

void AppModel::setScreen(int s)
{
  if(s==m_screen || s<0 || s>2) return;
  m_screen=s;
  emit screenChanged();
}

bool AppModel::digital() const
{
  return transmissionModeIndex==TRXDRM;
}

void AppModel::setDigital(bool on)
{
  if((transmissionModeIndex==TRXDRM)==on) return;
  if(mainWindowPtr) mainWindowPtr->switchMode(on ? TRXDRM : TRXSSTV);   // both pages, the saved tab, the frequency list ...
  emit digitalChanged();   // also when a running transmission or reception refused the change: the strip shows what is
}

bool AppModel::mediaVolumeAvailable() const
{
  if(m_testVolume) return true;
#ifdef Q_OS_ANDROID
  return true;
#else
  return false;
#endif
}

int AppModel::mediaVolume() const
{
#ifdef Q_OS_ANDROID
  return m_volume;
#else
  return m_testVolume ? m_volume : 70;
#endif
}

void AppModel::setMediaVolume(int percent)
{
#ifdef Q_OS_ANDROID
  androidSetMediaVolumePercent(qBound(0,percent,100));
  refreshVolume();
#else
  Q_UNUSED(percent);
#endif
}

QString AppModel::versionText() const
{
  return MAJORVERSION+MINORVERSION;
}

QString AppModel::licensesText() const
{
  QFile f(QStringLiteral(":/THIRD_PARTY_LICENSES.md"));
  return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

void AppModel::openHelp()
{
  if(mainWindowPtr) QMetaObject::invokeMethod(mainWindowPtr,"slotDocumentation",Qt::QueuedConnection);
}

void AppModel::finishFirstRun()
{
  QSettings().setValue("FirstRun/done",true);
  if(m_firstRun) {m_firstRun=false; emit firstRunChanged();}
  askMicrophone();   // skipped before the sound step: ask now
}

void AppModel::askMicrophone()
{
  if(mainWindowPtr) QMetaObject::invokeMethod(mainWindowPtr,[]{mainWindowPtr->askMicrophone();},Qt::QueuedConnection);
}

void AppModel::setMicDenied(bool d)
{
  if(m_micDenied==d) return;
  m_micDenied=d;
  emit micDeniedChanged();
}

void AppModel::setSettingsOpen(bool on)
{
  if(m_settingsOpen==on) return;
  m_settingsOpen=on;
  emit settingsOpenChanged();
}

void AppModel::openSettings()
{
  setSettingsOpen(true);
}

void AppModel::openClassicSettings()
{
  // the dialog fills the screen (and keeps clear of the system bars: widgetWindows), like a page of its own
  QMetaObject::invokeMethod(qApp,[]
    {
      if(!mainWindowPtr) return;
      if(configDialogPtr)
        {
          configDialogPtr->readSettings();   // its pages take over what the new pages saved meanwhile (OK writes them all back)
          configDialogPtr->setProperty("touchMaximized",true);
          configDialogPtr->setWindowState(configDialogPtr->windowState()|Qt::WindowMaximized);
        }
      QMetaObject::invokeMethod(mainWindowPtr,"slotConfigure");
    },Qt::QueuedConnection);
}

void AppModel::openAppSettings()
{
#ifdef Q_OS_ANDROID
  androidOpenAppSettings();
#endif
}

void AppModel::systemBarIcons(bool dark)
{
#ifdef Q_OS_ANDROID
  androidDarkSystemBarIcons(dark);
#else
  Q_UNUSED(dark)
#endif
}

void AppModel::haptic(int kind)
{
#ifdef Q_OS_ANDROID
  androidHaptic(kind);
#else
  Q_UNUSED(kind)
#endif
}

void AppModel::keepScreenOn(bool on)
{
#ifdef Q_OS_ANDROID
  androidKeepScreenOn(on);
#else
  Q_UNUSED(on);
#endif
}

void AppModel::refreshVolume()
{
  int v=m_volume;
  if(m_testVolume) v=m_testPercent;
#ifdef Q_OS_ANDROID
  else v=androidMediaVolumePercent();
#endif
  if(v!=m_volume) {m_volume=v; emit mediaVolumeChanged();}
}

// Where the volume keys act, and whether that is where the sound goes (#182)
void AppModel::refreshVolumeDevice()
{
  QString keys;
  bool mismatch=false;
#ifdef Q_OS_ANDROID
  const androidVolumeRoute r=androidVolumeRouteFor(outputAudioDevice);
  keys=r.keysDevice;
  mismatch=r.mismatch;
#endif
  if(m_testVolume)
    {
      keys=m_testKeysDevice;
      mismatch=!keys.isEmpty() && !outputAudioDevice.isEmpty() && outputAudioDevice!="default" && outputAudioDevice!=keys;
    }
  if(keys==m_keysDevice && mismatch==m_mismatch) return;
  const bool routeNamed=(outputAudioDevice.isEmpty() || outputAudioDevice=="default");   // the route text names the device the keys act on
  m_keysDevice=keys;
  m_mismatch=mismatch;
  emit volumeDeviceChanged();
  if(routeNamed) emit routeChanged();
}

void AppModel::pollVolume()
{
  refreshVolume();
  if(m_polls++%5==0) refreshVolumeDevice();   // the first look, and then about every two seconds
}

void AppModel::setTestVolume(bool on,int percent,const QString &keysDevice)
{
  m_testVolume=on;
  m_testPercent=percent;
  m_testKeysDevice=keysDevice;
  emit mediaVolumeChanged();   // also: the volume is available (or not) now
  refreshVolume();
  refreshVolumeDevice();
}

void AppModel::useSystemDefaultOutput()
{
  SettingsController::instance()->setOutputDevice(QStringLiteral("default"));   // the sound restarts, as for any change of the Speaker setting
  refreshVolumeDevice();
  emit routeChanged();
}

QString AppModel::routeText() const
{
  const QString out=outputAudioDevice;
  if(out.isEmpty() || out=="default") return m_keysDevice.isEmpty() ? tr("Phone speaker") : m_keysDevice;   // the system default is the device the keys act on
  return out;
}

void AppModel::quit()
{
  if(mainWindowPtr) QMetaObject::invokeMethod(mainWindowPtr,"slotExit",Qt::QueuedConnection);
}

void AppModel::moveToBackground()
{
  platformService::moveToBackground();
}
