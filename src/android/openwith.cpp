#include "openwith.h"

#include "appglobal.h"
#include "dispatch/filedecoder.h"
#include "platformfiles.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QImageReader>
#include "qmlui.h"
#include "mainwindow.h"
#include "txwidget.h"
#include "uimode.h"

#include <android/log.h>

#define ALOG(...) __android_log_print(ANDROID_LOG_INFO,"qsstv-files",__VA_ARGS__)

static const char *schemes[]={"file","content"};

openWith::openWith(QObject *parent) : QObject(parent), ready(false)
{
  for(const char *s:schemes) QDesktopServices::setUrlHandler(s,this,"open");
}

openWith::~openWith()
{
  for(const char *s:schemes) QDesktopServices::unsetUrlHandler(s);
}

static bool looksLikeImage(const QString &path)
{
  static const QStringList suffixes={"png","jpg","jpeg","webp","gif","bmp","heic","heif","avif","jp2","tif","tiff"};
  return suffixes.contains(QFileInfo(path).suffix().toLower());
}

static bool looksLikeRecording(const QString &path)
{
  static const QStringList suffixes={"wav","mp3","ogg","oga","opus","flac","aac","m4a","3gp","amr"};
  return suffixes.contains(QFileInfo(path).suffix().toLower());
}

void openWith::open(const QUrl &url)
{
  const bool local=url.isLocalFile();
  const QString what=local ? url.toLocalFile() : url.toString();
  ALOG("open with: %s",qPrintable(what));
  if(local && !looksLikeRecording(what) && !looksLikeImage(what))
    {
      // somebody else's file URL (not from the intent): the system gets it, as it would without this handler
      for(const char *s:schemes) QDesktopServices::unsetUrlHandler(s);
      QDesktopServices::openUrl(url);
      for(const char *s:schemes) QDesktopServices::setUrlHandler(s,this,"open");
      return;
    }
  if(!ready) {pending<<what;return;}
  decode(what);
}

void openWith::windowIsUp()
{
  ready=true;
  for(const QString &f:pending) decode(f);
  pending.clear();
}

void openWith::decode(const QString &what)
{
  const QString local=platformFiles::importPicked(what);
  ALOG("opening %s (readable %d)",qPrintable(local),int(QFileInfo(local).isReadable()));
  if(local.isEmpty()) return;
  if(looksLikeImage(local) || QImageReader(local).canRead())
    {
      // a picture shared to the program, or opened with it: it is what is going to be sent next
      ALOG("a picture to send: %s",qPrintable(local));
      qmlUi::sendPicture(local);
      return;
    }
  if(!fileDecoderPtr) return;
  fileDecoderPtr->decodeFiles(QStringList(local));
}
