#include "platformservice.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

namespace platformService
{

#ifdef Q_OS_ANDROID

void setActive(bool on)
{
  static bool current=false;
  if(on==current) return;
  QJniObject context=QNativeInterface::QAndroidApplication::context();
  if(!context.isValid()) return;
  QJniObject::callStaticMethod<void>("io/github/jbdubbs/qsstv_experimental/RxService","setActive",
                                     "(Landroid/content/Context;Z)V",context.object<jobject>(),jboolean(on));
  current=on;
}

#else

void setActive(bool) {}

#endif

}
