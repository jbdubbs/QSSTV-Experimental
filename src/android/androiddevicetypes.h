#ifndef ANDROIDDEVICETYPES_H
#define ANDROIDDEVICETYPES_H

// Which of the audio devices Android lists can carry an SSTV or DRM signal, by AudioDeviceInfo.getType(). Plain numbers and no
// Android headers, so tests/androiddevices checks it on the desktop.
//
// The system lists more than a person can use: the telephone call path, the buffer other apps' sound is captured from (remote
// submix), tuners, IP and bus devices, the echo reference, the hearing aid. They are left out of the lists, so a first choice in
// Settings cannot be one that opens and stays silent.

inline bool androidDeviceUsable(int type,bool input)
{
  switch(type)
    {
    case 3:    // wired headset (a headset with a microphone, on the 3.5 mm jack)
    case 5:    // analog line
    case 6:    // digital line
    case 7:    // Bluetooth SCO: the call profile, narrow (8 or 16 kHz): listed, marked unsuitable for receiving by the program
    case 11:   // USB device (a sound card, a Digirig)
    case 12:   // USB accessory
    case 13:   // dock
    case 19:   // aux line
    case 22:   // USB headset
    case 26:   // Bluetooth LE headset
    case 31:   // analog dock
      return true;
    case 15:   // the phone's own microphone
      return input;
    case 1:    // earpiece
    case 2:    // loudspeaker
    case 4:    // wired headphones
    case 8:    // Bluetooth A2DP (music: good for sending, there is no microphone on it)
    case 24:   // protected loudspeaker
    case 27:   // Bluetooth LE speaker
      return !input;
    default:   // telephony 18, remote submix 25, FM 14, FM tuner 16, TV tuner 17, IP 20, bus 21, hearing aid 23, echo reference 28,
               // HDMI 9 10 29, broadcast 30, unknown 0
      return false;
    }
}

#endif // ANDROIDDEVICETYPES_H
