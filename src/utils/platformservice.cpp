#include "platformservice.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include "androidshell.h"
#endif

namespace platformService
{

#ifdef Q_OS_ANDROID

void setState(int state,const QString &detail)
{
  static int current=0;
  static QString currentDetail;
  if(state==current && detail==currentDetail) return;
  androidSetServiceState(state,detail);
  current=state;
  currentDetail=detail;
}

void setActive(bool on)
{
  setState(on ? 1 : 0);
}

void moveToBackground()
{
  QJniObject activity=QNativeInterface::QAndroidApplication::context();
  if(activity.isValid()) activity.callMethod<jboolean>("moveTaskToBack","(Z)Z",jboolean(true));
}

#else

void setState(int,const QString &) {}
void setActive(bool) {}
void moveToBackground() {}

#endif

}
