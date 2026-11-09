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

// HEIC/HEIF by its name, or by a HEIC brand (major or compatible) in the file's first box. Many camera files have the major
// brand mif1 and heic only among the compatible ones; an AVIF file lists avif/mif1/miaf and is not taken for HEIC.
static bool isHeic(const QString &path)
{
  const QString suffix=QFileInfo(path).suffix().toLower();
  if(suffix=="heic" || suffix=="heif" || suffix=="heics" || suffix=="heifs") return true;
  QFile f(path);
  if(!f.open(QIODevice::ReadOnly)) return false;
  const QByteArray head=f.read(64);
  if(head.size()<16 || head.mid(4,4)!="ftyp") return false;
  const quint32 boxSize=(quint32(quint8(head[0]))<<24)|(quint32(quint8(head[1]))<<16)|(quint32(quint8(head[2]))<<8)|quint32(quint8(head[3]));
  const int end=qMin<int>(head.size(),boxSize);
  static const QList<QByteArray> brands={"heic","heix","hevc","hevx","heim","heis"};
  for(int i=8;i+4<=end;i+=(i==8 ? 8 : 4))   // major brand, then (skipping the minor version) the compatible ones
    if(brands.contains(head.mid(i,4))) return true;
  return false;
}

// The program has no HEVC decoder, so a HEIC picture is decoded by the phone (FileHelper.decodeToPng) and the PNG goes on.
// If the phone cannot, the HEIC stays and fails later like any picture that cannot be read.
static QString decodeHeic(const QString &path)
{
  if(!isHeic(path)) return path;
  const QJniObject png=QJniObject::callStaticObjectMethod(helperClass,"decodeToPng","(Ljava/lang/String;)Ljava/lang/String;",
                                                          QJniObject::fromString(path).object<jstring>());
  const QString out=png.isValid() ? png.toString() : QString();
  __android_log_print(ANDROID_LOG_INFO,"qsstv-files","HEIC %s -> %s",qPrintable(path),out.isEmpty() ? "not decoded" : qPrintable(out));
  if(out.isEmpty()) return path;
  QFile::remove(path);
  return out;
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
  return decodeHeic(dest);
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

bool shareImage(const QString &localFile)
{
  QJniObject context=QNativeInterface::QAndroidApplication::context();
  if(!context.isValid() || !QFileInfo::exists(localFile)) return false;
  QJniObject::callStaticMethod<void>(helperClass,"shareImage","(Landroid/content/Context;Ljava/lang/String;)V",
                                     context.object<jobject>(),QJniObject::fromString(localFile).object<jstring>());
  return true;
}

void cleanImportCache()
{
  QDir(importDir()).removeRecursively();
}

#else

QString importPicked(const QString &picked) {return picked;}
bool exportToGallery(const QString &) {return false;}
bool shareImage(const QString &) {return false;}
void cleanImportCache() {}

#endif

}
