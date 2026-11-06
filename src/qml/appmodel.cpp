#include "appmodel.h"
#include "appglobal.h"
#include "configdialog.h"
#include "mainwindow.h"
#include "platformservice.h"
#include "soundbase.h"
#include "soundconfig.h"

#ifdef Q_OS_ANDROID
#include "androidaudio.h"
#endif
#include <QFile>
#include <QJSEngine>
#include <QQmlEngine>

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
  // for looking at both colour schemes on the desktop and in the tests: QSSTV_QML_THEME=light|dark
  const QByteArray t=qgetenv("QSSTV_QML_THEME");
  if(t=="light") m_colorScheme=1;
  else if(t=="dark") m_colorScheme=2;
  bool ok=false;
  const double fs=qEnvironmentVariable("QSSTV_QML_FONTSCALE").toDouble(&ok);   // the same for the text size
  if(ok && fs>0.5 && fs<4.0) m_fontScale=fs;
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

void AppModel::openSettings()
{
  // the dialog fills the screen (and keeps clear of the system bars: widgetWindows), like a page of its own
  QMetaObject::invokeMethod(qApp,[]
    {
      if(!mainWindowPtr) return;
      if(configDialogPtr)
        {
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
