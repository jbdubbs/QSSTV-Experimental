# Release notes of the Android app

Drafts for the first release that carries the Android app. The version number is not set here: it is the program version at the time of the
release (`MAJORVERSION`/`MINORVERSION`, see PLAY.md section 3), so write it in when the release is cut. Two texts: the short one for the
"What's new" box of the Play Console (500 characters at most, per language) and the longer one for the GitHub release.

## Google Play, "What's new" (about 430 characters)

```
First version for Android. Receive and send slow-scan TV (SSTV) and digital SSTV (DRM) pictures with the phone's microphone and speaker or a USB audio and radio interface. Screens made for touch: Receive, Transmit and Gallery, with settings, templates, calibration and frequencies on the phone. Looks: light, dark, Dim and Night (red on black). A Stop button in the notification, shortcuts on the icon (hold it), and the Back gesture.
```

## GitHub release (the Android part)

**QSSTV-Experimental for Android**

A phone and tablet version of the program, with a screen of its own made for fingers. It is the same receiver and transmitter as on the computer
(the same modes, the same pictures, the same files), so what works there works here.

- **Receive, Transmit, Gallery.** Hold the phone next to the radio's speaker and a picture starts by itself; choose a picture, check the time, press Send.
  A level meter says when the sound is too quiet or too loud. The gallery groups pictures by day, with a viewer you can pinch and swipe, sharing, and Undo for a delete.
- **Sound.** The phone's microphone and speaker, a headset cable or a USB audio interface; the receiver stays on with the screen off.
- **Radio.** CAT and push to talk through USB serial interfaces (Digirig and similar), the frequencies of your radio as a list you can tune from the Receive and Transmit screens.
- **Digital SSTV (DRM)** next to analog SSTV, one switch at the top of Receive and Transmit; profiles for the digital settings.
- **Templates.** Make and change them by touch (move, size, turn, colors, the macros for call sign, report, time and position) or open the ones made on the computer.
- **Calibration of the sound clock** with internet time, a radio time signal (WWV) or a picture from another station.
- **Looks.** Follows the phone's light or dark setting; or choose Light, Dark, **Dim** (a softer dark for a dark room) or **Night** (red on black, so the eyes of someone watching
  the sky or the band keep their night vision; the pictures can be shown in red too). Large text, screen readers and "Remove animations" are respected.
- **Android.** A **Stop** button in the notification that appears while the program listens or sends; hold the icon for **Receive**, **Transmit** or **Gallery**;
  share a picture to the program from the photos app; the Back gesture goes one step back, and from the first screen to the home screen (the program keeps receiving).
- **Fixed on the way.** Digital (DRM) reception no longer stops when the app goes to the background.

Not on the phone: the repeater and unattended beacon, FTP upload, the XML-RPC interface, the command line and the update check (Google Play updates the program).
The classic settings of the computer program are still reachable from Settings > Advanced for the options that have no page of their own.
The User's Guide has an Android page: <https://jbdubbs.github.io/QSSTV-Experimental/pages/android.html>.
