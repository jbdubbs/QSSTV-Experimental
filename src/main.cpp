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
#include "sstv/engineselection.h"
#include "sstv/sstvparam.h"
#include "sstv/videofilterselection.h"
#include "sstv/mmsstvslant.h"
#include <QImageReader>
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
      if(a=="-b" || a=="--batch" || a=="-h" || a=="--help" || a=="-v" || a=="--version" || a=="--list-modes")
        {
          qputenv("QT_QPA_PLATFORM","offscreen");
          return;
        }
    }
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
                               "The window opens and you watch it decode, then the sound card receiver resumes.","file");
  QCommandLineOption batchOpt(QStringList() << "b" << "batch","Headless: decode the files without a window, save every picture, print the results and exit. "
                              "Exit code 0: every file gave a picture; 1: a file gave none; 2: unusable file or bad option; 3: timeout. "
                              "Your settings are read but never written.");
  QCommandLineOption outDirOpt(QStringList() << "o" << "out-dir","With --batch: directory for the pictures, named <file>_<n>_<MODE>.png (default: current directory).","dir");
  QCommandLineOption modeOpt(QStringList() << "m" << "mode","Receive only this mode (short name as in --list-modes, e.g. PD120, JB60) instead of auto detection.","mode");
  QCommandLineOption engineOpt("engine","Receive engine for this run: auto (your setting), qsstv (QSSTV-Experimental's own for every mode) or core (mmsstv-core where it supports the mode).","auto|qsstv|core");
  QCommandLineOption wideOpt("wide-filter","Wide video filter for the fast modes for this run: auto (your setting), on or off.","auto|on|off");
  QCommandLineOption slantOpt("slant","Auto Slant on the MMSSTV Core engine for this run: auto (your setting), on or off.","auto|on|off");
  QCommandLineOption timeoutOpt("timeout","With --batch: give up on a file after this many seconds (default: its length + 30 s).","seconds");
  QCommandLineOption listOpt("list-modes","Print the mode names that --mode accepts and exit.");
  parser.addOptions(QList<QCommandLineOption>() << helpOpt << versionOpt << decodeOpt << batchOpt << outDirOpt << modeOpt << engineOpt << wideOpt << slantOpt << timeoutOpt << listOpt);
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
  int engineOverride=-1,wideOverride=-1;
  if(parser.isSet(engineOpt))
    {
      QString v=parser.value(engineOpt).toLower();
      if(v=="qsstv") engineOverride=0;
      else if(v=="core") engineOverride=1;
      else if(v!="auto")
        {
          fprintf(stderr,"--engine must be auto, qsstv or core\n");
          return fileDecoder::EXIT_BADFILE;
        }
    }
  if(parser.isSet(wideOpt))
    {
      QString v=parser.value(wideOpt).toLower();
      if(v=="on") wideOverride=1;
      else if(v=="off") wideOverride=0;
      else if(v!="auto")
        {
          fprintf(stderr,"--wide-filter must be auto, on or off\n");
          return fileDecoder::EXIT_BADFILE;
        }
    }
  int slantOverride=-1;
  if(parser.isSet(slantOpt))
    {
      QString v=parser.value(slantOpt).toLower();
      if(v=="on") slantOverride=1;
      else if(v=="off") slantOverride=0;
      else if(v!="auto")
        {
          fprintf(stderr,"--slant must be auto, on or off\n");
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
  setRxEngineOverride(engineOverride);
  setWideVideoFilterOverride(wideOverride);
  setMmsstvSlantOverride(slantOverride);

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
  if(parser.isSet(modeOpt) && !rxWidgetPtr->setRxModeByName(parser.value(modeOpt)))
    {
      fprintf(stderr,"unknown mode \"%s\" (see --list-modes)\n",parser.value(modeOpt).toLocal8Bit().constData());
      mainWindowPtr->shutdown(false);
      globalEnd();
      return fileDecoder::EXIT_BADFILE;
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
