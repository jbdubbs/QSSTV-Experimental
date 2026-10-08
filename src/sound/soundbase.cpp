#include "soundbase.h"
#include "logging.h"
#include "configparams.h"
#include "arraydumper.h"
#include "rawclock.h"

#include <QDebug>
#include <QApplication>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define FASTCAPTURE

const QString captureStateStr[soundBase::CPEND+1]=
{
  "Capture Init",
  "Capture Starting",
  "Capture Running",
  "Capture Listen Starting",
  "Capture Listen",
  "Capture End"
};


const QString playbackStateStr[soundBase::PBEND+1]=
{
  "Playback Init",
  "Playback Starting",
  "Playback Running",
  "Playback End"
};



soundBase::soundBase(QObject *parent) : QThread(parent)
{
  captureState=CPINIT;
  playbackState=PBINIT;
  fileSource=false;
  fileEof=false;
  fileCancelled=false;
  filePaced=false;
  fileTailLeft=0;
  fileNoiseState=12345;
  overruns=0;
  stampFrames=0;
  downsampleFilterPtr=new downsampleFilter(DOWNSAMPLESIZE,true);

}

soundBase::~soundBase()
{
  delete downsampleFilterPtr;
}

void soundBase::run()
{
  stopThread=false;
  unsigned int delay=0;  //todo check use of delay
  while(!stopThread)
    {
      if((captureState==CPINIT) &&   (playbackState==PBINIT))
        {
          msleep(100);
          continue;
        }
      switch (captureState)
        {
        case CPINIT:
          break;
        case CPSTARTING:
          prepareCapture();
          flushCapture();
          rxBuffer.reset(); //clear the rxBuffer
          rxVolumeBuffer.reset();
          switchCaptureState(CPRUNNING);
          break;
        case CPRUNNING:
          if (capture()==0) msleep(1);
          break;
        case CPLISTENSTART:
          prepareCapture();
          flushCapture();
          switchCaptureState(CPLISTEN);
          break;
        case CPLISTEN:
          if(captureListen()==0) msleep(1);
          break;
        case CPEND:
          switchCaptureState(CPINIT);
          break;
        }
      switch(playbackState)
        {
        case PBINIT:
          break;
        case PBSTARTING:
          preparePlayback();
          flushPlayback();
          prebuf=true;
          if (play()==0) msleep(10);
          else
            {
              prebuf=false;
              switchPlaybackState(PBRUNNING);
              addToLog("playback started",LOGSOUND);
            }
          break;
        case PBRUNNING:
          if (play()==0)
            {
              addToLog(QString("playback stopped: delay=%1").arg(delay),LOGSOUND);
              waitPlaybackEnd();
              msleep(delay);
              waveOut.close();
              addToLog("playback stopped",LOGSOUND);
              switchPlaybackState(PBINIT);
            }
          msleep(0);
          break;
        case PBEND:
          switchPlaybackState(PBINIT);
          break;
        }
    }
}

// seconds of low level noise fed after the end of a decoded file
#define FILETAILSECONDS 5

/*!
  One block of the file source: the next DOWNSAMPLESIZE samples of the recording, then (after the end of the file)
  low level noise. Sets endAfterBlock once the noise tail has been delivered.
*/
int soundBase::readFileBlock(bool &endAfterBlock)
{
  int got=0;
  if(!fileEof)
    {
      got=fileReader.read(tempRXBuffer,DOWNSAMPLESIZE);
      if(got<DOWNSAMPLESIZE) fileEof=true;
    }
  if(fileEof)
    {
      for(int i=got;i<DOWNSAMPLESIZE;i++)
        {
          fileNoiseState=fileNoiseState*1664525u+1013904223u;
          tempRXBuffer[i]=(qint16)((int)((fileNoiseState>>16)%7)-3);
        }
      fileTailLeft-=DOWNSAMPLESIZE-got;
      if(fileTailLeft<=0) endAfterBlock=true;
      got=DOWNSAMPLESIZE;
    }
  return got;
}

int soundBase::capture()
{
  int count=0;
  bool endAfterBlock=false;
  if(rxBuffer.spaceLeft()<RXSTRIPE) return 0;
  if(fileSource)
    {
      count=readFileBlock(endAfterBlock);
      if(filePaced) msleep((100*count)/BASESAMPLERATE); // about 10 times real time, so the picture builds up visibly
    }
  else if(soundRoutingInput==SNDINFROMFILE)
    {
      count=waveIn.read((qint16*)tempRXBuffer,DOWNSAMPLESIZE);
      //delay to give realtime feeling
      if(count<0)
        {
          // we have an error in reading the wav file
          waveIn.close();
          switchCaptureState(CPINIT);
        }
      else if(count==0)
        {
          switchCaptureState(CPEND);
        }
      //    msleep((1000*count)/sampleRate);
      msleep((100*count)/sampleRate);
    }
  else if(soundDriverOK)
    {
      // read from soundcard
      count=read(countAvailable);
      if(count==0) return 0;
      if(count !=DOWNSAMPLESIZE)
        {
          switchCaptureState(CPINIT);
        }

      if((storedFrames<=(ulong)recordingSize*1048576L) && (soundRoutingInput==SNDINCARDTOFILE))
        {
          addToLog(QString("written %1 tofile").arg(count),LOGSOUND);
          waveOut.write((quint16*)tempRXBuffer,count,false);
          storedFrames+=count;
        }
    }
  downsampleFilterPtr->downSample4(tempRXBuffer);
  volume=downsampleFilterPtr->avgVolumeDb;
  rxBuffer.putNoCheck(downsampleFilterPtr->filteredDataPtr(),RXSTRIPE);
  rxVolumeBuffer.putNoCheck(downsampleFilterPtr->getVolumePtr(),RXSTRIPE);
  if(endAfterBlock) switchCaptureState(CPEND);
  return count;
}

int soundBase::captureListen()
{
  int count=read(countAvailable);
  if(count<=0) return 0;
  if(count>DOWNSAMPLESIZE) count=DOWNSAMPLESIZE;
  {
    // the soundcard's own timeline, whether or not the caller keeps up with rawRxBuffer
    double now=rawMonotonicSeconds();
    QMutexLocker lock(&stampMutex);
    stampFrames+=count;
    if(stamps.size()<100000) stamps.push_back({stampFrames,now});
  }
  if((int)rawRxBuffer.spaceLeft()<count)
    {
      overruns++;
      return count;
    }
  FILTERPARAMTYPE rawSamples[DOWNSAMPLESIZE];
  for(int i=0;i<count;i++) rawSamples[i]=FILTERPARAMTYPE(tempRXBuffer[i]);
  rawRxBuffer.putNoCheck(rawSamples,count);
  return count;
}

bool soundBase::startListen()
{
  if(!soundDriverOK)
    {
      errorHandler("No valid sound device (see configuration)","");
      return false;
    }
  switchPlaybackState(PBINIT);
  fileSource=false;
  rawRxBuffer.reset();
  overruns=0;
  {
    QMutexLocker lock(&stampMutex);
    stamps.clear();
    stampFrames=0;
  }
  if(!isRunning()) start();
  switchCaptureState(CPLISTENSTART);
  return true;
}

void soundBase::takeListenStamps(std::vector<listenStamp> &out)
{
  QMutexLocker lock(&stampMutex);
  out.insert(out.end(),stamps.begin(),stamps.end());
  stamps.clear();
}

void soundBase::stopListen()
{
  if(captureState==CPLISTENSTART || captureState==CPLISTEN) switchCaptureState(CPINIT);
  rawRxBuffer.reset();
}


void soundBase::idleTX()
{
  waveOut.closeFile();
  waveIn.closeFile();
  playbackState=PBINIT;
}

void soundBase::idleRX()
{
  if(fileSource && captureState!=CPINIT) fileCancelled=true;   // stopped before the end of the file
  captureState=CPINIT;


  waveOut.closeFile();
  waveIn.closeFile();

}


void soundBase::stopSoundThread()
{
  idleRX();
  idleTX();
  stopThread=true;
  while(isRunning())
    {
      QApplication::processEvents();
    }
  closeDevices();
}


bool soundBase::startCapture()
{
  switchPlaybackState(PBINIT);
  soundIOPtr->rxBuffer.reset();
//  soundIOPtr->rxVolumeBuffer.reset();
  downsampleFilterPtr->init();
  storedFrames=0;
  switch(soundRoutingInput)
    {
    case SNDINFROMFILE:
      {
        if(!waveIn.openFileForRead("",true))
          {
            errorHandler("File not opened","");
            return false;
          }
      }
      break;
    case SNDINCARDTOFILE:
      {
        if(!soundDriverOK)
          {
            errorHandler("No valid sound device (see configuration)","");
            return false;
          }
        if(!waveOut.openFileForWrite("",true,true)) // always output stereo
          {
            errorHandler("File not opened","");
            return false;
          }
      }
      break;
    case SNDINCARD:
      if(!soundDriverOK)
        {
          errorHandler("No valid sound device (see configuration)","");
          return false;
        }
      break;
    }
  switchCaptureState(CPSTARTING);
  return true;
}

bool soundBase::startFileCapture(const QString &path,bool realtimePacing,QString &error)
{
  switchPlaybackState(PBINIT);
  soundIOPtr->rxBuffer.reset();
  rawRxBuffer.reset();
  downsampleFilterPtr->init();
  storedFrames=0;
  fileReader.close();
  if(!fileReader.open(path,error))
    {
      fileSource=false;
      return false;
    }
  fileSource=true;
  fileEof=false;
  fileCancelled=false;
  filePaced=realtimePacing;
  fileTailLeft=(long)FILETAILSECONDS*BASESAMPLERATE;
  switchCaptureState(CPSTARTING);
  return true;
}

void soundBase::clearFileSource()
{
  fileReader.close();
  fileSource=false;
  fileEof=false;
  fileCancelled=false;
}

QString soundBase::txFileName;
volatile bool soundBase::txFileDone=false;

int soundBase::play()
{
  unsigned int numFrames;
  int framesWritten;
  if(!txFileName.isEmpty() && !prebuf && !txFileDone && txBuffer.count()==0)
    {
      // Encoding to a file nothing paces us like a sound card does: an empty buffer only means the TX
      // thread is behind, not that the transmission is over (waitEnd() sets txFileDone for that).
      msleep(1);
      return 1;
    }
  if(prebuf)
    {
      if(txBuffer.count()<(DOWNSAMPLESIZE*8))
        {
          return 0;
        }
    }
  if((numFrames=txBuffer.count())>=DOWNSAMPLESIZE) numFrames=DOWNSAMPLESIZE;
  if(numFrames>0)
    {
      framesWritten=0;
    }
  // copy first: the ring buffer wraps, so writing straight from its read pointer ran past the end of the buffer
  // whenever a block straddled the wrap (garbage in the wav every 65536 frames)
  txBuffer.copyNoCheck(tempTXBuffer,numFrames);
  if(soundRoutingOutput==SNDOUTTOFILE || !txFileName.isEmpty())  // output the wav-file
    {

      if(storedFrames<=(ulong)recordingSize*1048576L)
        {
          waveOut.write((quint16*)tempTXBuffer,numFrames,true); //always stereo
          storedFrames+=numFrames;
        }
    }
  addToLog(QString("frames to write: %1 at %2 buffered:%3").arg(numFrames).arg(txBuffer.getReadIndex()).arg(txBuffer.count()),LOGSOUND);

  //  framesWritten=write(numFrames);
  if(!txFileName.isEmpty()) return numFrames;   // encoding to a file: no sound card
  framesWritten=write(DOWNSAMPLESIZE);
  addToLog(QString("frames written: %1").arg(framesWritten),LOGSOUND);
  if(framesWritten<0)
    {
      addToLog("Sound write error",LOGSOUND);
    }
  return numFrames;
}

bool soundBase::startPlayback()
{
  switchCaptureState(CPINIT);
  if(!soundDriverOK && txFileName.isEmpty())
    {
      errorHandler("No valid sound device (see configuration)","");
      return false;
    }
  storedFrames=0;
  txFileDone=false;
  soundIOPtr->txBuffer.reset();
  if(!txFileName.isEmpty())
    {
      if(!waveOut.openFileForWrite(txFileName,false,true))
        {
          errorHandler("File not opened",txFileName);
          return false;
        }
    }
  else if(soundRoutingOutput==SNDOUTTOFILE)
    {

      if(!waveOut.openFileForWrite("",true,true)) // indicate stereo
        {
          errorHandler("File not opened","");
          return false;
        }
    }
  playbackState=PBSTARTING;

  addToLog(QString("start playback, txbuffercount: %1").arg(txBuffer.count()),LOGSOUND);
  return true;
}


void soundBase::errorHandler(QString title, QString info)
{
  addToLog(title+" "+info,LOGSOUND);
  lastErrorStr=title+" "+info;
}

void soundBase::switchCaptureState(ecaptureState cs)
{
  addToLog(QString("Switching from captureState %1 to %2").arg(captureStateStr[captureState]).arg(captureStateStr[cs]),LOGSOUND);
  captureState=cs;
}

void soundBase::switchPlaybackState(eplaybackState ps)
{
  addToLog(QString("Switching from playbackState %1 to %2").arg(playbackStateStr[playbackState]).arg(playbackStateStr[ps]),LOGSOUND);
  playbackState=ps;
}
