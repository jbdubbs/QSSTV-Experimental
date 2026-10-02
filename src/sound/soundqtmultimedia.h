#ifndef SOUNDQTMULTIMEDIA_H
#define SOUNDQTMULTIMEDIA_H

// Cross-platform audio backend, replacing soundPulse (PulseAudio) and soundAlsa (ALSA) --
// see Phase 4 of the cross-platform plan. Qt Multimedia maps to WASAPI on Windows,
// CoreAudio on macOS, and PulseAudio/PipeWire/ALSA on Linux, so all three OSes now go
// through the same code instead of two Linux/macOS-only backends plus a would-be third
// one for Windows.

#include "soundbase.h"

#include <QAudioSource>
#include <QAudioSink>
#include <QIODevice>

// Matches soundalsa.h's free-function convention: config/soundconfig.cpp calls this
// directly (not through soundBase's own unused virtual getCardList()) to populate the
// input/output device combo boxes. Descriptions only -- see soundqtmultimedia.cpp's
// findAudioDevice() for how a saved description is matched back to a QAudioDevice.
void getCardList(QStringList &inputList, QStringList &outputList);

class soundQtMultimedia : public soundBase
{
public:
  soundQtMultimedia();
  ~soundQtMultimedia();
  bool init(int samplerate);
  int read(int &countAvailable);
  int write(uint numFrames);
protected:
  void flushCapture();
  void flushPlayback();
  void closeDevices();
  void waitPlaybackEnd();
private:
  QAudioSource *audioSourcePtr;
  QAudioSink *audioSinkPtr;
  qint64 rxFilled=0;            // bytes of the current RX block already read into tempRXBuffer
  QIODevice *captureDevicePtr;  // owned by audioSourcePtr, not deleted directly
  QIODevice *playbackDevicePtr; // owned by audioSinkPtr, not deleted directly
};
#endif // SOUNDQTMULTIMEDIA_H
