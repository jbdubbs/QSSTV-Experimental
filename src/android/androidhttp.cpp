#include "androidhttp.h"

#include <QCoreApplication>
#include <QHash>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>

#include <android/log.h>

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO,"qsstv-http",__VA_ARGS__)

static const char *helperClass="io/github/jbdubbs/qsstv_experimental/HttpHelper";

// the requests that wait for an answer (GUI thread only)
static QHash<qint64,androidHttpDone> &waiting()
{
  static QHash<qint64,androidHttpDone> w;
  return w;
}
static qint64 lastToken=0;

static QString fromJava(JNIEnv *env,jstring s)
{
  if(s==nullptr) return QString();
  const char *chars=env->GetStringUTFChars(s,nullptr);
  const QString text=QString::fromUtf8(chars);
  env->ReleaseStringUTFChars(s,chars);
  return text;
}

// called by HttpHelper on its own thread
static void JNICALL httpDoneNative(JNIEnv *env,jclass,jlong token,jint status,jstring body,jstring error)
{
  const QByteArray data=fromJava(env,body).toUtf8();
  const QString why=fromJava(env,error);
  QMetaObject::invokeMethod(qApp,[token,status,data,why]
    {
      auto it=waiting().find(token);
      if(it==waiting().end()) return;   // cancelled
      const androidHttpDone done=it.value();
      waiting().erase(it);
      done(status,data,why);
    },Qt::QueuedConnection);
}

qint64 androidHttpGet(const QUrl &url,const QByteArray &userAgent,int timeoutMs,androidHttpDone done)
{
  static bool registered=false;
  if(!registered)
    {
      QJniEnvironment env;
      const JNINativeMethod methods[]={{"done","(JILjava/lang/String;Ljava/lang/String;)V",reinterpret_cast<void *>(httpDoneNative)}};
      registered=env.registerNativeMethods(helperClass,methods,1);
      if(!registered) ALOG("the answer callback could not be registered");
    }
  const qint64 token=++lastToken;
  if(!registered)
    {
      QMetaObject::invokeMethod(qApp,[token,done] {done(0,QByteArray(),QStringLiteral("the system's HTTPS helper is not available"));},Qt::QueuedConnection);
      return token;
    }
  waiting().insert(token,done);
  QJniObject::callStaticMethod<void>(helperClass,"get","(JLjava/lang/String;Ljava/lang/String;I)V",
                                     jlong(token),QJniObject::fromString(url.toString()).object<jstring>(),
                                     QJniObject::fromString(QString::fromUtf8(userAgent)).object<jstring>(),jint(timeoutMs));
  return token;
}

void androidHttpCancel(qint64 token)
{
  waiting().remove(token);
}
