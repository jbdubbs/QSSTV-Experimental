#ifndef SOUNDBACKEND_H
#define SOUNDBACKEND_H

class soundBase;

/*!
  The sound backend of this platform: Qt Multimedia everywhere (WASAPI, CoreAudio, PulseAudio/PipeWire/ALSA), and the
  native Android backend (AAudio through Oboe) on Android, where Qt's QAudioSource cannot choose the microphone input
  preset and polling its QIODevice from the sound thread does not suit the Android audio threads.
*/
soundBase *newSoundBackend();

#endif // SOUNDBACKEND_H
