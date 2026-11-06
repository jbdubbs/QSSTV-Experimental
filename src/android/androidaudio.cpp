#include "androidaudio.h"
#include "androiddevicetypes.h"

#include <QCoreApplication>
#include <QJniArray>
#include <QJniObject>

static const char *helper="io/github/jbdubbs/qsstv_experimental/AudioHelper";

static QJniObject context()
{
  return QNativeInterface::QAndroidApplication::context();
}

QList<androidAudioDevice> androidAudioDevices(bool inputs)
{
  QList<androidAudioDevice> result;
  const QJniObject ctx=context();
  if(!ctx.isValid()) return result;
  const QJniArray<jstring> lines=QJniObject::callStaticMethod<jstring[]>(helper,"listDevices",
                                   "(Landroid/content/Context;Z)[Ljava/lang/String;",ctx.object<jobject>(),jboolean(inputs));
  for(const QJniObject &line:lines)
    {
      const QStringList f=line.toString().split('\t');
      if(f.size()<4) continue;
      androidAudioDevice d;
      d.id=f[0].toInt();
      d.type=f[1].toInt();
      if(!androidDeviceUsable(d.type,inputs)) continue;   // the call path, the capture of other apps' sound, tuners...
      d.description=f[2];
      d.builtIn=(f[3]=="1");
      d.narrowband=(d.type==7);   // TYPE_BLUETOOTH_SCO
      result.append(d);
    }
  return result;
}

int androidDeviceId(const QString &savedName,bool inputs)
{
  if(savedName.isEmpty() || savedName=="default") return 0;
  for(const androidAudioDevice &d:androidAudioDevices(inputs))
    if(d.description==savedName) return d.id;
  return 0;
}

bool androidIsBuiltInSpeaker(int outputDeviceId)
{
  for(const androidAudioDevice &d:androidAudioDevices(false))
    if(d.id==outputDeviceId) return d.builtIn;
  return false;
}

int androidMediaVolumePercent()
{
  const QJniObject ctx=context();
  if(!ctx.isValid()) return 100;
  return QJniObject::callStaticMethod<jint>(helper,"musicVolumePercent","(Landroid/content/Context;)I",ctx.object<jobject>());
}

void androidSetMediaVolumePercent(int percent)
{
  const QJniObject ctx=context();
  if(!ctx.isValid()) return;
  QJniObject::callStaticMethod<void>(helper,"setMusicVolumePercent","(Landroid/content/Context;I)V",ctx.object<jobject>(),jint(percent));
}

void androidRequestTransmitFocus()
{
  const QJniObject ctx=context();
  if(ctx.isValid()) QJniObject::callStaticMethod<void>(helper,"requestTransmitFocus","(Landroid/content/Context;)V",ctx.object<jobject>());
}

void androidAbandonTransmitFocus()
{
  const QJniObject ctx=context();
  if(ctx.isValid()) QJniObject::callStaticMethod<void>(helper,"abandonTransmitFocus","(Landroid/content/Context;)V",ctx.object<jobject>());
}

void androidRequestNotificationPermission()
{
  const QJniObject ctx=context();
  if(ctx.isValid()) QJniObject::callStaticMethod<void>(helper,"requestNotificationPermission","(Landroid/content/Context;)V",ctx.object<jobject>());
}

void androidOpenAppSettings()
{
  const QJniObject ctx=context();
  if(ctx.isValid()) QJniObject::callStaticMethod<void>(helper,"openAppSettings","(Landroid/content/Context;)V",ctx.object<jobject>());
}
