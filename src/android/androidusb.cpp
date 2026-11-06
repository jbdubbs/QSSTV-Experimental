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

bool androidUsbAllow(const QString &name,int seconds,QString &why)
{
  if(hasPermission(name)) return true;
  QJniObject::callStaticMethod<void>(helper,"requestPermission","(Landroid/content/Context;Ljava/lang/String;)V",
                                     context().object<jobject>(),jstr(name).object<jstring>());
  // the system's box is over the program until the person has answered; the program goes on meanwhile
  QEventLoop loop;
  QTimer poll;
  QTimer limit;
  limit.setSingleShot(true);
  QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(hasPermission(name)) loop.quit();});
  QObject::connect(&limit,&QTimer::timeout,&loop,&QEventLoop::quit);
  poll.start(250);
  limit.start(seconds*1000);
  loop.exec();
  if(hasPermission(name)) return true;
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
