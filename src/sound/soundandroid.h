#ifndef SOUNDANDROID_H
#define SOUNDANDROID_H

// The Android sound backend: AAudio through Oboe, behind the same soundBase interface as soundQtMultimedia.
//
// Why not Qt Multimedia here: QAudioSource has no way to choose the microphone input preset (Android's default
// processing -- automatic gain control, noise suppression -- pumps and distorts the 1200-2300 Hz tones and the sync pulses),
// and the desktop backend's way of working, a worker thread polling a QIODevice that belongs to the GUI thread a thousand
// times a second, suits neither the Android audio threads nor the battery. Here the audio callbacks only move samples to
// and from two lock-free rings; the sound thread reads and writes the rings, and sleeps until a full block is there.
//
// Devices are listed by Qt (QMediaDevices on Android reports the Android device ids and hot-plug changes), so the list in
// the configuration and the saved device names are the same as with the Qt backend; only the streams are native.

#include "soundbase.h"
#include "samplering.h"

#include <oboe/Oboe.h>

#include <atomic>
#include <memory>
#include <vector>

#include <QMutex>
#include <QWaitCondition>

class soundAndroid : public soundBase
{
public:
  soundAndroid();
  ~soundAndroid();
  bool init(int samplerate);
  int read(int &countAvailable);
  int write(uint numFrames);

  //! A description of what was opened (device ids, rate, preset), for the log and the Sound page
  QString routeSummary() const override {return route;}
  //! Input samples that reached full scale (or were clipped by the digital gain) since the last call
  int takeClipCount() {return clips.exchange(0);}
  //! The microphone delivers nothing for a while, or is overloaded (see soundBase::inputProblem())
  QString inputProblem();
  //! Live change of the digital gain (dB), without reopening the stream
  void setRxGainDb(double db) override;

protected:
  void flushCapture();
  void flushPlayback();
  void prepareCapture();
  void preparePlayback();
  void closeDevices();
  void waitPlaybackEnd();
  void idleTick();

private:
  class dataCallback;
  class errorCallback;
  friend class dataCallback;
  friend class errorCallback;

  QString inputProblemNow();
  bool openInput(int deviceId,QString &why);
  bool openOutput(int deviceId,QString &why);
  void startInput();
  void stopInput();
  void startOutput();
  void stopOutput();
  oboe::DataCallbackResult inputReady(const qint16 *data,int numFrames);
  oboe::DataCallbackResult outputReady(qint16 *data,int numFrames);
  void streamFailed(const QString &which,oboe::Result r);

  std::shared_ptr<oboe::AudioStream> inStream;
  std::shared_ptr<oboe::AudioStream> outStream;
  std::shared_ptr<dataCallback> inCallback;
  std::shared_ptr<dataCallback> outCallback;
  std::shared_ptr<errorCallback> errCallback;
  bool inRunning;
  bool outRunning;
  int outChannels;               // 1 (the speaker and most radio interfaces) or 2 (a PTT tone on the other channel)
  std::atomic<bool> closing;
  int idleTicks;
  QString route;

  sampleRing capRing;            // microphone -> sound thread
  sampleRing playRing;           // sound thread -> speaker
  std::vector<qint16> txConv;    // one TX block in the sample layout of the output stream
  QMutex waitMutex;
  QWaitCondition dataCond;       // a block of input is complete
  QWaitCondition spaceCond;      // the output callback made room
  std::atomic<bool> readerWaiting;
  std::atomic<bool> writerWaiting;
  std::atomic<int> gainQ8;       // digital gain, 256 = 1.0
  std::atomic<int> clips;
  std::atomic<unsigned> overruns;
  std::atomic<unsigned> underruns;
  std::atomic<int> peak;           // the largest input sample since the level was last logged
  std::atomic<unsigned> zeroRun;   // input samples in a row that are exactly zero: what a muted or blocked microphone delivers
  bool focusHeld;                  // audio focus is held for the transmission in progress
};

#endif // SOUNDANDROID_H
