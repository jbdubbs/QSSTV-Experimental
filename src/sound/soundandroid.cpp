#include "soundandroid.h"
#include "configparams.h"   // inputAudioDevice, outputAudioDevice, micPreset, rxGainDb, the channel options (via soundconfig.h)
#include "platformpermissions.h"
#include "androidaudio.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QMetaObject>

#include <android/log.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#undef OK   // supportfunctions.h has #define OK true, which would turn oboe::Result::OK into oboe::Result::true

// ---------------------------------------------------------------------------------------------------------------------
// the Oboe callbacks: nothing but a call into the backend. Both run on Android's audio threads and must not block.

class soundAndroid::dataCallback : public oboe::AudioStreamDataCallback
{
public:
  dataCallback(soundAndroid *o,bool in) : owner(o), input(in) {}
  oboe::DataCallbackResult onAudioReady(oboe::AudioStream *,void *data,int32_t numFrames) override
  {
    if(input) return owner->inputReady(static_cast<const qint16 *>(data),numFrames);
    return owner->outputReady(static_cast<qint16 *>(data),numFrames);
  }
private:
  soundAndroid *owner;
  bool input;
};

class soundAndroid::errorCallback : public oboe::AudioStreamErrorCallback
{
public:
  explicit errorCallback(soundAndroid *o) : owner(o) {}
  // the stream was closed by the system: a headset or USB interface that was unplugged, or the audio service restarting
  void onErrorAfterClose(oboe::AudioStream *stream,oboe::Result r) override
  {
    owner->streamFailed(stream->getDirection()==oboe::Direction::Input ? QString("capture") : QString("playback"),r);
  }
private:
  soundAndroid *owner;
};

// ---------------------------------------------------------------------------------------------------------------------

// The program's own log (addToLog) is compiled out of release builds; these few lines are what `adb logcat -s qsstv-audio` shows
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO,"qsstv-audio",__VA_ARGS__)

#define WAITMS 10   // longest a thread waits for the audio callback before looking at its stop flag again

static const char *presetName(oboe::InputPreset p)
{
  switch(p)
    {
    case oboe::InputPreset::Unprocessed: return "unprocessed";
    case oboe::InputPreset::VoiceRecognition: return "voice recognition";
    case oboe::InputPreset::Generic: return "standard";
    case oboe::InputPreset::Camcorder: return "camcorder";
    case oboe::InputPreset::VoiceCommunication: return "voice communication";
    case oboe::InputPreset::VoicePerformance: return "voice performance";
    default: return "unknown";
    }
}

soundAndroid::soundAndroid() : capRing(17), playRing(17)   // 131072 samples: 2.7 s of microphone, 1.4 s of stereo speaker
{
  inRunning=false;
  outRunning=false;
  outChannels=1;
  closing=false;
  idleTicks=0;
  readerWaiting=false;
  writerWaiting=false;
  gainQ8=256;
  clips=0;
  overruns=0;
  underruns=0;
  zeroRun=0;
  peak=0;
  focusHeld=false;
  txConv.resize(size_t(DOWNSAMPLESIZE)*2);
  inCallback=std::make_shared<dataCallback>(this,true);
  outCallback=std::make_shared<dataCallback>(this,false);
  errCallback=std::make_shared<errorCallback>(this);
}

soundAndroid::~soundAndroid()
{
  closeDevices();
}

void soundAndroid::setRxGainDb(double db)
{
  db=std::max(0.0,std::min(30.0,db));
  gainQ8=int(std::lround(std::pow(10.0,db/20.0)*256.0));
}

bool soundAndroid::init(int samplerate)
{
  soundDriverOK=false;
  sampleRate=samplerate;
  closeDevices();   // restartSound() builds a new object each time, but a second init() on one must start clean

  if(!platformPermissions::microphoneGranted())
    {
      errorHandler("Audio init error","the microphone permission has not been granted");
      return false;
    }

  // One channel unless a PTT tone goes on the other channel. A phone speaker and most radio interfaces are mono, and Android
  // sums a stereo stream for them, which would put the tone into the picture.
  outChannels=(pttToneOtherChannel && !duplicateChannel) ? 2 : 1;
  setRxGainDb(rxGainDb);

  // the saved names are the ones androidAudioDevices() makes ("Phone speaker", "USB audio: Digirig Mobile"); 0 lets the system
  // choose the route, which follows a headset being plugged in
  const int inId=androidDeviceId(inputAudioDevice,true);
  const int outId=androidDeviceId(outputAudioDevice,false);
  QString why;
  if(!openInput(inId,why))
    {
      closeDevices();
      errorHandler("Audio init error",QString("capture: %1").arg(why));
      return false;
    }
  if(!openOutput(outId,why))
    {
      closeDevices();
      errorHandler("Audio init error",QString("playback: %1").arg(why));
      return false;
    }
  if(outChannels==2 && androidIsBuiltInSpeaker(outStream->getDeviceId()))
    {
      // Android sums a stereo stream for a mono speaker: the PTT tone on the second channel would land in the picture
      outStream->close();
      outStream.reset();
      outChannels=1;
      if(!openOutput(outId,why))
        {
          closeDevices();
          errorHandler("Audio init error",QString("playback: %1").arg(why));
          return false;
        }
      emit audioNotice(tr("The PTT tone on the second channel is not used on the phone's own speaker."));
    }
  route=QString("input id %1, %2 Hz, %3 microphone processing; output id %4, %5 channel%6")
        .arg(inStream->getDeviceId()).arg(inStream->getSampleRate()).arg(presetName(inStream->getInputPreset()))
        .arg(outStream->getDeviceId()).arg(outChannels).arg(outChannels==1 ? "" : "s");
  addToLog("Android audio: "+route,LOGSOUND);
  ALOG("opened: %s",qPrintable(route));
  for(const androidAudioDevice &d:androidAudioDevices(true)) ALOG("input  id %d type %d: %s",d.id,d.type,qPrintable(d.description));
  for(const androidAudioDevice &d:androidAudioDevices(false)) ALOG("output id %d type %d: %s",d.id,d.type,qPrintable(d.description));
  ALOG("media volume %d%%",androidMediaVolumePercent());
  isStereo=false;
  soundDriverOK=true;
  return true;
}

bool soundAndroid::openInput(int deviceId,QString &why)
{
  // the processing the system may apply to the microphone. "Automatic" asks for the rawest one and falls back, since not
  // every handset offers the unprocessed source
  std::vector<oboe::InputPreset> tries;
  switch(micPreset)
    {
    case micPresetUnprocessed: tries={oboe::InputPreset::Unprocessed,oboe::InputPreset::Generic}; break;
    case micPresetVoiceRecognition: tries={oboe::InputPreset::VoiceRecognition,oboe::InputPreset::Generic}; break;
    case micPresetStandard: tries={oboe::InputPreset::Generic}; break;
    default: tries={oboe::InputPreset::Unprocessed,oboe::InputPreset::VoiceRecognition,oboe::InputPreset::Generic}; break;
    }
  for(oboe::InputPreset preset : tries)
    {
      oboe::AudioStreamBuilder b;
      b.setDirection(oboe::Direction::Input)
       ->setPerformanceMode(oboe::PerformanceMode::PowerSaving)   // a picture takes minutes: no need for the low latency path
       ->setSharingMode(oboe::SharingMode::Shared)
       ->setFormat(oboe::AudioFormat::I16)
       ->setChannelCount(1)
       ->setSampleRate(sampleRate)
       ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
       ->setFormatConversionAllowed(true)
       ->setChannelConversionAllowed(true)
       ->setInputPreset(preset)
       ->setDeviceId(deviceId)
       ->setDataCallback(inCallback)
       ->setErrorCallback(errCallback);
      const oboe::Result r=b.openStream(inStream);
      if(r==oboe::Result::OK) break;
      why=QString::fromUtf8(oboe::convertToText(r));
      inStream.reset();
    }
  if(!inStream) return false;
  if(inStream->getSampleRate()!=sampleRate)
    {
      why=QString("the microphone runs at %1 Hz, not %2 Hz").arg(inStream->getSampleRate()).arg(sampleRate);
      inStream->close();
      inStream.reset();
      return false;
    }
  return true;
}

bool soundAndroid::openOutput(int deviceId,QString &why)
{
  oboe::AudioStreamBuilder b;
  b.setDirection(oboe::Direction::Output)
   ->setPerformanceMode(oboe::PerformanceMode::PowerSaving)
   ->setSharingMode(oboe::SharingMode::Shared)
   ->setUsage(oboe::Usage::Media)                  // the loudspeaker, not the earpiece, and not the call volume
   ->setContentType(oboe::ContentType::Music)
   ->setFormat(oboe::AudioFormat::I16)
   ->setChannelCount(outChannels)
   ->setSampleRate(sampleRate)
   ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::Medium)
   ->setFormatConversionAllowed(true)
   ->setChannelConversionAllowed(true)
   ->setDeviceId(deviceId)
   ->setDataCallback(outCallback)
   ->setErrorCallback(errCallback);
  const oboe::Result r=b.openStream(outStream);
  if(r!=oboe::Result::OK)
    {
      why=QString::fromUtf8(oboe::convertToText(r));
      outStream.reset();
      return false;
    }
  if(outStream->getSampleRate()!=sampleRate)
    {
      why=QString("the speaker runs at %1 Hz, not %2 Hz").arg(outStream->getSampleRate()).arg(sampleRate);
      outStream->close();
      outStream.reset();
      return false;
    }
  return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// capture

oboe::DataCallbackResult soundAndroid::inputReady(const qint16 *data,int numFrames)
{
  const int gain=gainQ8.load(std::memory_order_relaxed);
  qint16 chunk[1024];
  int done=0;
  int clipped=0;
  bool silent=true;
  int maxRaw=0;
  while(done<numFrames)
    {
      const int n=std::min(1024,numFrames-done);
      for(int i=0;i<n;i++)
        {
          const int raw=data[done+i];
          if(raw!=0) silent=false;
          if(raw>maxRaw) maxRaw=raw; else if(-raw>maxRaw) maxRaw=-raw;
          int v=(raw*gain)>>8;
          if(raw>=32700 || raw<=-32700) clipped++;   // the microphone itself is overloaded
          else if(v>32767) {v=32767;clipped++;}      // or the digital gain pushed it over
          else if(v<-32768) {v=-32768;clipped++;}
          chunk[i]=qint16(v);
        }
      if(capRing.write(chunk,size_t(n))<size_t(n)) overruns++;   // the sound thread is behind; the receiver sees a gap
      done+=n;
    }
  if(clipped) clips+=clipped;
  if(silent) zeroRun+=unsigned(numFrames); else zeroRun=0;
  if(maxRaw>peak.load(std::memory_order_relaxed)) peak.store(maxRaw,std::memory_order_relaxed);
  if(readerWaiting.load(std::memory_order_acquire)) dataCond.wakeOne();
  return oboe::DataCallbackResult::Continue;
}

void soundAndroid::startInput()
{
  idleTicks=0;
  if(!inStream || inRunning) return;
  capRing.reset();
  zeroRun=0;
  const oboe::Result r=inStream->requestStart();
  if(r!=oboe::Result::OK)
    {
      errorHandler("Audio capture error",QString::fromUtf8(oboe::convertToText(r)));
      return;
    }
  inRunning=true;
  ALOG("microphone started");
}

void soundAndroid::stopInput()
{
  if(!inStream || !inRunning) return;
  inStream->requestStop();
  oboe::StreamState next=oboe::StreamState::Stopping;
  inStream->waitForStateChange(oboe::StreamState::Stopping,&next,500*oboe::kNanosPerMillisecond);   // no callback after this
  inRunning=false;
  ALOG("microphone stopped, %u overruns, %d clipped samples",overruns.load(),clips.load());
}

void soundAndroid::prepareCapture()
{
  if(fileSource) return;   // decoding a recording: the microphone is not needed (and its indicator stays off)
  startInput();
}

QString soundAndroid::inputProblem()
{
  static QString last;
  static int polls=0;
  if(inRunning && ++polls%10==0) ALOG("input level: peak %d of 32768, %u samples of digital silence in a row, app state %d",peak.exchange(0),zeroRun.load(),int(QGuiApplication::applicationState()));
  const QString now=inputProblemNow();
  if(now!=last)
    {
      last=now;
      ALOG("input problem: %s",now.isEmpty() ? "none" : qPrintable(now));
    }
  return now;
}

QString soundAndroid::inputProblemNow()
{
  if(!inRunning) return QString();
  // a microphone that is muted, in use by a call, or blocked in the privacy settings delivers exact zeros, which a working one never does
  if(zeroRun.load()>=3u*unsigned(sampleRate))
    return tr("No microphone signal: it may be muted, in use by a call or another app, or blocked in the system's privacy settings.");
  // the count is since the last look, about a second ago: a clip here and there is nothing, hundreds are a distorted tone
  if(clips.exchange(0)>=200)
    return tr("The microphone is overloaded: move it away from the sound source or lower the volume there.");
  return QString();
}

void soundAndroid::preparePlayback()
{
  // called again every few milliseconds until enough of the transmission is queued: once per transmission
  if(focusHeld) return;
  focusHeld=true;
  androidRequestTransmitFocus();   // other apps stop playing, so nothing sounds over the picture
  const int volume=androidMediaVolumePercent();
  ALOG("media volume %d%%%s",volume,volume<40 ? ", telling the user" : "");
  if(volume<40) emit audioNotice(tr("The media volume is %1%. Raise it with the volume keys so the transmission is loud enough.").arg(volume));
}

void soundAndroid::idleTick()
{
  // a transmission that was stopped part way never reaches waitPlaybackEnd(): give the audio focus back at the latest here
  if(focusHeld)
    {
      stopOutput();
      androidAbandonTransmitFocus();
      focusHeld=false;
    }
  // neither receiving nor transmitting for three seconds: let go of the microphone (battery, and the system's microphone indicator)
  if(inRunning && ++idleTicks>=30) stopInput();
}

int soundAndroid::read(int &countAvailable)
{
  if(!soundDriverOK || !inStream) return 0;
  idleTicks=0;
  if(capRing.readable()<size_t(DOWNSAMPLESIZE))
    {
      // sleep until the callback has completed a block: about twelve wakeups a second instead of a thousand polls
      readerWaiting.store(true,std::memory_order_release);
      {
        QMutexLocker l(&waitMutex);
        if(capRing.readable()<size_t(DOWNSAMPLESIZE) && !stopThread) dataCond.wait(&waitMutex,WAITMS);
      }
      readerWaiting.store(false,std::memory_order_release);
    }
  const size_t have=capRing.readable();
  countAvailable=int(std::min(have,size_t(DOWNSAMPLESIZE)));
  if(have<size_t(DOWNSAMPLESIZE)) return 0;
  capRing.read(tempRXBuffer,size_t(DOWNSAMPLESIZE));
  return DOWNSAMPLESIZE;
}

void soundAndroid::flushCapture()
{
  capRing.discardAll();
}

// ---------------------------------------------------------------------------------------------------------------------
// playback

oboe::DataCallbackResult soundAndroid::outputReady(qint16 *data,int numFrames)
{
  const size_t samples=size_t(numFrames)*size_t(outChannels);
  const size_t got=playRing.read(data,samples);
  if(got<samples)
    {
      std::memset(data+got,0,(samples-got)*sizeof(qint16));   // silence, never a repeated block
      underruns++;
    }
  if(writerWaiting.load(std::memory_order_acquire)) spaceCond.wakeOne();
  return oboe::DataCallbackResult::Continue;
}

void soundAndroid::startOutput()
{
  if(!outStream || outRunning) return;
  const oboe::Result r=outStream->requestStart();
  if(r!=oboe::Result::OK)
    {
      errorHandler("Audio playback error",QString::fromUtf8(oboe::convertToText(r)));
      return;
    }
  outRunning=true;
  ALOG("speaker started");
}

void soundAndroid::stopOutput()
{
  if(!outStream || !outRunning) return;
  outStream->requestStop();
  oboe::StreamState next=oboe::StreamState::Stopping;
  outStream->waitForStateChange(oboe::StreamState::Stopping,&next,500*oboe::kNanosPerMillisecond);
  outRunning=false;
}

void soundAndroid::flushPlayback()
{
  // A new transmission: the speaker stream is stopped between transmissions, so nothing is reading the ring now
  stopOutput();
  playRing.reset();
  underruns=0;
}

int soundAndroid::write(uint numFrames)
{
  if(!soundDriverOK || !outStream) return 0;
  if(numFrames==0) return 0;
  if(numFrames>uint(DOWNSAMPLESIZE)) numFrames=DOWNSAMPLESIZE;
  // A frame is two 16-bit samples in one quint32, left in the low half. Only one channel is used for a mono output: the audio
  // is in the low half, or in the high half when the channels are swapped (see synthesizer::...).
  const bool audioInHigh=swapChannel && !duplicateChannel;
  for(uint i=0;i<numFrames;i++)
    {
      const quint32 f=tempTXBuffer[i];
      const qint16 lo=qint16(f&0xFFFF);
      const qint16 hi=qint16(f>>16);
      if(outChannels==1) txConv[i]=audioInHigh ? hi : lo;
      else
        {
          txConv[2*size_t(i)]=lo;
          txConv[2*size_t(i)+1]=hi;
        }
    }
  const size_t total=size_t(numFrames)*size_t(outChannels);
  size_t done=0;
  QElapsedTimer stall;
  stall.start();
  // The sound thread ends the playback when the TX buffer is found empty (soundBase::play()): with a sound card that blocks it
  // cannot get far ahead of the thread that makes the audio, but a ring this size lets it empty the buffer of the preamble and
  // the VIS code into the ring in a moment and find nothing there before the first line of the picture is made, and the rest of
  // the transmission is thrown away (the picture was "sent" in 3 seconds and nothing of it was heard). So the speaker's ring is
  // only filled to half a second, which the callback needs as a reserve, and no more.
  const size_t highWater=size_t(BASESAMPLERATE/2)*size_t(outChannels);
  while(done<total)
    {
      if(stopThread || closing) return -1;
      const size_t queued=playRing.readable();
      const size_t room=queued>=highWater ? 0 : highWater-queued;
      const size_t w=room>0 ? playRing.write(txConv.data()+done,std::min(total-done,room)) : 0;
      if(w>0)
        {
          done+=w;
          stall.restart();
          // two blocks are queued before the speaker starts, so the first callbacks cannot run dry
          if(!outRunning && playRing.readable()>=size_t(DOWNSAMPLESIZE)*2*size_t(outChannels)) startOutput();
          continue;
        }
      if(!outRunning) startOutput();   // the ring is full but nothing is draining it
      if(stall.elapsed()>2000)
        {
          errorHandler("Audio playback error","Audio output stalled");
          return -1;
        }
      writerWaiting.store(true,std::memory_order_release);
      {
        QMutexLocker l(&waitMutex);
        if(playRing.readable()>=highWater && !stopThread) spaceCond.wait(&waitMutex,WAITMS);
      }
      writerWaiting.store(false,std::memory_order_release);
    }
  return int(numFrames);
}

void soundAndroid::waitPlaybackEnd()
{
  if(!outStream) return;
  // a short transmission may not have reached the two-block start mark
  if(!outRunning && playRing.readable()>0) startOutput();
  // Everything handed to the ring has to be played before the PTT is released, so unlike the Qt backend this does wait
  QElapsedTimer t;
  t.start();
  while(playRing.readable()>0 && !stopThread && !closing && t.elapsed()<4000)
    {
      writerWaiting.store(true,std::memory_order_release);
      {
        QMutexLocker l(&waitMutex);
        if(playRing.readable()>0) spaceCond.wait(&waitMutex,WAITMS);
      }
      writerWaiting.store(false,std::memory_order_release);
    }
  const unsigned duringPlayback=underruns.load();   // what follows is the speaker's own tail, silence by design
  // and the last buffers are still on their way through the hardware
  if(!stopThread && !closing)
    {
      int ms=150;
      const oboe::ResultWithValue<double> latency=outStream->calculateLatencyMillis();
      if(latency) ms=int(std::min(500.0,std::max(50.0,latency.value()+50.0)));
      msleep(ms);
    }
  stopOutput();
  if(focusHeld)
    {
      androidAbandonTransmitFocus();
      focusHeld=false;
    }
  ALOG("speaker stopped, %u underruns during playback",duringPlayback);
  if(duringPlayback>0) addToLog(QString("playback underruns: %1").arg(duringPlayback),LOGSOUND);
}

// ---------------------------------------------------------------------------------------------------------------------

void soundAndroid::closeDevices()
{
  closing=true;
  stopInput();
  stopOutput();
  if(focusHeld)
    {
      androidAbandonTransmitFocus();
      focusHeld=false;
    }
  if(inStream) {inStream->close(); inStream.reset();}
  if(outStream) {outStream->close(); outStream.reset();}
  soundDriverOK=false;
  closing=false;
}

void soundAndroid::streamFailed(const QString &which,oboe::Result r)
{
  if(closing) return;   // our own close
  const QString why=QString("%1: %2").arg(which,QString::fromUtf8(oboe::convertToText(r)));
  addToLog("Android audio stream lost, "+why,LOGSOUND);
  ALOG("stream lost, %s",qPrintable(why));
  // from an audio thread to the GUI thread, where mainWindow::recoverSound() rebuilds the backend
  QMetaObject::invokeMethod(this,[this,why]{emit deviceLost(why);},Qt::QueuedConnection);
}
