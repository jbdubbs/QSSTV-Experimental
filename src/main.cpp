#include "kgstvcontroller.h"
/**************************************************************************
*   Copyright (C) 2000-2019 by Johan Maes                                 *
*   on4qz@telenet.be                                                      *
*   https://www.qsl.net/o/on4qz                                           *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include <QApplication>

#include <QtGui>
#include <QCommandLineParser>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif
#if defined(Q_OS_LINUX)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPluginLoader>
#endif
#include "appglobal.h"
#include "mainwindow.h"
#include <QPixmap>
#include <QSplashScreen>
#include <QEventLoop>
#include <QThread>
#include <QElapsedTimer>
#include <QTimer>
#include "dispatcher.h"
#include "dispatch/filedecoder.h"
#include "mainwidgets/rxwidget.h"
#include "mainwidgets/txwidget.h"
#include "sound/soundbase.h"
#include "sound/soundqtmultimedia.h"
#include "configparams.h"
#include "sstv/sstvparam.h"
#include <QAudioDevice>
#include <QMediaDevices>
#include <QImageReader>
#include <QRegularExpression>
#include "drmtx/drmparams.h"
#include "supportfunctions.h"
#include "appimageintegration.h"
#include "templaterender.h"


QSplashScreen *splash;

#ifdef Q_OS_WIN
/*!
  The Windows build is a GUI-subsystem executable (see CMakeLists.txt's
  WIN32_EXECUTABLE) so double-clicking qsstv.exe from Explorer never flashes a console
  window behind the GUI -- but this app also has real CLI/--batch functionality (see
  chooseHeadlessPlatform() below) that needs its printf/fprintf output to actually reach
  a terminal when launched from one. Attaching to the launching console (if any) and
  redirecting the standard streams to it is the standard fix for that combination; it's a
  no-op that fails silently when there's no parent console (i.e. launched from Explorer).
*/
static void attachParentConsoleIfAny()
{
  if(AttachConsole(ATTACH_PARENT_PROCESS))
    {
      freopen("CONOUT$","w",stdout);
      freopen("CONOUT$","w",stderr);
      freopen("CONIN$","r",stdin);
    }
}
#endif

/*!
  Options that print something or run without a window must not need a display: choose Qt's offscreen platform
  for them (before QApplication is created) unless the user picked a platform.
*/
static void chooseHeadlessPlatform(int argc,char **argv)
{
  if(!qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) return;
  for(int i=1;i<argc;i++)
    {
      QString a=QString::fromLocal8Bit(argv[i]);
      if(a=="-b" || a=="--batch" || a=="-h" || a=="--help" || a=="-v" || a=="--version" || a=="--list-modes" || a=="--listen" || a=="--list-devices" || a=="--install" || a=="--uninstall" || a=="--encode" || a.startsWith("--encode=")
         || a=="--render-template" || a.startsWith("--render-template=") || a=="--convert-template" || a.startsWith("--convert-template="))
        {
          qputenv("QT_QPA_PLATFORM","offscreen");
          return;
        }
    }
}

/*!
  The sound card inputs, one per line, the one used when none is chosen marked with a tab and "(default)". These are the
  names --device accepts (and Configure > Sound stores).
*/
static void printInputDevices(FILE *out)
{
  QStringList inputs,outputs;
  getCardList(inputs,outputs);
  const QString def=QMediaDevices::defaultAudioInput().description();
  for(const QString &d:inputs)
    fprintf(out,"%s%s\n",d.toLocal8Bit().constData(),d==def ? "\t(default)" : "");
}

/*!
  --device: the input whose description is `wanted` (ignoring case), or else the only one that contains it. On failure
  the reason and the available inputs go to stderr.
*/
static bool findInputDevice(const QString &wanted,QString &description)
{
  QStringList inputs,outputs;
  getCardList(inputs,outputs);
  QStringList partial;
  for(const QString &d:inputs)
    {
      if(d.compare(wanted,Qt::CaseInsensitive)==0)
        {
          description=d;
          return true;
        }
      if(d.contains(wanted,Qt::CaseInsensitive)) partial<<d;
    }
  if(partial.size()==1)
    {
      description=partial.first();
      return true;
    }
  fprintf(stderr,partial.isEmpty() ? "--device: no sound input \"%s\". The inputs are:\n" : "--device: \"%s\" matches more than one sound input. The inputs are:\n",
          wanted.toLocal8Bit().constData());
  printInputDevices(stderr);
  return false;
}

static bool platformThemeAvailable(const QString &name)
{
#ifdef Q_OS_LINUX
  QStringList dirs=qEnvironmentVariable("QT_PLUGIN_PATH").split(':',Qt::SkipEmptyParts);
  dirs<<QLibraryInfo::path(QLibraryInfo::PluginsPath);
  for(const QString &d:dirs)
    for(const QFileInfo &fi:QDir(d+"/platformthemes").entryInfoList({"*.so"},QDir::Files))
      {
        // metadata only: the plugin itself is not loaded
        const QJsonArray keys=QPluginLoader(fi.absoluteFilePath()).metaData().value("MetaData").toObject().value("Keys").toArray();
        for(const QJsonValue &k:keys)
          if(k.toString().compare(name,Qt::CaseInsensitive)==0) return true;
      }
#else
  Q_UNUSED(name)
#endif
  return false;
}

/*!
  Use the desktop's own file dialogs through xdg-desktop-portal. The AppImage bundles Qt without a GTK/KDE platform
  theme (so it would show Qt's own dialog), and an unpackaged build only gets a native one if the distro's Qt happens to
  ship a matching theme. Qt's xdgdesktopportal theme works everywhere without GTK/KDE libraries. It is opted into only
  when the user has not chosen a loadable theme (a named one that has no plugin here, e.g. kde inside the AppImage, is replaced), we are not headless or in Flatpak (which already uses it), and the portal is
  actually on the session bus: a missing portal would otherwise stop the dialog from appearing at all. Must run before
  QApplication is constructed.
*/
static void preferPortalTheme()
{
#ifdef Q_OS_LINUX
  if(qEnvironmentVariableIsSet("FLATPAK_ID") || qgetenv("QT_QPA_PLATFORM")=="offscreen") return;
  if(qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME"))
    {
      // a desktop session names its own theme (kde, gtk3, qt6ct, ...); honour it only if that plugin can be loaded.
      // The AppImage bundles none of them, so there it would silently fall back to bare Fusion with no native dialogs.
      for(const QString &n:qEnvironmentVariable("QT_QPA_PLATFORMTHEME").split(':',Qt::SkipEmptyParts))
        if(platformThemeAvailable(n)) return;
    }
  QDBusConnection bus=QDBusConnection::sessionBus();
  if(!bus.isConnected() || !bus.interface()) return;
  const QString portal=QStringLiteral("org.freedesktop.portal.Desktop");
  // the portal is normally bus-activated, so it need not be running yet
  if(bus.interface()->isServiceRegistered(portal).value() ||
     bus.interface()->activatableServiceNames().value().contains(portal))
    qputenv("QT_QPA_PLATFORMTHEME","xdgdesktopportal");
#endif
}

/*!
  Qt's xdgdesktopportal theme (and bare Fusion with no theme) reports the desktop's light/dark preference but does not
  refresh the widget palette from it, so the app stayed light on a dark desktop. Build the palette from Fusion itself,
  which follows the current colour scheme, and rebuild it when the scheme changes. A light palette is near-white, which
  hides text boxes and unchecked checkboxes, so its window and button colours are darkened (Fusion paints tab pages with
  the button colour) to make the white fields and indicator boxes stand out. Real platform themes (kde, gtk3, qt6ct, ...)
  and non-Fusion styles are left alone.
*/
static void applySchemeAwareFusionPalette()
{
#ifdef Q_OS_LINUX
  const QString theme=qEnvironmentVariable("QT_QPA_PLATFORMTHEME");
  if(!theme.isEmpty() && theme!="xdgdesktopportal") return;
  if(QApplication::style()->name().compare("fusion",Qt::CaseInsensitive)!=0) return;
  static const auto apply=[]
    {
      QPalette pal=QApplication::style()->standardPalette();
      if(pal.color(QPalette::Window).lightness()>=200)
        {
          pal.setColor(QPalette::Window,QColor(0xe6,0xe6,0xe6));
          pal.setColor(QPalette::Button,QColor(0xdc,0xdc,0xdc));
          pal.setColor(QPalette::Base,Qt::white);
        }
      QApplication::setPalette(pal);
    };
  apply();
  QObject::connect(QGuiApplication::styleHints(),&QStyleHints::colorSchemeChanged,qApp,[]{apply();});
#endif
}

/*!
  --render-template / --convert-template: work with a template file without starting the receiver or showing a window.
  Convert writes the native .qtempl (also the way to migrate a legacy .templ); render lays the template over a picture
  exactly as the transmit path does and saves the result as a PNG.
*/
static int templateCommand(const QString &file,bool convert,const QString &outFile,const QString &baseFile,
                           const QString &replayFile,const QString &sizeText,const QStringList &vars)
{
  tpl::LoadResult lr=tpl::loadFile(file);
  if(!lr.ok)
    {
      fprintf(stderr,"%s: %s\n",qPrintable(file),qPrintable(lr.error));
      return fileDecoder::EXIT_BADFILE;
    }
  for(const QString &w:lr.warnings) fprintf(stderr,"warning: %s\n",qPrintable(w));
  const QFileInfo fi(file);
  if(convert)
    {
      const QString out=outFile.isEmpty() ? fi.absolutePath()+"/"+fi.completeBaseName()+"."+tpl::nativeExtension : outFile;
      QString err;
      if(!tpl::saveFile(out,lr.doc,&err))
        {
          fprintf(stderr,"cannot write %s: %s\n",qPrintable(out),qPrintable(err));
          return 1;
        }
      printf("wrote %s\n",qPrintable(out));
      return 0;
    }
  QImage base;
  if(!baseFile.isEmpty() && !loadImageAutoRotate(base,baseFile))
    {
      fprintf(stderr,"cannot read the picture %s\n",qPrintable(baseFile));
      return fileDecoder::EXIT_BADFILE;
    }
  QSize size=base.isNull() ? QSize(qRound(lr.doc.canvas.width()),qRound(lr.doc.canvas.height())) : base.size();
  if(!sizeText.isEmpty())
    {
      const QStringList wh=sizeText.toLower().split('x');
      bool okw=false,okh=false;
      const int w=wh.value(0).toInt(&okw),h=wh.value(1).toInt(&okh);
      if(wh.size()!=2 || !okw || !okh || w<1 || h<1 || w>8192 || h>8192)
        {
          fprintf(stderr,"--template-size needs WIDTHxHEIGHT (1 to 8192), for example 320x256\n");
          return fileDecoder::EXIT_BADFILE;
        }
      size=QSize(w,h);
    }
  if(base.isNull())
    {
      base=QImage(size,QImage::Format_ARGB32);
      base.fill(QColor(128,128,128));
    }
  else if(base.size()!=size)
    {
      base=base.scaled(size,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
    }
  tpl::RenderOptions opt;
  for(const QString &v:vars)
    {
      const int eq=v.indexOf('=');
      if(eq!=1)
        {
          fprintf(stderr,"--template-var needs a single letter, '=' and a value, for example c=W1AW\n");
          return fileDecoder::EXIT_BADFILE;
        }
      opt.vars.insert(v.at(0),v.mid(2));
    }
  if(!replayFile.isEmpty() && !loadImageAutoRotate(opt.replay,replayFile))
    {
      fprintf(stderr,"cannot read the replay picture %s\n",qPrintable(replayFile));
      return fileDecoder::EXIT_BADFILE;
    }
  const QString out=outFile.isEmpty() ? fi.absoluteFilePath()+".png" : outFile;
  if(!tpl::overlay(lr.doc,base,opt).save(out))
    {
      fprintf(stderr,"cannot write %s\n",qPrintable(out));
      return 1;
    }
  printf("wrote %s\n",qPrintable(out));
  return 0;
}

int main( int argc, char ** argv )
{

  int result;
  QTimer tm;
  tm.setSingleShot(true);

#ifdef Q_OS_WIN
  attachParentConsoleIfAny();
#endif
  chooseHeadlessPlatform(argc,argv);
  preferPortalTheme();
  QCoreApplication::setOrganizationName(ORGANIZATION);
  QCoreApplication::setApplicationName(APPLICATION);
  // Wayland app id / taskbar-icon match; under Flatpak the desktop file is named after the app ID
  QGuiApplication::setDesktopFileName(qEnvironmentVariableIsSet("FLATPAK_ID") ? qEnvironmentVariable("FLATPAK_ID")
                                                                              : QStringLiteral("qsstv-experimental"));
  QApplication app( argc, argv );
  applySchemeAwareFusionPalette();
  if(qEnvironmentVariableIsSet("QSSTV_LOG_FORMATS"))
    {
      // diagnostic: which image formats this Qt/plugin set can read, and the dialog filter built from them
      QStringList rf;
      foreach(QByteArray f,QImageReader::supportedImageFormats()) rf<<QString::fromLatin1(f);
      fprintf(stderr,"image formats readable: %s\npicture filter: %s\nLIBHEIF_PLUGIN_PATH=%s\n",
              qPrintable(rf.join(' ')),qPrintable(pictureNameFilter()),qgetenv("LIBHEIF_PLUGIN_PATH").constData());
    }

  QCommandLineParser parser;
  // QSSTV-Experimental is built upon the original QSSTV 9.5.11 by Johan Maes, ON4QZ
  // (https://www.qsl.net/o/on4qz).
  parser.setApplicationDescription("QSSTV-Experimental: receive and transmit SSTV. Recordings (WAV, MP3, FLAC, OGG, AAC) can be decoded from the command line.");
  QCommandLineOption helpOpt(QStringList() << "h" << "help","Show this help.");
  QCommandLineOption versionOpt(QStringList() << "v" << "version","Show the version.");
  QCommandLineOption decodeOpt(QStringList() << "d" << "decode","Decode the SSTV recording <file> (repeatable; bare arguments are files too). "
                               "The window opens and you watch it decode (add --drm for a DRM recording), then the sound card receiver resumes.","file");
  QCommandLineOption batchOpt(QStringList() << "b" << "batch","Headless: decode the files without a window, save every picture, print the results and exit. "
                              "Exit code 0: every file gave a picture; 1: a file gave none; 2: unusable file or bad option; 3: timeout. "
                              "Your settings are read but never written.");
  QCommandLineOption outDirOpt(QStringList() << "o" << "out-dir","With --batch or --listen: directory for the pictures, named <file>_<n>_<MODE>.png (--listen: <MODE>_<date>_<time>.png; default: current directory).","dir");
  QCommandLineOption modeOpt(QStringList() << "m" << "mode","Receive only this mode (short name as in --list-modes, e.g. PD120, JB60) instead of auto detection.","mode");
  QCommandLineOption timeoutOpt("timeout","With --batch: give up on a file after this many seconds (default: its length + 30 s). With --listen: stop after this many seconds (default: never).","seconds");
  QCommandLineOption listenOpt("listen","Headless: receive from the sound card without a window and save every picture (named <MODE>_<date>_<time>.png, UTC, in --out-dir) until Ctrl+C, SIGTERM or --timeout. "
                               "Exit code 0: stopped; 1: --timeout and no picture; 2: unusable sound input or bad option. A picture still being received is saved when you stop, if it reaches the 'Save if Complete (%)' setting of the Receive tab (see --min-complete). "
                               "Your settings are read but never written. See --device, --mode, --drm, --out-dir.");
  QCommandLineOption minCompleteOpt("min-complete","With --batch or --listen: keep an analog picture only if at least this percentage (0 to 100) of its lines was received, instead of the 'Save if Complete (%)' "
                                    "setting of the Receive tab (default 25). Applies to interrupted and poorly received pictures alike; DRM files are not affected.","percent");
  QCommandLineOption deviceOpt("device","With --listen: the sound input to receive from (default: the one in Configure > Sound). Part of the name is enough if it is unique; see --list-devices.","name");
  QCommandLineOption listDevicesOpt("list-devices","Print the sound inputs that --device accepts and exit.");
  QCommandLineOption encodeOpt("encode","Headless: transmit the picture <image> through the normal SSTV TX path into a wav file (see --wav-out, --mode) and exit. "
                               "Your settings are read but never written.","image");
  QCommandLineOption wavOutOpt("wav-out","With --encode: the wav file to write (default: <image>.wav).","file");
  QCommandLineOption listOpt("list-modes","Print the mode names that --mode accepts and exit.");
  QCommandLineOption renderTplOpt("render-template","Headless: lay the template <file> (.qtempl, or a legacy .templ) over a picture exactly as the transmit path does, "
                                  "save the result as a PNG and exit (see --template-base, --template-size, --template-var, --template-replay, --template-out).","file");
  QCommandLineOption convertTplOpt("convert-template","Headless: convert the template <file> (a legacy .templ or a .qtempl) to the native .qtempl format and exit (see --template-out). The original is not touched.","file");
  QCommandLineOption tplOutOpt("template-out","With --render-template: the PNG to write (default: <file>.png). With --convert-template: the .qtempl to write (default: next to the original).","file");
  QCommandLineOption tplBaseOpt("template-base","With --render-template: the picture to lay the template over (default: a gray frame the size of the template).","image");
  QCommandLineOption tplSizeOpt("template-size","With --render-template: scale the picture to WIDTHxHEIGHT first (default: the size of the picture), as the transmit mode would.","WxH");
  QCommandLineOption tplVarOpt("template-var","With --render-template: the value of a text macro, for example c=W1AW for %c (repeatable). Unknown macros expand to nothing.","x=value");
  QCommandLineOption tplReplayOpt("template-replay","With --render-template: the picture shown in the template's replay slot.","image");
  QCommandLineOption installOpt("install","AppImage only: add QSSTV-Experimental (menu entry, icons) to your desktop and exit.");
  QCommandLineOption uninstallOpt("uninstall","AppImage only: remove the menu entry and icons added by --install and exit.");
  QCommandLineOption kgstvOpt("kgstv","With --batch, --decode or --listen: receive experimental KG-STV image/text instead of SSTV. With --encode: write a KG-STV image WAV.");
  QCommandLineOption kgstvFourOpt("kgstv-four-level","With --kgstv: use 4L-FSK framing (default MSK).");
  QCommandLineOption drmOpt("drm","With --batch or --listen: receive DRM (digital SSTV) instead of analog SSTV; the received file is saved as <file>_<n>_drm.<ext> (--listen: drm_<date>_<time>.<ext>) and with --batch a "
                            "'drm-stats:' line (sync / FAC / MSC percentages, MSC flaps, FAC and MSC MER) is printed per file. With --encode: transmit the picture as DRM.");
  QCommandLineOption drmModeOpt("drm-mode","With --encode --drm: robustness mode A, B or E (default: your TX setting).","mode");
  QCommandLineOption drmBwOpt("drm-bw","With --encode --drm: bandwidth 2.3, 2.8 or 4.8 (kHz; 2.2 and 2.5 are accepted for 2.3 and 2.8). 4.8 is a wide VHF/UHF extension that older versions cannot receive.","kHz");
  QCommandLineOption drmQamOpt("drm-qam","With --encode --drm: 4, 16 or 64.","qam");
  QCommandLineOption drmProtOpt("drm-prot","With --encode --drm: protection high or low.","level");
  QCommandLineOption drmIlvOpt("drm-interleave","With --encode --drm: short or long.","type");
  QCommandLineOption drmRsOpt("drm-rs","With --encode --drm: Reed-Solomon 0 (none) to 4.","n");
  QCommandLineOption drmSizeOpt("drm-size","With --encode --drm: compress the picture to about this many bytes (default: the size slider setting, 5000).","bytes");
  QCommandLineOption drmEncOpt("drm-encoding","With --encode --drm: picture codec jpeg2000 or avif (default: your TX setting).","codec");
  QCommandLineOption rxGainOpt("rx-gain","With --batch: test aid, amplify the recording by <dB> (negative to attenuate) before the receiver.","dB");
  QCommandLineOption rxNoiseOpt("rx-noise","With --batch: test aid, add white noise of this RMS level in dB full scale (e.g. -40) to the recording.","dBFS");
  parser.addOptions(QList<QCommandLineOption>() << kgstvOpt << kgstvFourOpt << drmOpt << drmModeOpt << drmBwOpt << drmQamOpt << drmProtOpt << drmIlvOpt << drmRsOpt << drmSizeOpt << drmEncOpt << rxGainOpt << rxNoiseOpt << helpOpt << versionOpt << decodeOpt << batchOpt << outDirOpt << modeOpt << timeoutOpt << listenOpt << deviceOpt << listDevicesOpt << minCompleteOpt << encodeOpt << wavOutOpt << listOpt << renderTplOpt << convertTplOpt << tplOutOpt << tplBaseOpt << tplSizeOpt << tplVarOpt << tplReplayOpt << installOpt << uninstallOpt);
  parser.addPositionalArgument("file","SSTV recordings to decode (same as --decode).","[file ...]");
  if(!parser.parse(app.arguments()))
    {
      fprintf(stderr,"%s\nTry --help.\n",parser.errorText().toLocal8Bit().constData());
      return fileDecoder::EXIT_BADFILE;
    }
  if((parser.isSet(kgstvOpt) && parser.isSet(drmOpt)) || (parser.isSet(kgstvFourOpt) && !parser.isSet(kgstvOpt))) {
    fprintf(stderr,"--kgstv cannot be combined with --drm; --kgstv-four-level requires --kgstv.\n");return fileDecoder::EXIT_BADFILE;
  }
  if(parser.isSet(helpOpt))
    {
      fputs(parser.helpText().toLocal8Bit().constData(),stdout);
      return 0;
    }
  if(parser.isSet(versionOpt))
    {
      printf("%s\n",qsstvVersion.toLocal8Bit().constData());
      return 0;
    }
  if(parser.isSet(installOpt) || parser.isSet(uninstallOpt))
    {
      QString msg;
      if(!appImageIntegration::available())
        {
          fprintf(stderr,"--install and --uninstall only work when running as an AppImage.\n");
          return fileDecoder::EXIT_BADFILE;
        }
      bool ok=parser.isSet(installOpt) ? appImageIntegration::install(msg) : appImageIntegration::uninstall(msg);
      fprintf(ok ? stdout : stderr,"%s\n",msg.toLocal8Bit().constData());
      return ok ? 0 : 1;
    }
  if(parser.isSet(listOpt))
    {
      for(int i=0;i<NUMSSTVMODES;i++) printf("%s\t%s\n",getSSTVModeNameShort((esstvMode)i).toLatin1().constData(),getSSTVModeNameLong((esstvMode)i).toLatin1().constData());
      return 0;
    }
  if(parser.isSet(listDevicesOpt))
    {
      printInputDevices(stdout);
      return 0;
    }
  if(parser.isSet(renderTplOpt) || parser.isSet(convertTplOpt))
    {
      const bool convert=parser.isSet(convertTplOpt);
      return templateCommand(parser.value(convert ? convertTplOpt : renderTplOpt),convert,parser.value(tplOutOpt),parser.value(tplBaseOpt),
                             parser.value(tplReplayOpt),parser.value(tplSizeOpt),parser.values(tplVarOpt));
    }
  QStringList files=parser.values(decodeOpt)+parser.positionalArguments();
  const bool listen=parser.isSet(listenOpt);
  const bool batch=parser.isSet(batchOpt) || listen;   // --listen is headless as well
  if(listen && (!files.isEmpty() || parser.isSet(encodeOpt)))
    {
      fprintf(stderr,"--listen receives from the sound card: it cannot be combined with files or --encode. Try --help.\n");
      return fileDecoder::EXIT_BADFILE;
    }
  if(batch && !listen && files.isEmpty())
    {
      fprintf(stderr,"--batch needs at least one file to decode (or use --listen). Try --help.\n");
      return fileDecoder::EXIT_BADFILE;
    }
  QString wantedDevice;
  if(parser.isSet(deviceOpt))
    {
      if(!listen)
        {
          fprintf(stderr,"--device only goes with --listen. Try --help.\n");
          return fileDecoder::EXIT_BADFILE;
        }
      if(!findInputDevice(parser.value(deviceOpt),wantedDevice)) return fileDecoder::EXIT_BADFILE;
    }
  int minComplete=-1;   // -1: the saved setting
  if(parser.isSet(minCompleteOpt))
    {
      bool ok=false;
      minComplete=parser.value(minCompleteOpt).toInt(&ok);
      if(!ok || minComplete<0 || minComplete>100)
        {
          fprintf(stderr,"--min-complete needs a percentage from 0 to 100\n");
          return fileDecoder::EXIT_BADFILE;
        }
      if(!batch)
        {
          fprintf(stderr,"--min-complete only goes with --batch or --listen. Try --help.\n");
          return fileDecoder::EXIT_BADFILE;
        }
    }
  bool timeoutOk=true;
  int timeoutSeconds=parser.isSet(timeoutOpt) ? parser.value(timeoutOpt).toInt(&timeoutOk) : 0;
  if(!timeoutOk || timeoutSeconds<0)
    {
      fprintf(stderr,"--timeout needs a number of seconds\n");
      return fileDecoder::EXIT_BADFILE;
    }
  const bool encode=parser.isSet(encodeOpt);
  bool gainOk=true,noiseOk=true;
  if(parser.isSet(rxGainOpt)) soundBase::fileGainDb=parser.value(rxGainOpt).toDouble(&gainOk);
  if(parser.isSet(rxNoiseOpt)) soundBase::fileNoiseDbfs=parser.value(rxNoiseOpt).toDouble(&noiseOk);
  if(!gainOk || !noiseOk)
    {
      fprintf(stderr,"--rx-gain and --rx-noise need a number (dB)\n");
      return fileDecoder::EXIT_BADFILE;
    }
  // DRM transmit parameters: indices of the TX combo boxes (see txwidget.ui); unset ones keep the user's setting
  struct {const QCommandLineOption *opt; const char *name; QStringList values; int drmTxParams::*field;} drmChoices[]=
    {
      {&drmModeOpt,"--drm-mode",{"A","B","E"},&drmTxParams::robMode},
      {&drmBwOpt,"--drm-bw",{"2.3","2.8","4.8"},&drmTxParams::bandwith},
      {&drmQamOpt,"--drm-qam",{"4","16","64"},&drmTxParams::qam},
      {&drmProtOpt,"--drm-prot",{"high","low"},&drmTxParams::protection},
      {&drmIlvOpt,"--drm-interleave",{"short","long"},&drmTxParams::interleaver},
      {&drmRsOpt,"--drm-rs",{"0","1","2","3","4"},&drmTxParams::reedSolomon},
    };
  QList<QPair<int drmTxParams::*,int> > drmSet;
  for(const auto &ch : drmChoices)
    {
      if(!parser.isSet(*ch.opt)) continue;
      QString val=parser.value(*ch.opt);
      if(ch.opt==&drmBwOpt)   // the old nominal labels of the two narrow bandwidths
        {
          if(val=="2.2") val="2.3";
          else if(val=="2.5") val="2.8";
        }
      int idx=ch.values.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(val)),QRegularExpression::CaseInsensitiveOption));
      if(idx<0)
        {
          fprintf(stderr,"%s must be one of: %s\n",ch.name,ch.values.join(", ").toLocal8Bit().constData());
          return fileDecoder::EXIT_BADFILE;
        }
      drmSet.append(qMakePair(ch.field,idx));
    }

  int drmEncoding=-1;   // -1: keep the user's setting
  if(parser.isSet(drmEncOpt))
    {
      QString e=parser.value(drmEncOpt).toLower();
      if(e=="jpeg2000" || e=="jp2") drmEncoding=0;
      else if(e=="avif") drmEncoding=1;
      else
        {
          fprintf(stderr,"--drm-encoding must be one of: jpeg2000, avif\n");
          return fileDecoder::EXIT_BADFILE;
        }
    }

  QPixmap pixmap(":/icons/qsstvsplash.png");
  QSplashScreen splash(pixmap,Qt::WindowStaysOnTopHint);

  splashPtr=&splash;
  QElapsedTimer splashTime;
  bool splashShown=false;
#ifdef QT_NO_DEBUG
  if(!batch)
    {
      splash.show();
      splashShown=true;
    }
#endif
  splashTime.start();
  splashMessage("Version "+MAJORVERSION+MINORVERSION);
  tm.start(100);
  soundBase::fileOnly=parser.isSet(kgstvOpt) && ((batch && !listen) || encode);
  globalInit();
  mainWindowPtr=new mainWindow;
  if(soundBase::fileOnly){rxClock=BASESAMPLERATE;txClock=BASESAMPLERATE;} // files have a nominal clock, not a sound-card calibration
  mainWindowPtr->setWindowIcon(QPixmap(":/icons/qsstv.png"));
  if(listen)
    {
      // the window's constructor has read the settings: from here on only this run's choices (never written back)
      soundRoutingInput=soundBase::SNDINCARD;   // not a saved "input from file" / "record to file"
      if(!wantedDevice.isEmpty()) inputAudioDevice=wantedDevice;
      QStringList inputs,outputs;
      getCardList(inputs,outputs);
      // a saved device that is gone falls back to the default input (soundQtMultimedia::init)
      const QString opened=inputs.contains(inputAudioDevice) ? inputAudioDevice : QMediaDevices::defaultAudioInput().description();
      fileDecoderPtr->setListen(parser.value(outDirOpt),timeoutSeconds,opened);
    }
  else if(batch) fileDecoderPtr->setBatch(parser.value(outDirOpt),timeoutSeconds);
  if(!encode)
    {
      fileDecoderPtr->setDrm(parser.isSet(drmOpt));
      fileDecoderPtr->setKgstv(parser.isSet(kgstvOpt));
      if(parser.isSet(kgstvOpt) && kgstvControllerPtr) {
        auto options=kgstvControllerPtr->options();options.fourLevel=parser.isSet(kgstvFourOpt);kgstvControllerPtr->configure(options);
      }
      fileDecoderPtr->setVerbose(!files.isEmpty());
    }
  while(1)
  {
    app.processEvents();
    if(!tm.isActive()) break;
   }
  mainWindowPtr->init(); // this must follow show() because window has to be drawn first to determine fftframe window size
  mainWindowPtr->hide();
  if(minComplete>=0) rxWidgetPtr->setMinCompletion(minComplete);   // after init(): that is where the saved value is read
  if(listen && !soundIOPtr->driverOK())
    {
      fprintf(stderr,"cannot receive from the sound card: %s\n",soundIOPtr->getLastError().toLocal8Bit().constData());
      mainWindowPtr->shutdown(false);
      globalEnd();
      return fileDecoder::EXIT_BADFILE;
    }
  tm.start(100);
  while(1)
  {
    app.processEvents();
    if(!tm.isActive()) break;
   }
  // The splash stays up for at least 2 seconds so the logo is readable. The main window is not shown
  // until the splash closes, so it can never cover the splash (Wayland ignores WindowStaysOnTopHint).
  const int splashMinMs=2000;
  auto holdSplash=[&]()
  {
    if(!splashShown) return;
    splashMessage("Finished Starting");
    while(splashTime.elapsed()<splashMinMs)
      {
        app.processEvents(QEventLoop::AllEvents,50);
        QThread::msleep(10);
      }
  };
  if(encode)
    {
      // no main window is shown in encode mode
      holdSplash();
      splash.finish(mainWindowPtr);
    }
  if(parser.isSet(modeOpt) && !(encode && parser.isSet(drmOpt)) && !rxWidgetPtr->setRxModeByName(parser.value(modeOpt)))
    {
      fprintf(stderr,"unknown mode \"%s\" (see --list-modes)\n",parser.value(modeOpt).toLocal8Bit().constData());
      mainWindowPtr->shutdown(false);
      globalEnd();
      return fileDecoder::EXIT_BADFILE;
    }
  if(encode && parser.isSet(kgstvOpt))
    {
      QImage image;loadImageAutoRotate(image,parser.value(encodeOpt));
      if(image.isNull()){fprintf(stderr,"--encode: cannot read KG-STV image\n");mainWindowPtr->shutdown(false);globalEnd();return fileDecoder::EXIT_BADFILE;}
      mainWindowPtr->switchMode(TRXKGSTV);
      auto options=kgstvControllerPtr->options();options.fourLevel=parser.isSet(kgstvFourOpt);kgstvControllerPtr->configure(options);
      QString error;
      if(!kgstvControllerPtr->prepareImageJob(image.scaled(320,240,Qt::IgnoreAspectRatio,Qt::SmoothTransformation),myCallsign,&error)) {
        fprintf(stderr,"%s\n",qPrintable(error));mainWindowPtr->shutdown(false);globalEnd();return fileDecoder::EXIT_BADFILE;
      }
      soundBase::txFileName=parser.isSet(wavOutOpt)?parser.value(wavOutOpt):parser.value(encodeOpt)+".wav";
      QObject::connect(kgstvControllerPtr,&Kgstv::Controller::transmitFinished,&app,[&app](bool success){app.exit(success?0:2);},Qt::QueuedConnection);
      QTimer::singleShot(1800000,&app,[&app](){fprintf(stderr,"KG-STV encode timed out\n");app.exit(3);});
      mainWindowPtr->startRunning(false);
      if(!dispatcherPtr->startTX(txFunctions::TXSENDKGSTV)){mainWindowPtr->shutdown(false);globalEnd();return fileDecoder::EXIT_BADFILE;}
      result=app.exec();mainWindowPtr->shutdown(false);globalEnd();return result;
    }
  if(encode && parser.isSet(drmOpt))
    {
      // --encode --drm: send the picture through the normal DRM TX path into a wav file
      QImage img;
      loadImageAutoRotate(img,parser.value(encodeOpt));   // like the Transmit tab: a photo's EXIF/HEIF orientation is applied
      if(img.isNull())
        {
          fprintf(stderr,"--encode needs a readable image\n");
          mainWindowPtr->shutdown(false);
          globalEnd();
          return fileDecoder::EXIT_BADFILE;
        }
      mainWindowPtr->switchMode(TRXDRM);   // the window's own switch, so the tabs and both widgets follow
      drmTxParams prm=drmParams;
      for(const auto &s : drmSet) prm.*(s.first)=s.second;
      soundBase::txFileName=parser.isSet(wavOutOpt) ? parser.value(wavOutOpt) : parser.value(encodeOpt)+".wav";
      QObject::connect(txWidgetPtr,&txWidget::calibrationTxFinished,&app,[&app]()
      {
        app.exit(0);
      },Qt::QueuedConnection);
      QTimer::singleShot(1800000,&app,[&app](){fprintf(stderr,"--encode timed out\n");app.exit(3);});
      mainWindowPtr->startRunning(false);   // no sound card receiver: the transmitter writes a file
      if(!txWidgetPtr->sendDrmTestImage(prm,img,parser.value(drmSizeOpt).toUInt(),drmEncoding))
        {
          fprintf(stderr,"--encode: could not start the transmission\n");
          mainWindowPtr->shutdown(false);
          globalEnd();
          return fileDecoder::EXIT_BADFILE;
        }
      result=app.exec();
      mainWindowPtr->shutdown(false);
      globalEnd();
      fprintf(stderr,"wrote %s\n",qPrintable(soundBase::txFileName));
      return result;
    }
  if(encode)
    {
      // --encode: send the picture through the normal TX path (the calibration transmit helper) into a wav file
      esstvMode txMode=NOTVALID;
      for(int i=0;i<NUMSSTVMODES;i++)
        {
          if(getSSTVModeNameShort((esstvMode)i).compare(parser.value(modeOpt),Qt::CaseInsensitive)==0) txMode=(esstvMode)i;
        }
      QImage img;
      loadImageAutoRotate(img,parser.value(encodeOpt));   // like the Transmit tab: a photo's EXIF/HEIF orientation is applied
      if(txMode==NOTVALID || img.isNull())
        {
          fprintf(stderr,"--encode needs a readable image and a valid --mode (see --list-modes)\n");
          mainWindowPtr->shutdown(false);
          globalEnd();
          return fileDecoder::EXIT_BADFILE;
        }
      soundBase::txFileName=parser.isSet(wavOutOpt) ? parser.value(wavOutOpt) : parser.value(encodeOpt)+".wav";
      // only leave the event loop here: slotStop() emits this and then still calls startRX(), which needs the worker
      // threads, so they are shut down after exec() has returned
      QObject::connect(txWidgetPtr,&txWidget::calibrationTxFinished,&app,[&app]()
      {
        app.exit(0);
      },Qt::QueuedConnection);
      QTimer::singleShot(900000,&app,[&app](){fprintf(stderr,"--encode timed out\n");app.exit(3);});
      mainWindowPtr->startRunning(true);
      if(!txWidgetPtr->sendCalibrationImage(txMode,img))
        {
          fprintf(stderr,"--encode: could not start the transmission\n");
          mainWindowPtr->shutdown(false);
          globalEnd();
          return fileDecoder::EXIT_BADFILE;
        }
      result=app.exec();
      mainWindowPtr->shutdown(false);
      globalEnd();
      fprintf(stderr,"wrote %s\n",qPrintable(soundBase::txFileName));
      return result;
    }
  if(batch)
    {
      // queued: finished() may already be emitted by decodeFiles() (every file unusable), before exec() runs
      QObject::connect(fileDecoderPtr,&fileDecoder::finished,&app,[&app](int code)
      {
        mainWindowPtr->shutdown(false);   // no settings written, no FTP notices
        app.exit(code);
      },Qt::QueuedConnection);
    }
  else
    {
      holdSplash();
      mainWindowPtr->show();
      splash.finish(mainWindowPtr);
    }
  mainWindowPtr->startRunning(files.isEmpty() && !listen);   // decoding files replaces the sound card receiver until it is done
  if(listen) fileDecoderPtr->startListening();   // starts the receiver itself, once the mode is set
  else if(!files.isEmpty()) fileDecoderPtr->decodeFiles(files);
  result=app.exec();
  globalEnd();
  return result;
}
