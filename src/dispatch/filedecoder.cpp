#include "kgstvcontroller.h"
/***************************************************************************
 *   QSSTV-Experimental: decode SSTV from audio files                       *
 *   See filedecoder.h for what this is and why.                           *
 ***************************************************************************/
#include "filedecoder.h"
#include "appglobal.h"
#include "configparams.h"
#include "directoriesconfig.h"
#include "dispatcher.h"
#include "imageviewer.h"
#include "mainwindow.h"
#include "drmrx.h"
#include "rxfunctions.h"
#include "rxwidget.h"
#include "soundbase.h"
#include "soundqtmultimedia.h"
#include "wavreader.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QMediaDevices>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <csignal>
#include <cstdio>

fileDecoder *fileDecoderPtr=nullptr;

// --listen: the signal handler only sets this, poll() does the rest. A second signal while the run is stopping
// takes the default action, so a stop that hangs can still be killed with another Ctrl+C.
static volatile std::sig_atomic_t stopSignal=0;

static void onStopSignal(int sig)
{
  if(stopSignal)
    {
      signal(sig,SIG_DFL);
      raise(sig);
      return;
    }
  stopSignal=1;
}

fileDecoder::fileDecoder(QObject *parent) : QObject(parent)
{
  batch=false;
  timeoutSeconds=0;
  running=false;
  index=-1;
  currentSeconds=0;
  imagesInFile=0;messagesInFile=0;
  filesDone=0;
  imagesTotal=0;messagesTotal=0;
  exitCode=EXIT_OK;
  connect(&timer,SIGNAL(timeout()),SLOT(poll()));
}

void fileDecoder::setBatch(const QString &dir,int timeoutSec)
{
  batch=true;
  outDir=dir.isEmpty() ? QString(".") : dir;
  timeoutSeconds=timeoutSec;
}

void fileDecoder::setListen(const QString &dir,int timeoutSec,const QString &deviceName)
{
  setBatch(dir,timeoutSec);
  listening=true;
  device=deviceName;
}

/*!
  --listen: put the receiver on the sound card and leave it there until a stop signal or the timeout (poll()).
  The mode of the run (SSTV or DRM) is switched through the window's own mode switch so the tabs follow.
*/
void fileDecoder::startListening()
{
  signal(SIGINT,onStopSignal);
  signal(SIGTERM,onStopSignal);
#ifdef SIGHUP
  signal(SIGHUP,onStopSignal);
#endif
  running=true;
  stopping=false;
  timedOut=false;
  exitCode=EXIT_OK;
  imagesTotal=0;messagesTotal=0;
  const etransmissionMode wanted=kgstv ? TRXKGSTV : (drm ? TRXDRM : TRXSSTV);
  if(transmissionModeIndex!=wanted) mainWindowPtr->switchMode(wanted);
  dispatcherPtr->startRX();
  fileTimer.start();
  inputCheck.start();
  // a QMediaDevices object keeps the platform's device list up to date, which the check in pollListen() relies on
  if(!mediaDevices) mediaDevices=new QMediaDevices(this);
  timer.start(100);
  fprintf(stderr,"listening on %s (Ctrl+C to stop)\n",device.toLocal8Bit().constData());
}

void fileDecoder::soundFailed(const QString &error)
{
  fprintf(stderr,"sound device error: %s\n",error.toLocal8Bit().constData());
  if(!listening || !running) return;
  note(EXIT_BADFILE);
  finishAll(false);
}

/*!
  Stop capturing. The receiver saves a picture it is in the middle of once it notices that the capture has ended
  (rxFunctions::run(), soundBase::finishPictureOnStop); pollListen() waits for that. Not dispatcher::idleAll(): that
  would throw the picture away.
*/
void fileDecoder::beginStop()
{
  stopping=true;
  stopTimer.start();
  soundBase::finishPictureOnStop=true;
  soundIOPtr->idleRX();
}

void fileDecoder::pollListen()
{
  if(stopping)
    {
      if(rxWidgetPtr->functionsPtr()->isIdle() || stopTimer.elapsed()>3000)
        {
          // the RX thread posts its "picture done" event before it goes idle: handle it now
          QCoreApplication::sendPostedEvents(dispatcherPtr,0);
      QCoreApplication::sendPostedEvents(rxWidgetPtr,0);
          if(timedOut && imagesTotal==0 && messagesTotal==0) note(EXIT_NOIMAGE);
          finishAll(false);
        }
      return;
    }
  if(restarting) return;   // the sound device is being rebuilt: the receiver is idle on purpose
  if(stopSignal)
    {
      fprintf(stderr,"stopping\n");
      beginStop();
      return;
    }
  if(timeoutSeconds>0 && fileTimer.elapsed()>timeoutSeconds*1000LL)
    {
      fprintf(stderr,"stopping after %d s\n",timeoutSeconds);
      timedOut=true;
      beginStop();
      return;
    }
  if(rxWidgetPtr->functionsPtr()->isIdle())
    {
      // nobody stopped the receiver, so the capture ended by itself: the device is gone
      fprintf(stderr,"the audio input stopped\n");
      note(EXIT_BADFILE);
      QCoreApplication::sendPostedEvents(dispatcherPtr,0);
      QCoreApplication::sendPostedEvents(rxWidgetPtr,0);
      finishAll(false);
      return;
    }
  if(!device.isEmpty() && inputCheck.elapsed()>2000)
    {
      // An unplugged input is not always reported by the audio stream, and when it is, the recovery (mainWindow::
      // recoverSound) reopens the default input -- which an unattended receiver must not silently listen to instead.
      inputCheck.restart();
      QStringList inputs,outputs;
      getCardList(inputs,outputs);
      if(!inputs.contains(device))
        {
          fprintf(stderr,"the sound input \"%s\" is gone\n",device.toLocal8Bit().constData());
          note(EXIT_BADFILE);
          beginStop();   // saves the picture it was receiving
        }
    }
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
  if(transmissionModeIndex<TRXSSTV || transmissionModeIndex>=TRXNOMODE)
    {
      QMessageBox::information(parent,tr("Decode from file"),tr("Choose an SSTV, DRM or KG-STV receive tab first."));
      return;
    }
  QSettings qSettings;
  qSettings.beginGroup("RX");
  QString dir=qSettings.value("lastDecodeDir",audioPath).toString();
  qSettings.endGroup();
  QStringList files=QFileDialog::getOpenFileNames(parent,transmissionModeIndex==TRXKGSTV ? tr("Decode KG-STV from audio file") : (transmissionModeIndex==TRXDRM ? tr("Decode DRM from audio file") : tr("Decode SSTV from audio file")),dir,
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
  imagesTotal=0;messagesTotal=0;
  exitCode=EXIT_OK;
  running=true;
  if(batch || drm || kgstv)
    {
      // through the window's own mode switch, so the tabs follow; assigning the global alone would let the mode lock
      // switch the tab later, which stops the receiver (and so the file) in the middle of the decode.
      // Without --batch and --drm (menu, RX button, plain --decode) the mode of the tab you are on is kept.
      const etransmissionMode wanted=kgstv ? TRXKGSTV : (drm ? TRXDRM : TRXSSTV);
      if(transmissionModeIndex!=wanted) mainWindowPtr->switchMode(wanted);
    }
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
  imagesInFile=0;messagesInFile=0;
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
  if(transmissionModeIndex==TRXKGSTV && kgstvControllerPtr)kgstvControllerPtr->clearReceive();
  drmStats.reset();
  dispatcherPtr->idleAll();
  if(!soundIOPtr->startFileCapture(currentFile,!batch,error))
    {
      note(EXIT_BADFILE);
      fprintf(stderr,"%s: %s\n",currentFile.toLocal8Bit().constData(),error.toLocal8Bit().constData());
      return false;
    }
  if(transmissionModeIndex==TRXKGSTV && kgstvControllerPtr)kgstvControllerPtr->resetDemodulator(BASESAMPLERATE);
  rxWidgetPtr->functionsPtr()->startRX();
  fileTimer.start();
  if(batch) fprintf(stderr,"decoding %s (%s, %.1f s)\n",currentFile.toLocal8Bit().constData(),description.toLocal8Bit().constData(),seconds);
  return true;
}

void fileDecoder::poll()
{
  if(!running) return;
  if(listening)
    {
      pollListen();
      return;
    }
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
      QCoreApplication::sendPostedEvents(rxWidgetPtr,0);
      finishFile();
    }
}

void fileDecoder::finishFile()
{
  filesDone++;
  imagesTotal+=imagesInFile;
  if(imagesInFile==0 && messagesInFile==0)
    {
      note(EXIT_NOIMAGE);
      if(batch) fprintf(stderr,"%s: no image or text decoded\n",currentFile.toLocal8Bit().constData());
    }
  if(transmissionModeIndex==TRXDRM && (batch || verbose)) printDrmStats();
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
  Name of the file for the next picture (imagesInFile has been counted already). A file decode names it after the
  recording, a --listen run after the time like the pictures of the window do (dispatcher::saveRxSSTVImage), numbered when
  two arrive within the same second. `tag` is the SSTV mode or "drm".
*/
QString fileDecoder::outputName(const QString &tag,const QString &ext) const
{
  if(!listening)
    return QString("%1/%2_%3_%4.%5").arg(outDir).arg(QFileInfo(currentFile).completeBaseName()).arg(imagesInFile).arg(tag).arg(ext);
  const QString stamp=QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss");
  QString name=QString("%1/%2_%3.%4").arg(outDir).arg(tag).arg(stamp).arg(ext);
  for(int n=2;QFileInfo::exists(name);n++)
    name=QString("%1/%2_%3_%4.%5").arg(outDir).arg(tag).arg(stamp).arg(n).arg(ext);
  return name;
}

/*!
  Batch mode: called by the dispatcher when the receiver has finished a picture. The picture is in the RX image viewer.
*/
void fileDecoder::imageDecoded(esstvMode mode)
{
  if(mode==NOTVALID) return;   // too few lines received: nothing to keep
  QDir().mkpath(outDir);
  imagesInFile++;
  QString name=outputName(getSSTVModeNameShort(mode),"png");
  // the receiver paints into the viewer's displayed image (not its "source" image)
  QFile::remove(name);
  rxWidgetPtr->getImageViewerPtr()->save(name,"PNG",true,false);
  QSize size=QImageReader(name).size();
  if(size.isValid())
    {
      if(listening) imagesTotal++;   // a file decode adds up imagesInFile when the file is done
      printf("%s -> %s (%s, %dx%d)\n",sourceName().toLocal8Bit().constData(),name.toLocal8Bit().constData(),
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

//! GUI: the dispatcher has shown and saved a DRM file the normal way
void fileDecoder::drmImageShown()
{
  imagesInFile++;
}

void fileDecoder::reportError(const QString &title,const QString &text)
{
  fprintf(stderr,"%s: %s\n",title.toLocal8Bit().constData(),text.toLocal8Bit().constData());
}

/*!
  Batch DRM: called by the dispatcher when the receiver has saved a received file (picture or other data).
*/
void fileDecoder::drmImageDecoded(const QString &file,const QString &info)
{
  QDir().mkpath(outDir);
  imagesInFile++;
  QString name=outputName("drm",QFileInfo(file).suffix());
  QFile::remove(name);
  if(!QFile::copy(file,name))
    {
      imagesInFile--;
      note(EXIT_BADFILE);
      fprintf(stderr,"cannot write %s\n",name.toLocal8Bit().constData());
      return;
    }
  if(listening) imagesTotal++;
  QSize size=QImageReader(name).size();
  printf("%s -> %s (DRM, %dx%d) %s\n",sourceName().toLocal8Bit().constData(),name.toLocal8Bit().constData(),
         size.width(),size.height(),info.toLocal8Bit().constData());
  fflush(stdout);
}

//! one machine readable line per file: percentages are of the RX stripes (1024 samples at 12 kHz) in the file
void fileDecoder::printDrmStats()
{
  const drmRxStats &s=drmStats;
  const double n=s.stripes>0 ? s.stripes : 1;
  printf("drm-stats: file=%s stripes=%ld time=%.0f%% frame=%.0f%% fac=%.0f%% msc=%.0f%% msc_flaps=%ld mer=%.1f mer_msc=%.1f mode=%d occupancy=%d images=%d amp_dev=%.2f msc_blocks=%ld crc_ok=%ld crc_bad=%ld pkt_bad=%ld hdr_seg=%ld data_seg=%ld\n",
         currentFile.toLocal8Bit().constData(),s.stripes,100*s.timeSync/n,100*s.frameSync/n,100*s.facValid/n,100*s.mscValid/n,s.mscFlaps,
         s.merCount>0 ? s.merSum/s.merCount : 0.0,s.merMscCount>0 ? s.merMscSum/s.merMscCount : 0.0,s.mode,s.occupancy,imagesInFile,s.ampDeviation(),s.mscBlocks,s.pktCrcOk,s.pktCrcBad,s.pktBad,s.hdrSeg,s.dataSeg);
  fflush(stdout);
}

void fileDecoder::kgstvImageDecoded(const QImage &image)
{
  if(!running)return;
  ++imagesInFile;QDir().mkpath(outDir);
  if(batch) {
    const QString name=outputName("kgstv","png");
    if(!image.save(name,"PNG")){--imagesInFile;note(EXIT_BADFILE);return;}
    printf("%s -> %s (KG-STV, 320x240, 300/300 blocks)\n",qPrintable(sourceName()),qPrintable(name));fflush(stdout);
  }
  if(listening)++imagesTotal;
}
void fileDecoder::kgstvTextDecoded(const QString &sender,const QString &text)
{
  if(!running)return;
  ++messagesInFile;++messagesTotal;
  if(batch) {
    QDir().mkpath(outDir);
    const QString name=outputName(QString("kgstv-text-%1").arg(messagesInFile),"txt");
    QFile file(name);const QByteArray bytes=(text+"\n").toUtf8();
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(bytes)!=bytes.size()){note(EXIT_BADFILE);return;}
    printf("%s -> %s (KG-STV text, sender=%s)\n",qPrintable(sourceName()),qPrintable(name),qPrintable(sender));fflush(stdout);
  }
}
