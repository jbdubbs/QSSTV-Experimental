# QSSTV-Experimental

QSSTV-Experimental is an amateur-radio program for sending and receiving **SSTV** (slow-scan television) and **HAMDRM / DSSTV**
(digital SSTV) pictures using a sound card. It is compatible with most of MMSSTV and EasyPal.

It is a fork of **QSSTV 9.5.11**, written by **Johan Maes, ON4QZ** (copyright 2000-2019, <https://www.qsl.net/o/on4qz>).
The HAMDRM software in QSSTV is based on RX/TXAMADRM by **PA0MBO**. Further contributions to the Qt6/macOS/FreeBSD
porting and fixes came from Jörg Wunsch (DL8DTL), Daniele Forsi, hspil, Ruxton, AsciiWolf, Barry de Graaff, Greg Tangey and
Sam Saccone. This program is based in part on QSSTV and, like it, is released under the GNU GPL v3 (see `COPYING` and `LICENSE`).

## Features

- Receive and transmit SSTV in the common mode families: Martin, Scottie, Robot, PD, Wraase SC2, Pasokon, AVT, BW, FAX480, and the MP/MR/ML/MC families
- HAMDRM (DSSTV) receive and transmit
- Image gallery for received and transmitted pictures
- Image viewer for gallery pictures
- TX image editor with templates, macros, text and image items; stock and template libraries
- Image import and export in PNG, JPEG, GIF, BMP and JPEG 2000
- Rig control through Hamlib, CAT, serial (RTS/DTR) and VOX PTT, and XML-RPC remote control
- CW identification, FSK ID, and a waterfall / spectrum / scope display

## Changes versus upstream QSSTV 9.5.11

### Platforms

- **Windows** (64-bit, 10 1809 or later), in addition to Linux x86 and arm64
- **Self-contained Linux AppImage** that bundles Qt6 and every runtime dependency
- **macOS** (Not yet available - lack of hardware)

### Features

- **Decode SSTV from audio files** (WAV, MP3, FLAC, OGG, AAC), from the GUI (*File > Decode SSTV From File...*) at about 10x real time, or from the command line with `qsstv --batch`, headless, with scriptable exit codes
- **New SSTV modes:** *JB60*, a ~61 s half-time color mode, and *PD120W*, a 16:9 widescreen PD120
- **Sample-rate calibration** from WWV time ticks, NTP, or the slant of a received SSTV picture
- **Stitched grid images:** transmit large pictures as 1x2, 2x1 or 2x2 grids on the larger modes
- **Gallery multi-select** to delete or send several images to TX, with the Del key for quick deletion
- **Image viewer zoom and pan:** smooth mouse-wheel zoom and drag-to-pan
- **Clipboard support:** copy images from anywhere, paste into the transmit window or TX stock
- **WebP, AVIF and HEIC** image formats, with EXIF/HEIF orientation applied on import
- **ADIF logbook broadcast over UDP**, on every OS
- **Smooth display scaling** option for the RX canvas
- **DRM MER readout** (formerly labelled "SNR") plus an MSC MER, with tooltips on the MER and Time/Frame/FAC/MSC indicators
- **Live DRM size-slider preview:** the picture preview updates as the slider is dragged

### Changes

1. **Receiver and transmitter robustness.** Audio recovers after a device change or Windows sleep/resume; the QSSTV Engine TX tone level is raised to match MMSSTV; end-of-image footer tones are sent on transmit; and TX aborts cleanly when playback cannot start.
2. **DRM fixes.** Several long-standing HAMDRM receive bugs were found with a new loopback test harness and fixed: ring-buffer copies at the wrap, a Qt6 MOT-header pointer bug, and the mode-A 2.3 kHz boosted-pilot table.
3. **Smaller usability fixes.** Working File > TX Test Pattern in release builds with new patterns, Yes/No quit confirmation, the other mode's tabs locked out during TX/RX, a toolbar Edit button, new editor items placed in the centre of the view, larger Papirus toolbar and gallery icons, and a clearer Save Waterfall Image.
4. **Qt6 and CMake.** Ported from Qt5/qmake to Qt6/CMake.
5. **Audio, webcam and serial through Qt.** PulseAudio, ALSA and V4L2 are replaced by Qt Multimedia, and serial PTT uses QSerialPort, so one code path serves audio, webcam and PTT on every OS.
6. **Logbook IPC.** The SysV shared-memory logbook link is replaced by ADIF over UDP.
7. **Portable paths and code.** Hard-coded Unix paths replaced with Qt equivalents; assorted Qt6, macOS and FreeBSD compatibility fixes; a vendored Hamlib on Windows; a reproducible Ubuntu 24.04 container build with official Qt 6.11 for the AppImage.
8. **Rebranded** as QSSTV-Experimental (the original copyright and attribution are retained throughout the code and About box).

## Download a pre-built release

Ready-to-run builds are published on the [Releases page](https://github.com/jbdubbs/QSSTV-Experimental/releases):

- **Linux:** download the `.AppImage`, make it executable (`chmod +x QSSTV-Experimental-*.AppImage`) and run it. It bundles Qt6 and its other runtime dependencies.
- **Windows:** download the `-win64.zip`, extract it anywhere and run `qsstv.exe`.
- **macOS:** (Not yet available - lack of hardware)

## Building

See [BUILDING.md](BUILDING.md) for building from source on Linux and Windows, the AppImage build, and the image-format plugin requirements.
