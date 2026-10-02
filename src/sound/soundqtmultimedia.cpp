#include "soundqtmultimedia.h"
#include "configparams.h" // inputAudioDevice/outputAudioDevice (via soundconfig.h)

#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioFormat>

// The persisted inputAudioDevice/outputAudioDevice strings (config/soundconfig.cpp) are
// QAudioDevice::description() text, matching the combo box's plain display text via the
// existing generic getValue/setValue(QString&, QComboBox*) helpers (supportfunctions.cpp)
// -- no bespoke lookup code needed there. "default" (soundconfig.cpp's own default
// QSettings value, predating this backend) and an empty/no-longer-present device both
// fall back to the platform default, which is a strictly better failure mode than the
// old ALSA backend's hard failure on an invalid saved device name.
static QAudioDevice findAudioDevice(const QString &saved, const QList<QAudioDevice> &devices,
                                     const QAudioDevice &fallback)
{
  if(saved.isEmpty() || saved=="default") return fallback;
  for(const QAudioDevice &d : devices)
    if(d.description()==saved) return d;
  return fallback;
}

// QAudioSource/QAudioSink report failures via error() (a QAudio::Error enum), unlike
// QIODevice's errorString(); stringify it ourselves for errorHandler()'s free-text log.
static QString audioErrorString(QAudio::Error e)
{
  switch(e)
    {
    case QAudio::NoError: return "no error";
    case QAudio::OpenError: return "the audio device could not be opened";
    case QAudio::IOError: return "an I/O error occurred";
    case QAudio::FatalError: return "a fatal error occurred";
    default: return "unknown audio error";
    }
}

void getCardList(QStringList &inputList, QStringList &outputList)
{
  inputList.clear();
  outputList.clear();
  const auto inputs=QMediaDevices::audioInputs();
  for(const QAudioDevice &d : inputs) inputList.append(d.description());
  const auto outputs=QMediaDevices::audioOutputs();
  for(const QAudioDevice &d : outputs) outputList.append(d.description());
}

soundQtMultimedia::soundQtMultimedia()
{
  audioSourcePtr=nullptr;
  audioSinkPtr=nullptr;
  captureDevicePtr=nullptr;
  playbackDevicePtr=nullptr;
}

soundQtMultimedia::~soundQtMultimedia()
{
  closeDevices();
}

bool soundQtMultimedia::init(int samplerate)
{
  soundDriverOK=false;
  sampleRate=samplerate;

  QAudioDevice inDev=findAudioDevice(inputAudioDevice,QMediaDevices::audioInputs(),
                                      QMediaDevices::defaultAudioInput());
  QAudioDevice outDev=findAudioDevice(outputAudioDevice,QMediaDevices::audioOutputs(),
                                       QMediaDevices::defaultAudioOutput());

  // Capture mono, same as soundPulse (PulseAudio does the downmix if the hardware is
  // stereo) rather than soundAlsa's "open stereo and downmix ourselves" fallback -- one
  // less thing for this backend to get wrong, and every current Qt Multimedia backend
  // handles the channel conversion internally.
  QAudioFormat inFormat;
  inFormat.setSampleRate(sampleRate);
  inFormat.setChannelCount(MONOCHANNEL);
  inFormat.setSampleFormat(QAudioFormat::Int16);
  // isFormatSupported() is only advisory here, not a real capability probe: it can say no
  // for a format the device (or the backend underneath it) will happily deliver once
  // actually opened. Concretely, under Wine, mmdevapi/winepulse cache a device's WASAPI
  // engine mix format -- always 32-bit float, since PipeWire's internal graph is float --
  // and reject any request that doesn't match it exactly, even though the real Windows
  // WASAPI shared-mode engine (and PulseAudio itself, once a stream is opened) freely
  // converts sample format for you. So log a mismatch instead of treating it as fatal, and
  // let the QAudioSource::start() result below -- an actual open attempt, not a metadata
  // query -- be what decides whether this is really a failure.
  if(!inDev.isFormatSupported(inFormat))
    addToLog(QString("%1 reports no support for %2 Hz mono 16-bit capture; trying anyway")
             .arg(inDev.description()).arg(sampleRate),LOGSOUND);

  // Playback is stereo -- tempTXBuffer packs one interleaved L+R Int16 pair per quint32
  // entry (see soundbase.h), matching what soundPulse already sends.
  QAudioFormat outFormat;
  outFormat.setSampleRate(sampleRate);
  outFormat.setChannelCount(STEREOCHANNEL);
  outFormat.setSampleFormat(QAudioFormat::Int16);
  if(!outDev.isFormatSupported(outFormat))
    addToLog(QString("%1 reports no support for %2 Hz stereo 16-bit playback; trying anyway")
             .arg(outDev.description()).arg(sampleRate),LOGSOUND);

  delete audioSourcePtr; // restartSound() calls init() again on an already-initialized backend
  delete audioSinkPtr;
  captureDevicePtr=nullptr;
  playbackDevicePtr=nullptr;
  rxFilled=0;

  audioSourcePtr=new QAudioSource(inDev,inFormat,this);
  audioSinkPtr=new QAudioSink(outDev,outFormat,this);

  // See soundbase.h's deviceLost() doc comment. error()!=NoError excludes our own
  // clean stop()s (flushPlayback()'s stop()/start() cycle, closeDevices()'s teardown)
  // from being mistaken for an unexpected device loss -- those always leave error()
  // at NoError.
  connect(audioSourcePtr,&QAudioSource::stateChanged,this,[this](QAudio::State state)
    {
      if(state==QAudio::StoppedState && audioSourcePtr->error()!=QAudio::NoError)
        emit deviceLost(QString("capture: %1").arg(audioErrorString(audioSourcePtr->error())));
    });
  connect(audioSinkPtr,&QAudioSink::stateChanged,this,[this](QAudio::State state)
    {
      if(state==QAudio::StoppedState && audioSinkPtr->error()!=QAudio::NoError)
        emit deviceLost(QString("playback: %1").arg(audioErrorString(audioSinkPtr->error())));
    });

  captureDevicePtr=audioSourcePtr->start();
  playbackDevicePtr=audioSinkPtr->start();
  if(!captureDevicePtr || !playbackDevicePtr)
    {
      errorHandler("Audio init error","Unable to start the audio input/output device");
      return false;
    }

  isStereo=false;
  soundDriverOK=true;
  return true;
}

int soundQtMultimedia::read(int &countAvailable)
{
  if(!soundDriverOK || !captureDevicePtr) return 0;
  const qint64 wanted=qint64(DOWNSAMPLESIZE)*sizeof(qint16);
  // Don't gate on bytesAvailable(): Qt 6.4's PulseAudio capture device (what Ubuntu 24.04's
  // Qt, and so the AppImage, ships) reports 0 there in pull mode even while read() delivers
  // data, so waiting for it to reach a full block never reads anything. read() is
  // non-blocking everywhere (0 = nothing yet), so accumulate partial reads until a block fills.
  qint64 got=captureDevicePtr->read(((char*)tempRXBuffer)+rxFilled,wanted-rxFilled);
  if(got<0)
    {
      rxFilled=0;
      errorHandler("Audio capture error",audioErrorString(audioSourcePtr->error()));
      return -1;
    }
  rxFilled+=got;
  countAvailable=int(rxFilled);
  if(rxFilled<wanted) return 0; // not a full block yet -- soundBase::run()'s own polling loop retries
  rxFilled=0;
  return DOWNSAMPLESIZE;
}

int soundQtMultimedia::write(uint numFrames)
{
  if(!soundDriverOK || !playbackDevicePtr) return 0;
  if(numFrames==0) return 0;
  const char *p=(const char*)tempTXBuffer;
  qint64 remaining=qint64(numFrames)*sizeof(quint32);
  // QIODevice::write() on a QAudioSink's pull-mode device only accepts what currently
  // fits in its internal buffer, returning less (or 0) rather than growing unbounded.
  // Retrying until everything is accepted is what gives this the same real-time pacing
  // pa_simple_write's blocking behavior provided for soundPulse -- SSTV TX timing depends
  // on write() not returning until a device has genuinely consumed a full block.
  while(remaining>0)
    {
      qint64 written=playbackDevicePtr->write(p,remaining);
      if(written<0)
        {
          errorHandler("Audio playback error",audioErrorString(audioSinkPtr->error()));
          return -1;
        }
      if(written==0)
        {
          msleep(1);
          continue;
        }
      p+=written;
      remaining-=written;
    }
  return numFrames;
}

void soundQtMultimedia::flushCapture()
{
  rxFilled=0;
  if(captureDevicePtr) captureDevicePtr->readAll(); // discard whatever's buffered
}

void soundQtMultimedia::flushPlayback()
{
  // Stop and restart rather than QAudioSink::reset() (which also leaves the sink
  // stopped): guarantees a clean, known state -- a fresh QIODevice -- regardless of
  // exactly which internal state reset() would have left it in.
  if(!audioSinkPtr) return;
  audioSinkPtr->stop();
  playbackDevicePtr=audioSinkPtr->start();
}

void soundQtMultimedia::closeDevices()
{
  if(audioSourcePtr)
    {
      audioSourcePtr->stop();
      delete audioSourcePtr;
      audioSourcePtr=nullptr;
    }
  if(audioSinkPtr)
    {
      audioSinkPtr->stop();
      delete audioSinkPtr;
      audioSinkPtr=nullptr;
    }
  captureDevicePtr=nullptr;
  playbackDevicePtr=nullptr;
}

void soundQtMultimedia::waitPlaybackEnd()
{
  // soundPulse's equivalent is a no-op too: write()'s own blocking/pacing has already
  // ensured every buffered sample was handed to the device by the time playback stops.
}
