#ifndef SOUNDBASE_H
#define SOUNDBASE_H

#include "appglobal.h"
#include "wavio.h"
#include "wavreader.h"
#include "buffermanag.h"
#include "downsamplefilter.h"


#include <QThread>
#include <QMutex>
#include <vector>

#define BYTESPOWER 18


//WWV WWVH             2500.0, 5000.0, 10000.0

//·      GBR                           60.0 kHz

//·      RWM                          4996.0, 9996.0, 14996.0

//·      CHU                            7335.0



#define PERIODSIZE (DOWNSAMPLESIZE)
#define BUFFERSIZE (8*DOWNSAMPLESIZE)


class soundBase : public QThread
{
  Q_OBJECT

public:
  enum edataSrc{SNDINCARD,SNDINFROMFILE,SNDINCARDTOFILE};
  enum edataDst{SNDOUTCARD,SNDOUTTOFILE};
  enum eplaybackState{PBINIT,PBSTARTING,PBRUNNING,PBEND};
  enum ecaptureState{CPINIT,CPSTARTING,CPRUNNING,CPLISTENSTART,CPLISTEN,CPEND};

  // Command line --encode: when set, transmit audio goes to this wav file (no dialog, no sound card, not paced to real time)
  static QString txFileName;
  static volatile bool txFileDone;   // set by txFunctions::waitEnd() once the TX thread has queued its last sample

  explicit soundBase(QObject *parent = 0);
  ~soundBase();
  virtual bool init(int samplerate)=0;
  void run();
  void idleRX();
  void idleTX();
  void stopSoundThread();
  virtual void getCardList() {;}

  bool startCapture();
  bool startPlayback();

  // "Decode from file": a one-shot file source, independent of the persisted soundRoutingInput.
  // The recording is read as mono 48 kHz (see wavReader), followed by a few seconds of low level noise so the
  // receiver sees the signal disappear (which is what saves a picture that was still being received).
  bool startFileCapture(const QString &path,bool realtimePacing,QString &error);
  void clearFileSource();
  bool fileSourceActive() const {return fileSource;}
  bool fileDecodeFinished() const {return fileSource && fileEof && captureState==CPINIT;}
  bool fileSourceCancelled() const {return fileCancelled;}   //!< the capture was stopped before the file was finished
  int fileProgressPercent() const {return fileReader.progressPercent();}
  buffer<FILTERPARAMTYPE,BYTESPOWER> rxBuffer;
  buffer<FILTERPARAMTYPE,BYTESPOWER> rxVolumeBuffer;
  // mmsstv-linux-port Step 7: raw, un-decimated capture samples (same
  // DOWNSAMPLESIZE-per-call cadence as rxBuffer's decimated push, just not
  // yet run through downsampleFilterPtr->downSample4()). Populated
  // alongside rxBuffer in capture(); mmsstv-core's CSSTVDEM needs genuine
  // raw audio, not QSSTV's own already-demodulated/decimated stream. Only
  // meaningfully drained when a mode has ENGINE_MMSSTV_CORE selected (see
  // sstv/engineselection.h) -- otherwise it just accumulates and gets
  // reset like any other unread ring buffer.
  buffer<FILTERPARAMTYPE,BYTESPOWER> rawRxBuffer;
  buffer<SOUNDFRAME,16> txBuffer;
  double getVolumeDb(){return volume;}
  FILTERPARAMTYPE *getVolumePtr() {return downsampleFilterPtr->getVolumePtr();}
  const QString getLastError() { return lastErrorStr;}
  bool isPlaying() {return playbackState!=PBINIT;}
  bool isCapturing() {return captureState!=CPINIT;}

  // "Listen" mode (Options > Calibrate): capture from the soundcard and put every raw sample in rawRxBuffer,
  // without any decoding. The caller drains rawRxBuffer; if it can't keep up samples are dropped and
  // listenOverruns() goes up (the caller must then restart its measurement, the sample timeline has a hole).
  bool startListen();
  void stopListen();
  unsigned int listenOverruns() const {return overruns;}
  int streamSampleRate() const {return sampleRate;}   //!< rate of the samples the application receives
  /** One entry per block captured in listen mode: total frames received so far and the raw monotonic clock
      (rawMonotonicSeconds()) when the block was complete. Used by the NTP calibration, independent of rawRxBuffer. */
  struct listenStamp {quint64 frames; double time;};
  void takeListenStamps(std::vector<listenStamp> &out);
  int countAvailable;
signals:
  // Emitted (soundQtMultimedia only, see init()) when the underlying audio stream
  // stops on its own with a real error rather than via our own stop()/closeDevices()
  // -- e.g. a device disappearing mid-session. Windows system sleep is the motivating
  // case (mainwindow.cpp's nativeEvent() handles that one directly via
  // WM_POWERBROADCAST, since that's authoritative regardless of what the backend
  // reports; this is the cross-platform fallback/diagnostic in case it ever does
  // report something, or for any other unexpected mid-session device loss).
  void deviceLost(const QString &reason);

public slots:

protected:
  bool soundDriverOK;
  bool isStereo;
  int capture();
  int play();

  virtual int read(int &countAvailable)=0;
  virtual int write(uint numFrames)=0;
  virtual void flushCapture()=0;
  virtual void flushPlayback()=0;
  virtual void prepareCapture() {;}
  virtual void preparePlayback() {;}
  virtual void closeDevices()=0;
  virtual void waitPlaybackEnd()=0;


  int sampleRate;
  qint16 tempRXBuffer[DOWNSAMPLESIZE*2*2]; // in some cases the hardware interface is stereo (can be S16_LE or S32_LE)
  quint32 tempTXBuffer[DOWNSAMPLESIZE*2];
  bool stopThread;
  eplaybackState playbackState;
  ecaptureState  captureState;


  wavIO waveIn;
  wavIO waveOut;
  wavReader fileReader;
  bool fileSource;
  bool fileEof;
  bool fileCancelled;
  bool filePaced;
  long fileTailLeft;
  unsigned int fileNoiseState;
  int readFileBlock(bool &endAfterBlock);
  void errorHandler(QString title,QString info);
  void switchCaptureState(ecaptureState cs);
  void switchPlaybackState(eplaybackState ps);



private:
  downsampleFilter *downsampleFilterPtr;
  double volume;
//  uint intVolume;
  int captureListen();
  volatile unsigned int overruns;
  QMutex stampMutex;
  std::vector<listenStamp> stamps;
  quint64 stampFrames;
  QString lastErrorStr;
  quint64 storedFrames;
  bool prebuf;


};

#endif // SOUNDBASE_H
