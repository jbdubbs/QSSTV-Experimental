#include "appmodel.h"
#include "appglobal.h"
#include "mainwindow.h"
#include "platformservice.h"
#include "soundbase.h"
#include "soundconfig.h"

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
