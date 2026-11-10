#include "photopicker.h"

#include "appglobal.h"
#include "mainwindow.h"
#include "platformfiles.h"
#include "qmlui.h"
#include "txwidget.h"
#include "uimode.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>

#include <android/log.h>

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO,"qsstv-files",__VA_ARGS__)

static const char *activityClass="io/github/jbdubbs/qsstv_experimental/MainActivity";

// called by MainActivity.onActivityResult() on the Android UI thread
static void JNICALL photoPickedNative(JNIEnv *env,jclass,jstring uri)
{
  const char *chars=env->GetStringUTFChars(uri,nullptr);
  const QString picked=QString::fromUtf8(chars);
  env->ReleaseStringUTFChars(uri,chars);
  ALOG("photo picked: %s",qPrintable(picked));
  QMetaObject::invokeMethod(qApp,[picked]
    {
      const QString local=platformFiles::importPicked(picked);   // the system's grant ends with the activity result: copy it now
      if(local.isEmpty()) return;
      qmlUi::sendPicture(local);
    },Qt::QueuedConnection);
}

void androidPickPhoto()
{
  static bool registered=false;
  if(!registered)
    {
      QJniEnvironment env;
      const JNINativeMethod methods[]={{"photoPicked","(Ljava/lang/String;)V",reinterpret_cast<void *>(photoPickedNative)}};
      registered=env.registerNativeMethods(activityClass,methods,1);
      if(!registered) ALOG("the photo picker callback could not be registered");
    }
  QJniObject activity=QNativeInterface::QAndroidApplication::context();
  if(!activity.isValid()) return;
  activity.callMethod<void>("pickPhoto","()V");
}
