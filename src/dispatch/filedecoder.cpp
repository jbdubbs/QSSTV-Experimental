/***************************************************************************
 *   mmsstv-linux-port: decode SSTV from audio files                       *
 *   See filedecoder.h for what this is and why.                           *
 ***************************************************************************/
#include "filedecoder.h"
#include "appglobal.h"
#include "configparams.h"
#include "directoriesconfig.h"
#include "dispatcher.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "rxfunctions.h"
#include "rxwidget.h"
#include "mmsstv_sstv_rx.h"
#include "soundbase.h"
#include "wavreader.h"

#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <cstdio>

fileDecoder *fileDecoderPtr=nullptr;

fileDecoder::fileDecoder(QObject *parent) : QObject(parent)
{
  batch=false;
  timeoutSeconds=0;
  running=false;
  index=-1;
  currentSeconds=0;
  imagesInFile=0;
  filesDone=0;
  imagesTotal=0;
  exitCode=EXIT_OK;
  connect(&timer,SIGNAL(timeout()),SLOT(poll()));
}

void fileDecoder::setBatch(const QString &dir,int timeoutSec)
{
  batch=true;
  outDir=dir.isEmpty() ? QString(".") : dir;
  timeoutSeconds=timeoutSec;
}

void fileDecoder::chooseAndDecode(QWidget *parent)
{
  if(running)
    {
      statusBarPtr->showMessage(tr("Already decoding a file"),4000);
      return;
    }
  if(soundIOPtr && soundIOPtr->isPlaying())
    {
      QMessageBox::information(parent,tr("Decode from file"),tr("Stop the transmission first."));
      return;
    }
  if(transmissionModeIndex!=TRXSSTV)
    {
      QMessageBox::information(parent,tr("Decode from file"),tr("Decoding from a file works in SSTV mode. Switch to the SSTV tab first."));
      return;
    }
  QSettings qSettings;
  qSettings.beginGroup("RX");
  QString dir=qSettings.value("lastDecodeDir",audioPath).toString();
  qSettings.endGroup();
  QStringList files=QFileDialog::getOpenFileNames(parent,tr("Decode SSTV from audio file"),dir,
                                                  tr("Audio files (*.wav *.mp3 *.flac *.ogg *.oga *.opus *.aac *.m4a);;WAV audio (*.wav *.WAV);;All files (*)"));
  if(files.isEmpty()) return;
  qSettings.beginGroup("RX");
  qSettings.setValue("lastDecodeDir",QFileInfo(files.first()).absolutePath());
  qSettings.endGroup();
  decodeFiles(files);
}

void fileDecoder::decodeFiles(const QStringList &files)
{
  if(files.isEmpty()) return;
  if(running)
    {
      queue+=files;
      return;
    }
  queue=files;
  index=-1;
  filesDone=0;
  imagesTotal=0;
  exitCode=EXIT_OK;
  running=true;
  if(batch) transmissionModeIndex=TRXSSTV;
  timer.start(100);
  // a file that cannot be started is skipped; startNext() returns false when the queue is exhausted
  while(!startNext())
    {
      if(index>=queue.size())
        {
          finishAll(false);
          return;
        }
    }
}

/*!
  Start the next file. Returns true when a decode is running, false when the file was unusable (index has then
  already moved on; index>=queue.size() means there is nothing left).
*/
bool fileDecoder::startNext()
{
  index++;
  if(index>=queue.size()) return false;
  currentFile=queue.at(index);
  imagesInFile=0;
  QString error;
  double seconds=0;
  QString description;
  if(!wavReader::probe(currentFile,error,&seconds,&description))
    {
      note(EXIT_BADFILE);
      fprintf(stderr,"%s: %s\n",currentFile.toLocal8Bit().constData(),error.toLocal8Bit().constData());
      if(!batch) QMessageBox::warning(mainWindowPtr,tr("Decode from file"),QString("%1\n\n%2").arg(currentFile).arg(error));
      return false;
    }
  currentSeconds=seconds;
  dispatcherPtr->idleAll();
  // mmsstv-core's demodulator stays locked after a picture (it never drops its own lock), which hides the next
  // file's VIS: put it back to hunting for one (applied by the RX thread, see MmsstvSstvRx::serviceAbort())
  rxWidgetPtr->functionsPtr()->resetCoreLock();
  if(!soundIOPtr->startFileCapture(currentFile,!batch,error))
    {
      note(EXIT_BADFILE);
      fprintf(stderr,"%s: %s\n",currentFile.toLocal8Bit().constData(),error.toLocal8Bit().constData());
      return false;
    }
  rxWidgetPtr->functionsPtr()->startRX();
  fileTimer.start();
  if(batch) fprintf(stderr,"decoding %s (%s, %.1f s)\n",currentFile.toLocal8Bit().constData(),description.toLocal8Bit().constData(),seconds);
  return true;
}

void fileDecoder::poll()
{
  if(!running) return;
  if(soundIOPtr->fileSourceCancelled())
    {
      finishAll(true);   // the user pressed Stop
      return;
    }
  if(!batch)
    {
      statusBarPtr->showMessage(tr("Decoding %1: %2 %").arg(QFileInfo(currentFile).fileName()).arg(soundIOPtr->fileProgressPercent()));
    }
  else
    {
      int limit=timeoutSeconds>0 ? timeoutSeconds : (int)(currentSeconds+30);
      if(fileTimer.elapsed()>limit*1000LL)
        {
          fprintf(stderr,"%s: timed out after %d s\n",currentFile.toLocal8Bit().constData(),limit);
          note(EXIT_TIMEOUT);
          dispatcherPtr->idleAll();
          finishAll(false);
          return;
        }
    }
  if(soundIOPtr->fileDecodeFinished() && rxWidgetPtr->functionsPtr()->isIdle())
    {
      // the RX thread posts its "picture done" events before it goes idle: handle them now
      QCoreApplication::sendPostedEvents(dispatcherPtr,0);
      finishFile();
    }
}

void fileDecoder::finishFile()
{
  filesDone++;
  imagesTotal+=imagesInFile;
  if(imagesInFile==0)
    {
      note(EXIT_NOIMAGE);
      if(batch) fprintf(stderr,"%s: no image decoded\n",currentFile.toLocal8Bit().constData());
    }
  soundIOPtr->clearFileSource();
  while(!startNext())
    {
      if(index>=queue.size())
        {
          finishAll(false);
          return;
        }
    }
}

void fileDecoder::finishAll(bool cancelled)
{
  timer.stop();
  running=false;
  soundIOPtr->clearFileSource();
  if(batch)
    {
      fflush(stdout);
      emit finished(exitCode);
      return;
    }
  statusBarPtr->showMessage(cancelled ? tr("File decoding stopped")
                                      : tr("Finished decoding %1 file(s), %2 image(s)").arg(filesDone).arg(imagesTotal),8000);
  if(!cancelled) dispatcherPtr->startRX();   // back to the sound card
}

/*!
  Batch mode: called by the dispatcher when the receiver has finished a picture. The picture is in the RX image viewer.
*/
void fileDecoder::imageDecoded(esstvMode mode)
{
  if(mode==NOTVALID) return;   // too few lines received: nothing to keep
  QDir().mkpath(outDir);
  imagesInFile++;
  QString name=QString("%1/%2_%3_%4.png").arg(outDir).arg(QFileInfo(currentFile).completeBaseName()).arg(imagesInFile).arg(getSSTVModeNameShort(mode));
  // the receiver paints into the viewer's displayed image (not its "source" image)
  QFile::remove(name);
  rxWidgetPtr->getImageViewerPtr()->save(name,"PNG",true,false);
  QSize size=QImageReader(name).size();
  if(size.isValid())
    {
      printf("%s -> %s (%s, %dx%d)\n",currentFile.toLocal8Bit().constData(),name.toLocal8Bit().constData(),
             getSSTVModeNameShort(mode).toLatin1().constData(),size.width(),size.height());
      fflush(stdout);
    }
  else
    {
      imagesInFile--;
      note(EXIT_BADFILE);
      fprintf(stderr,"cannot write %s\n",name.toLocal8Bit().constData());
    }
}

void fileDecoder::reportError(const QString &title,const QString &text)
{
  fprintf(stderr,"%s: %s\n",title.toLocal8Bit().constData(),text.toLocal8Bit().constData());
}
