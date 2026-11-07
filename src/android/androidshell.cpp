#include "androidshell.h"

#include "appglobal.h"
#include "qml/appmodel.h"
#include "qmlui.h"
#include "widgetwindows.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>

#include <android/log.h>

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO,"qsstv-shell",__VA_ARGS__)

static const char *activityClass="io/github/jbdubbs/qsstv_experimental/MainActivity";
static const char *serviceClass="io/github/jbdubbs/qsstv_experimental/RxService";

// MainActivity: the system's Back arrived (the Android UI thread)
static void JNICALL backInvokedNative(JNIEnv *,jclass)
{
  // a Widgets window over the Qt Quick one (the classic settings, a message box, the camera dialog) takes Back first
  QMetaObject::invokeMethod(qApp,[] {if(!widgetWindows::back()) emit AppModel::instance()->backInvoked();},Qt::QueuedConnection);
}

// RxService: the Stop button of the notification (the Android UI thread)
static void JNICALL stopRequestedNative(JNIEnv *,jclass)
{
  ALOG("stop from the notification");
  QMetaObject::invokeMethod(qApp,[] {qmlUi::stopFromNotification();},Qt::QueuedConnection);
}

void androidShellInit()
{
  static bool done=false;
  if(done) return;
  done=true;
  QJniEnvironment env;
  const JNINativeMethod back[]={{"backInvoked","()V",reinterpret_cast<void *>(backInvokedNative)}};
  if(!env.registerNativeMethods(activityClass,back,1)) ALOG("the Back callback could not be registered");
  const JNINativeMethod stop[]={{"stopRequested","()V",reinterpret_cast<void *>(stopRequestedNative)}};
  if(!env.registerNativeMethods(serviceClass,stop,1)) ALOG("the Stop callback could not be registered");
}

QString androidTakeRequestedScreen()
{
  const QJniObject s=QJniObject::callStaticObjectMethod(activityClass,"takeRequestedScreen","()Ljava/lang/String;");
  return s.isValid() ? s.toString() : QString();
}

void androidSetServiceState(int state,const QString &detail)
{
  androidShellInit();
  QJniObject context=QNativeInterface::QAndroidApplication::context();
  if(!context.isValid()) return;
  QJniObject::callStaticMethod<void>(serviceClass,"setState","(Landroid/content/Context;ILjava/lang/String;)V",
                                     context.object<jobject>(),jint(state),QJniObject::fromString(detail).object<jstring>());
}
