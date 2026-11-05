#include "platformfiles.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJniObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <android/log.h>
#endif

namespace platformFiles
{

#ifdef Q_OS_ANDROID

static const char *helperClass="io/github/jbdubbs/qsstv_experimental/FileHelper";

static QString importDir()
{
  return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/imports";
}

QString importPicked(const QString &picked)
{
  if(!picked.startsWith("content:")) return picked;
  QJniObject context=QNativeInterface::QAndroidApplication::context();
  QString name="file";
  if(context.isValid())
    {
      const QJniObject n=QJniObject::callStaticObjectMethod(helperClass,"displayName",
                                                            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
                                                            context.object<jobject>(),QJniObject::fromString(picked).object<jstring>());
      if(n.isValid()) name=n.toString();
    }
  // the name is what the user sees in the picker; keep it recognizable but safe as a path component
  name.replace(QRegularExpression("[^A-Za-z0-9._ -]"),"_");
  const QString dir=importDir()+"/"+QString::number(QDateTime::currentMSecsSinceEpoch());
  if(!QDir().mkpath(dir)) return QString();
  const QString dest=dir+"/"+name;
  QFile in(picked);
  QFile out(dest);
  if(!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly))
    {
      __android_log_print(ANDROID_LOG_WARN,"qsstv-files","cannot copy %s: %s",qPrintable(picked),qPrintable(in.isOpen() ? out.errorString() : in.errorString()));
      return QString();
    }
  char buffer[65536];
  qint64 n;
  while((n=in.read(buffer,sizeof(buffer)))>0)
    if(out.write(buffer,n)!=n) {out.close();QFile::remove(dest);return QString();}
  out.close();
  __android_log_print(ANDROID_LOG_INFO,"qsstv-files","picked %s -> %s (%lld bytes)",qPrintable(picked),qPrintable(dest),(long long)QFileInfo(dest).size());
  return dest;
}

bool exportToGallery(const QString &localFile)
{
  QJniObject context=QNativeInterface::QAndroidApplication::context();
  if(!context.isValid() || !QFileInfo::exists(localFile)) return false;
  const QJniObject uri=QJniObject::callStaticObjectMethod(helperClass,"saveToGallery",
                         "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                         context.object<jobject>(),QJniObject::fromString(localFile).object<jstring>(),
                         QJniObject::fromString(QFileInfo(localFile).fileName()).object<jstring>());
  const bool ok=uri.isValid() && !uri.toString().isEmpty();
  __android_log_print(ANDROID_LOG_INFO,"qsstv-files","gallery %s: %s",ok ? "added" : "skipped",qPrintable(localFile));
  return ok;
}

void cleanImportCache()
{
  QDir(importDir()).removeRecursively();
}

#else

QString importPicked(const QString &picked) {return picked;}
bool exportToGallery(const QString &) {return false;}
void cleanImportCache() {}

#endif

}
