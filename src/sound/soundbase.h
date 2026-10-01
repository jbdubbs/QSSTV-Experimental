#ifndef SOUNDBASE_H
#define SOUNDBASE_H

#include "appglobal.h"
#include "wavio.h"
#include "wavreader.h"
#include "buffermanag.h"
#include "downsamplefilter.h"


#include <QThread>
#include <QMutex>

#define BYTESPOWER 18


//WWV WWVH             2500.0, 5000.0, 10000.0

//·      GBR                           60.0 kHz

//·      RWM                          4996.0, 9996.0, 14996.0

//·      CHU                            7335.0



#define PERIODSIZE (DOWNSAMPLESIZE)
#define BUFFERSIZE (8*DOWNSAMPLESIZE)
#define CALIBRATIONSIZE (PERIODSIZE)
#define CALIBRATIONLEADIN 80


class soundBase : public QThread
{
  Q_OBJECT

public:
  enum edataSrc{SNDINCARD,SNDINFROMFILE,SNDINCARDTOFILE};
  enum edataDst{SNDOUTCARD,SNDOUTTOFILE};
  enum eplaybackState{PBINIT,PBSTARTING,PBRUNNING,PBCALIBRATESTART,PBCALIBRATEWAIT,PBCALIBRATE,PBEND};
  enum ecaptureState{CPINIT,CPSTARTING,CPRUNNING,CPCALIBRATESTART,CPCALIBRATEWAIT,CPCALIBRATE,CPEND};

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

  bool calibrate(bool isCapture);
  bool calibrationCount(unsigned int &frames, double &elapsedTime);
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
  int captureCalibration(bool leadIn);
  int playbackCalibration(bool leadIn);
  QMutex mutex;
  QElapsedTimer stopwatch;
  unsigned int calibrationFrames;
  unsigned int leadInCounter;
  int calibrationTime;
  double ucalibrationTime;
  double ustartcalibrationTime;
  struct timespec ts;
  QString lastErrorStr;
  quint64 storedFrames;
  bool prebuf;
  unsigned int prevFrames;


};

#endif // SOUNDBASE_H
