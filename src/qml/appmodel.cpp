#include "appmodel.h"
#include "appglobal.h"
#include "configdialog.h"
#include "mainwindow.h"
#include "platformservice.h"
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
  QObject::connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState state)
    {
      if(state!=Qt::ApplicationActive) return;
      setReducedMotion(systemAnimationsRemoved());
      checkShortcut();   // a launcher shortcut brought the program to the front
    });
#ifdef Q_OS_ANDROID
  androidShellInit();
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
  return 70;
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
#ifdef Q_OS_ANDROID
  const int v=androidMediaVolumePercent();
  if(v!=m_volume) {m_volume=v; emit mediaVolumeChanged();}
#endif
}

QString AppModel::routeText() const
{
  const QString out=outputAudioDevice;
  if(out.isEmpty() || out=="default") return tr("Phone speaker");
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
