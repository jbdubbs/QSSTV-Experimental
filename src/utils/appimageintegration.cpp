#include "appimageintegration.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace
{
const char *kDesktopName="qsstv-experimental.desktop";
const char *kIconName="qsstv-experimental.png";
const char *kMetainfoName="io.github.jbdubbs.qsstv_experimental.metainfo.xml";

QString appImagePath() {return qEnvironmentVariable("APPIMAGE");}
QString appDir() {return qEnvironmentVariable("APPDIR");}
QString dataHome() {return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);}
QString desktopDest() {return dataHome()+"/applications/"+kDesktopName;}
QString metainfoDest() {return dataHome()+"/metainfo/"+kMetainfoName;}
QString hicolorDir() {return dataHome()+"/icons/hicolor";}

QString quoted(const QString &p)
{
  QString q=p;
  q.replace('\\',"\\\\").replace('"',"\\\"").replace('$',"\\$").replace('`',"\\`");
  return "\""+q+"\"";
}

// Desktop-entry Exec values escape '%' by doubling it
QString execPath() {QString p=appImagePath(); p.replace('%',"%%"); return quoted(p);}

void refreshCaches()
{
  QProcess::execute("update-desktop-database",{dataHome()+"/applications"});
  QProcess::execute("gtk-update-icon-cache",{"-f","-t",hicolorDir()});
  QProcess::execute("kbuildsycoca6",{"--noincremental"});   // harmless when KDE is absent
}

QString desktopField(const QString &file,const QString &key)
{
  QFile f(file);
  if(!f.open(QIODevice::ReadOnly)) return QString();
  const QStringList lines=QString::fromUtf8(f.readAll()).split('\n');
  for(const QString &ln:lines)
    if(ln.startsWith(key+"=")) return ln.mid(key.length()+1).trimmed();
  return QString();
}

// Menu entries made by any QSSTV-Experimental release: the current fixed name, later/earlier names of ours, and any
// entry of the user's that launches a QSSTV-Experimental AppImage. appimagekit_* files belong to AppImageLauncher /
// appimaged and are left alone.
QStringList ourDesktopFiles()
{
  QStringList r;
  QDir d(dataHome()+"/applications");
  for(const QString &n:d.entryList({"*.desktop"},QDir::Files))
    {
      if(n.startsWith("appimagekit",Qt::CaseInsensitive)) continue;
      const QString path=d.filePath(n);
      const QString exec=desktopField(path,"Exec")+" "+desktopField(path,"TryExec");
      if(n==kDesktopName || n.startsWith("qsstv-experimental") ||
         (exec.contains("QSSTV-Experimental",Qt::CaseInsensitive) && exec.contains(".AppImage",Qt::CaseInsensitive)))
        r<<path;
    }
  if(!r.contains(desktopDest())) r<<desktopDest();
  return r;
}

// Remove everything a release of ours may have put in the user's data directory. True if anything was removed.
bool removeAll()
{
  bool removed=false;
  for(const QString &f:ourDesktopFiles()) removed|=QFile::remove(f);
  removed|=QFile::remove(metainfoDest());
  QDir icons(hicolorDir());
  for(const QString &sz:icons.entryList(QDir::Dirs|QDir::NoDotAndDotDot))
    removed|=QFile::remove(hicolorDir()+"/"+sz+"/apps/"+kIconName);
  return removed;
}

QString findUpdateTool()
{
  QString t=appDir()+"/usr/share/qsstv/appimageupdatetool";   // bundled copy
  if(!QFileInfo(t).isExecutable()) t=QStandardPaths::findExecutable("appimageupdatetool");
  if(t.isEmpty()) t=QStandardPaths::findExecutable("appimageupdatetool-x86_64.AppImage");
  if(t.isEmpty()) t=QStandardPaths::findExecutable("appimageupdatetool-aarch64.AppImage");
  return t;
}
}

namespace appImageIntegration
{
bool available() {return !appImagePath().isEmpty() && !appDir().isEmpty();}

bool isInstalled()
{
  for(const QString &f:ourDesktopFiles()) if(QFile::exists(f)) return true;
  return false;
}

bool isCurrent()
{
  QFile f(desktopDest());
  if(!f.open(QIODevice::ReadOnly)) return false;
  // the file must also be the only entry of ours, so leftovers from other releases get swept by a reinstall
  return QString::fromUtf8(f.readAll()).contains("Exec="+execPath()+" ") && ourDesktopFiles().size()==1;
}

QString installedTarget()
{
  for(const QString &f:ourDesktopFiles())
    {
      const QString t=desktopField(f,"TryExec");
      if(!t.isEmpty()) return t;
    }
  return QString();
}

bool installedByOtherTool()
{
  // AppImageLauncher / appimaged write appimagekit_<md5>-<name>.desktop
  QDir d(dataHome()+"/applications");
  const QStringList l=d.entryList({"appimagekit*qsstv*.desktop","appimagekit*QSSTV*.desktop"},QDir::Files);
  return !l.isEmpty();
}

bool install(QString &message)
{
  if(!available()) {message=QObject::tr("Not running from an AppImage; nothing to integrate."); return false;}
  QFile in(appDir()+"/usr/share/applications/"+kDesktopName);
  if(!in.open(QIODevice::ReadOnly))
    {
      message=QObject::tr("Cannot read %1").arg(in.fileName());
      return false;
    }
  removeAll();   // replace whatever older releases left behind, whatever it was called
  QStringList out;
  const QStringList lines=QString::fromUtf8(in.readAll()).split('\n');
  for(const QString &ln:lines)
    {
      if(ln.startsWith("Exec=")) out<<"Exec="+execPath()+" %F";
      else if(ln.startsWith("TryExec=")) out<<"TryExec="+appImagePath();
      else if(ln.startsWith("X-AppImage-")) continue;
      else out<<ln;
    }
  QDir().mkpath(QFileInfo(desktopDest()).path());
  QFile d(desktopDest());
  if(!d.open(QIODevice::WriteOnly|QIODevice::Truncate))
    {
      message=QObject::tr("Cannot write %1").arg(d.fileName());
      return false;
    }
  d.write(out.join('\n').toUtf8());
  d.close();
  // every bundled hicolor size
  QDir icons(appDir()+"/usr/share/icons/hicolor");
  for(const QString &sz:icons.entryList(QDir::Dirs|QDir::NoDotAndDotDot))
    {
      const QString src=icons.filePath(sz+"/apps/"+kIconName);
      if(!QFile::exists(src)) continue;
      const QString dst=hicolorDir()+"/"+sz+"/apps/"+kIconName;
      QDir().mkpath(QFileInfo(dst).path());
      QFile::remove(dst);
      QFile::copy(src,dst);
    }
  const QString mi=appDir()+"/usr/share/metainfo/"+kMetainfoName;
  if(QFile::exists(mi))
    {
      QDir().mkpath(QFileInfo(metainfoDest()).path());
      QFile::remove(metainfoDest());
      QFile::copy(mi,metainfoDest());
    }
  refreshCaches();
  message=QObject::tr("QSSTV-Experimental was added to your application menu:\n%1").arg(desktopDest());
  return true;
}

bool uninstall(QString &message)
{
  if(!removeAll())
    {
      message=QObject::tr("QSSTV-Experimental is not integrated (nothing to remove).");
      return false;
    }
  refreshCaches();
  message=QObject::tr("QSSTV-Experimental was removed from your application menu.");
  return true;
}

int checkForUpdate(QString &message)
{
  const QString tool=findUpdateTool();
  if(!available() || tool.isEmpty())
    {
      message=QObject::tr("appimageupdatetool was not found.");
      return -1;
    }
  QProcess p;
  QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
  env.insert("APPIMAGE_EXTRACT_AND_RUN","1");   // the tool is an AppImage too; do not require FUSE
  p.setProcessEnvironment(env);
  p.start(tool,{"--check-for-update",appImagePath()});
  if(!p.waitForFinished(60000))
    {
      p.kill();
      message=QObject::tr("The update check timed out.");
      return -1;
    }
  // appimageupdatetool: exit 0 = up to date, 1 = update available, other = error
  if(p.exitStatus()==QProcess::NormalExit && p.exitCode()==0) return 0;
  if(p.exitStatus()==QProcess::NormalExit && p.exitCode()==1) return 1;
  message=QObject::tr("no release information could be fetched");   // e.g. offline, or no published release yet
  return -1;
}

bool runUpdate()
{
  const QString tool=findUpdateTool();
  if(tool.isEmpty() || !available()) return false;
  QProcess p;
  p.setProgram(tool);
  p.setArguments({appImagePath()});
  QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
  env.insert("APPIMAGE_EXTRACT_AND_RUN","1");
  p.setProcessEnvironment(env);
  return p.startDetached();
}

bool openUrl(const QUrl &url)
{
  if(!available()) return QDesktopServices::openUrl(url);
  // The AppImage's LD_LIBRARY_PATH etc. point at our bundled libs; inherited by xdg-open and the browser it
  // starts, they make the browser fail to launch. Give the helper a cleaned environment.
  const QString root=appDir();
  QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
  for(const QString &var:{"LD_LIBRARY_PATH","PATH","XDG_DATA_DIRS","QT_PLUGIN_PATH","QML2_IMPORT_PATH","GIO_EXTRA_MODULES"})
    {
      if(!env.contains(var)) continue;
      QStringList keep;
      for(const QString &e:env.value(var).split(':',Qt::SkipEmptyParts))
        if(root.isEmpty() || !e.startsWith(root)) keep<<e;
      if(keep.isEmpty()) env.remove(var); else env.insert(var,keep.join(':'));
    }
  for(const QString &var:{"LIBHEIF_PLUGIN_PATH","QT_QPA_PLATFORM","QT_QPA_PLATFORM_PLUGIN_PATH","QT_QPA_PLATFORMTHEME","APPIMAGE_EXTRACT_AND_RUN"})
    env.remove(var);
  const QString u=url.toString(QUrl::FullyEncoded);
  const QList<QStringList> launchers={{"xdg-open"},{"gio","open"},{"kde-open"},{"sensible-browser"}};
  for(const QStringList &l:launchers)
    {
      const QString exe=QStandardPaths::findExecutable(l.first(),env.value("PATH").split(':',Qt::SkipEmptyParts));
      if(exe.isEmpty()) continue;
      QProcess p;
      p.setProgram(exe);
      p.setArguments(l.mid(1)<<u);
      p.setProcessEnvironment(env);
      if(p.startDetached()) return true;
    }
  return false;
}
}
