#ifndef ANDROIDAUDIO_H
#define ANDROIDAUDIO_H

#include <QList>
#include <QString>

/*!
  What Android's audio manager knows about the sound hardware (packaging/android/src/.../AudioHelper.java). Android builds only.
*/
struct androidAudioDevice
{
  int id=0;                  //!< the number Oboe's setDeviceId() takes; changes when the device is plugged in anew
  int type=0;                //!< AudioDeviceInfo.TYPE_*
  QString description;       //!< what the user sees and what is saved: "Phone speaker", "USB audio: Digirig Mobile"
  bool builtIn=false;        //!< part of the phone itself: its speaker, earpiece or microphone
  bool narrowband=false;     //!< Bluetooth call audio: 8 or 16 kHz, no use for receiving a picture
};

QList<androidAudioDevice> androidAudioDevices(bool inputs);
//! The id of the device saved under this name, or 0 (let the system choose) for "default", an empty name or a device that is gone
int androidDeviceId(const QString &savedName,bool inputs);
bool androidIsBuiltInSpeaker(int outputDeviceId);   //!< a stereo tone on the second channel would be summed into the picture
int androidMediaVolumePercent();
void androidSetMediaVolumePercent(int percent);
//! The volume keys of the phone change the media volume while the program is in front (otherwise the ringer's, when nothing plays)
void androidVolumeKeysChangeMedia();
//! The screen stays on while a picture is sent (and may go off again afterwards)
void androidKeepScreenOn(bool on);
void androidDarkSystemBarIcons(bool dark);   //!< the icons of the status and navigation bars: dark for a light screen
void androidHaptic(int kind);   //!< a short vibration: 0 tick, 1 confirmation, 2 refusal
//! "media 7/15, ring 4/7", for the log
QString androidVolumeSummary();
void androidRequestTransmitFocus();
void androidAbandonTransmitFocus();
void androidRequestNotificationPermission();
void androidOpenAppSettings();

#endif // ANDROIDAUDIO_H
