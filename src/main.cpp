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
#include "appglobal.h"
#include "mainwindow.h"
#include <QPixmap>
#include <QSplashScreen>
#include <QTimer>
#include "dispatcher.h"
#include "dispatch/filedecoder.h"
#include "mainwidgets/rxwidget.h"
#include "mainwidgets/txwidget.h"
#include "sound/soundbase.h"
#include "sstv/sstvparam.h"
#include <QImageReader>
#include <QRegularExpression>
#include "drmtx/drmparams.h"
#include "supportfunctions.h"


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
      if(a=="-b" || a=="--batch" || a=="-h" || a=="--help" || a=="-v" || a=="--version" || a=="--list-modes" || a=="--encode" || a.startsWith("--encode="))
        {
          qputenv("QT_QPA_PLATFORM","offscreen");
          return;
        }
    }
}

/*!
  The AppImage bundles Qt without a desktop platform theme (no GTK/KDE), so it falls back to plain Fusion with a
  near-white window colour: text boxes and unchecked checkboxes then vanish into the background. Darken the window
  and button colours (Fusion paints tab pages with the button colour) of a light scheme so the white fields and indicator boxes stand out. Dark schemes and desktops with a
  real platform theme are left alone.
*/
static void fixBareFusionLook()
{
#ifdef Q_OS_LINUX
  if(!qEnvironmentVariableIsSet("APPIMAGE")) return;
  QString theme=qEnvironmentVariable("QT_QPA_PLATFORMTHEME");
  if(theme.contains("gtk") || theme.contains("kde")) return;
  if(QApplication::style()->name().compare("fusion",Qt::CaseInsensitive)!=0) return;
  QPalette pal=QApplication::palette();
  if(pal.color(QPalette::Window).lightness()<200) return;
  pal.setColor(QPalette::Window,QColor(0xe6,0xe6,0xe6));
  pal.setColor(QPalette::Button,QColor(0xdc,0xdc,0xdc));
  pal.setColor(QPalette::Base,Qt::white);
  QApplication::setPalette(pal);
#endif
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
  QCoreApplication::setOrganizationName(ORGANIZATION);
  QCoreApplication::setApplicationName(APPLICATION);
  QApplication app( argc, argv );
  fixBareFusionLook();
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
  QCommandLineOption outDirOpt(QStringList() << "o" << "out-dir","With --batch: directory for the pictures, named <file>_<n>_<MODE>.png (default: current directory).","dir");
  QCommandLineOption modeOpt(QStringList() << "m" << "mode","Receive only this mode (short name as in --list-modes, e.g. PD120, JB60) instead of auto detection.","mode");
  QCommandLineOption timeoutOpt("timeout","With --batch: give up on a file after this many seconds (default: its length + 30 s).","seconds");
  QCommandLineOption encodeOpt("encode","Headless: transmit the picture <image> through the normal SSTV TX path into a wav file (see --wav-out, --mode) and exit. "
                               "Your settings are read but never written.","image");
  QCommandLineOption wavOutOpt("wav-out","With --encode: the wav file to write (default: <image>.wav).","file");
  QCommandLineOption listOpt("list-modes","Print the mode names that --mode accepts and exit.");
  QCommandLineOption drmOpt("drm","With --batch: receive DRM (digital SSTV) instead of analog SSTV; the received file is saved as <file>_<n>_drm.<ext> and a "
                            "'drm-stats:' line (sync / FAC / MSC percentages, MSC flaps, FAC and MSC MER) is printed per file. With --encode: transmit the picture as DRM.");
  QCommandLineOption drmModeOpt("drm-mode","With --encode --drm: robustness mode A, B or E (default: your TX setting).","mode");
  QCommandLineOption drmBwOpt("drm-bw","With --encode --drm: bandwidth 2.2 or 2.5 (kHz).","kHz");
  QCommandLineOption drmQamOpt("drm-qam","With --encode --drm: 4, 16 or 64.","qam");
  QCommandLineOption drmProtOpt("drm-prot","With --encode --drm: protection high or low.","level");
  QCommandLineOption drmIlvOpt("drm-interleave","With --encode --drm: short or long.","type");
  QCommandLineOption drmRsOpt("drm-rs","With --encode --drm: Reed-Solomon 0 (none) to 4.","n");
  QCommandLineOption drmSizeOpt("drm-size","With --encode --drm: compress the picture to about this many bytes (default: the size slider setting, 5000).","bytes");
  QCommandLineOption rxGainOpt("rx-gain","With --batch: test aid, amplify the recording by <dB> (negative to attenuate) before the receiver.","dB");
  QCommandLineOption rxNoiseOpt("rx-noise","With --batch: test aid, add white noise of this RMS level in dB full scale (e.g. -40) to the recording.","dBFS");
  parser.addOptions(QList<QCommandLineOption>() << drmOpt << drmModeOpt << drmBwOpt << drmQamOpt << drmProtOpt << drmIlvOpt << drmRsOpt << drmSizeOpt << rxGainOpt << rxNoiseOpt << helpOpt << versionOpt << decodeOpt << batchOpt << outDirOpt << modeOpt << timeoutOpt << encodeOpt << wavOutOpt << listOpt);
  parser.addPositionalArgument("file","SSTV recordings to decode (same as --decode).","[file ...]");
  if(!parser.parse(app.arguments()))
    {
      fprintf(stderr,"%s\nTry --help.\n",parser.errorText().toLocal8Bit().constData());
      return fileDecoder::EXIT_BADFILE;
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
  if(parser.isSet(listOpt))
    {
      for(int i=0;i<NUMSSTVMODES;i++) printf("%s\t%s\n",getSSTVModeNameShort((esstvMode)i).toLatin1().constData(),getSSTVModeNameLong((esstvMode)i).toLatin1().constData());
      return 0;
    }
  QStringList files=parser.values(decodeOpt)+parser.positionalArguments();
  const bool batch=parser.isSet(batchOpt);
  if(batch && files.isEmpty())
    {
      fprintf(stderr,"--batch needs at least one file to decode. Try --help.\n");
      return fileDecoder::EXIT_BADFILE;
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
      {&drmBwOpt,"--drm-bw",{"2.2","2.5"},&drmTxParams::bandwith},
      {&drmQamOpt,"--drm-qam",{"4","16","64"},&drmTxParams::qam},
      {&drmProtOpt,"--drm-prot",{"high","low"},&drmTxParams::protection},
      {&drmIlvOpt,"--drm-interleave",{"short","long"},&drmTxParams::interleaver},
      {&drmRsOpt,"--drm-rs",{"0","1","2","3","4"},&drmTxParams::reedSolomon},
    };
  QList<QPair<int drmTxParams::*,int> > drmSet;
  for(const auto &ch : drmChoices)
    {
      if(!parser.isSet(*ch.opt)) continue;
      int idx=ch.values.indexOf(QRegularExpression(QString("^%1$").arg(QRegularExpression::escape(parser.value(*ch.opt))),QRegularExpression::CaseInsensitiveOption));
      if(idx<0)
        {
          fprintf(stderr,"%s must be one of: %s\n",ch.name,ch.values.join(", ").toLocal8Bit().constData());
          return fileDecoder::EXIT_BADFILE;
        }
      drmSet.append(qMakePair(ch.field,idx));
    }

  QPixmap pixmap(":/icons/qsstvsplash.png");
  QSplashScreen splash(pixmap,Qt::WindowStaysOnTopHint);

  splashPtr=&splash;
#ifdef QT_NO_DEBUG
  if(!batch) splash.show();
#endif
  QFont f;
  f.setBold(true);
  f.setPixelSize(20);
  splashPtr->setFont(f);
  splashStr="\n\n\n";
  splashStr+=QString( "Starting %1").arg(qsstvVersion).rightJustified(25,' ')+"\n";
  splash.showMessage (splashStr,Qt::AlignLeft,Qt::white);
  tm.start(100);
  globalInit();
  mainWindowPtr=new mainWindow;
  mainWindowPtr->setWindowIcon(QPixmap(":/icons/qsstv.png"));
  if(batch) fileDecoderPtr->setBatch(parser.value(outDirOpt),timeoutSeconds);
  if(!encode)
    {
      fileDecoderPtr->setDrm(parser.isSet(drmOpt));
      fileDecoderPtr->setVerbose(!files.isEmpty());
    }
  while(1)
  {
    app.processEvents();
    if(!tm.isActive()) break;
   }
  mainWindowPtr->init(); // this must follow show() because window has to be drawn first to determine fftframe window size
  mainWindowPtr->hide();
  tm.start(100);
  while(1)
  {
    app.processEvents();
    if(!tm.isActive()) break;
   }
  splash.finish(mainWindowPtr);
  if(parser.isSet(modeOpt) && !(encode && parser.isSet(drmOpt)) && !rxWidgetPtr->setRxModeByName(parser.value(modeOpt)))
    {
      fprintf(stderr,"unknown mode \"%s\" (see --list-modes)\n",parser.value(modeOpt).toLocal8Bit().constData());
      mainWindowPtr->shutdown(false);
      globalEnd();
      return fileDecoder::EXIT_BADFILE;
    }
  if(encode && parser.isSet(drmOpt))
    {
      // --encode --drm: send the picture through the normal DRM TX path into a wav file
      QImage img(parser.value(encodeOpt));
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
      if(!txWidgetPtr->sendDrmTestImage(prm,img,parser.value(drmSizeOpt).toUInt()))
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
      QImage img(parser.value(encodeOpt));
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
      mainWindowPtr->show();
    }
  mainWindowPtr->startRunning(files.isEmpty());   // decoding files replaces the sound card receiver until it is done
  if(!files.isEmpty()) fileDecoderPtr->decodeFiles(files);
  result=app.exec();
  globalEnd();
  return result;
}
