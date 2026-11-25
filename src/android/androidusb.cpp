#include "androidusb.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJniArray>
#include <QJniObject>
#include <QTimer>

static const char *helper="io/github/jbdubbs/qsstv_experimental/UsbSerialHelper";

static QJniObject context()
{
  return QNativeInterface::QAndroidApplication::context();
}

static QJniObject jstr(const QString &s)
{
  return QJniObject::fromString(s);
}

QList<androidUsbPort> androidUsbPorts()
{
  QList<androidUsbPort> result;
  const QJniObject ctx=context();
  if(!ctx.isValid()) return result;
  const QJniArray<jstring> lines=QJniObject::callStaticMethod<jstring[]>(helper,"listPorts",
                                   "(Landroid/content/Context;)[Ljava/lang/String;",ctx.object<jobject>());
  for(const QJniObject &line:lines)
    {
      const QStringList f=line.toString().split('\t');
      if(f.size()<3) continue;
      result.append({f[0],f[1],f[2]=="1"});
    }
  return result;
}

static bool hasPermission(const QString &name)
{
  return QJniObject::callStaticMethod<jboolean>(helper,"hasPermission","(Landroid/content/Context;Ljava/lang/String;)Z",
                                                context().object<jobject>(),jstr(name).object<jstring>());
}

// asks the system for the permission and waits for the answer (the event loop goes on meanwhile): the box is over the program until
// the person has answered
template<typename Has,typename Request> static bool waitForPermission(Has has,Request request,int seconds)
{
  if(has()) return true;
  request();
  QEventLoop loop;
  QTimer poll;
  QTimer limit;
  limit.setSingleShot(true);
  QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(has()) loop.quit();});
  QObject::connect(&limit,&QTimer::timeout,&loop,&QEventLoop::quit);
  poll.start(250);
  limit.start(seconds*1000);
  loop.exec();
  return has();
}

bool androidUsbAllow(const QString &name,int seconds,QString &why)
{
  const bool ok=waitForPermission([&]{return hasPermission(name);},
    [&]{QJniObject::callStaticMethod<void>(helper,"requestPermission","(Landroid/content/Context;Ljava/lang/String;)V",
                                           context().object<jobject>(),jstr(name).object<jstring>());},seconds);
  if(ok) return true;
  why=QString("The system did not let the program use the USB adapter %1 (it asks when the adapter is plugged in, "
              "and when the program starts to use it).").arg(name);
  return false;
}

int androidUsbStartBridge(const QString &name,int baud,int dataBits,int stopBits,int parity,int flow,QString &why)
{
  const int port=QJniObject::callStaticMethod<jint>(helper,"startBridge","(Landroid/content/Context;Ljava/lang/String;IIIII)I",
                   context().object<jobject>(),jstr(name).object<jstring>(),jint(baud),jint(dataBits),jint(stopBits),jint(parity),jint(flow));
  if(port>0) return port;
  why=QJniObject::callStaticObjectMethod(helper,"lastError","()Ljava/lang/String;").toString();
  return 0;
}

bool androidUsbOpenLines(const QString &name,QString &why)
{
  const int r=QJniObject::callStaticMethod<jint>(helper,"openLines","(Landroid/content/Context;Ljava/lang/String;)I",
                context().object<jobject>(),jstr(name).object<jstring>());
  if(r==0) return true;
  why=QJniObject::callStaticObjectMethod(helper,"lastError","()Ljava/lang/String;").toString();
  return false;
}

bool androidUsbSetLines(const QString &name,int rts,int dtr)
{
  return QJniObject::callStaticMethod<jboolean>(helper,"setLines","(Ljava/lang/String;II)Z",jstr(name).object<jstring>(),jint(rts),jint(dtr));
}

void androidUsbClose(const QString &name)
{
  QJniObject::callStaticMethod<void>(helper,"close","(Ljava/lang/String;)V",jstr(name).object<jstring>());
}

// ---- CM108 GPIO PTT ----

static const char *cm108Helper="io/github/jbdubbs/qsstv_experimental/Cm108Helper";

QList<androidUsbPort> androidCm108Devices()
{
  QList<androidUsbPort> result;
  const QJniObject ctx=context();
  if(!ctx.isValid()) return result;
  const QJniArray<jstring> lines=QJniObject::callStaticMethod<jstring[]>(cm108Helper,"list",
                                   "(Landroid/content/Context;)[Ljava/lang/String;",ctx.object<jobject>());
  for(const QJniObject &line:lines)
    {
      const QStringList f=line.toString().split('\t');
      if(f.size()<3) continue;
      result.append({f[0],f[1],f[2]=="1"});
    }
  return result;
}

static bool cm108HasPermission(const QString &name)
{
  return QJniObject::callStaticMethod<jboolean>(cm108Helper,"hasPermission","(Landroid/content/Context;Ljava/lang/String;)Z",
                                                context().object<jobject>(),jstr(name).object<jstring>());
}

bool androidCm108Allow(const QString &name,int seconds,QString &why)
{
  const bool ok=waitForPermission([&]{return cm108HasPermission(name);},
    [&]{QJniObject::callStaticMethod<void>(cm108Helper,"requestPermission","(Landroid/content/Context;Ljava/lang/String;)V",
                                           context().object<jobject>(),jstr(name).object<jstring>());},seconds);
  if(ok) return true;
  why=QString("The system did not let the program use the CM108 interface %1 (it asks when the interface is plugged in, "
              "and when the program starts to use it).").arg(name);
  return false;
}

bool androidCm108Open(const QString &name,QString &why)
{
  const int r=QJniObject::callStaticMethod<jint>(cm108Helper,"open","(Landroid/content/Context;Ljava/lang/String;)I",
                context().object<jobject>(),jstr(name).object<jstring>());
  if(r==0) return true;
  why=QJniObject::callStaticObjectMethod(cm108Helper,"lastError","()Ljava/lang/String;").toString();
  return false;
}

bool androidCm108Send(const QString &name,const unsigned char data[4],QString &why)
{
  const bool ok=QJniObject::callStaticMethod<jboolean>(cm108Helper,"send","(Ljava/lang/String;IIII)Z",jstr(name).object<jstring>(),
                  jint(data[0]),jint(data[1]),jint(data[2]),jint(data[3]));
  if(!ok) why=QJniObject::callStaticObjectMethod(cm108Helper,"lastError","()Ljava/lang/String;").toString();
  return ok;
}

void androidCm108Close(const QString &name)
{
  QJniObject::callStaticMethod<void>(cm108Helper,"close","(Ljava/lang/String;)V",jstr(name).object<jstring>());
}
