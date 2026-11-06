# Releasing the Android app on Google Play

What the maintainer does once, and what to answer in the Play Console. The build itself is `.github/workflows/android.yml`
(see BUILDING.md for building by hand). Nothing here is secret; the keystore and its password are.

## 1. Accounts and names that cannot change later

- **Application id: `io.github.jbdubbs.qsstv_experimental`.** It is part of every APK, and Play will not accept a different one for the same app.
- A Google Play developer account (one-time fee). A *personal* account created recently must run a **closed test with at least 12 testers
  for 14 days** before it may publish to production (the rule is Google's and changes: check "Production access" in the Console).
  An *organization* account is exempt but needs a D-U-N-S number.
- Developer identity verification, a public contact e-mail address (it appears in the listing), and the privacy policy URL (section 5).

## 2. The upload key (once)

Play re-signs what you upload with its own *app signing key* (Play App Signing); you sign the upload with an *upload key* that
Google can reset if you lose it. Create it on a computer you keep:

```
keytool -genkeypair -v -keystore upload.jks -storetype PKCS12 -alias upload \
        -keyalg RSA -keysize 2048 -validity 9125 -dname "CN=QSSTV-Experimental,O=NT0Y,C=US"
```

Give the store and key the same password (PKCS12 does that by default). **Back up `upload.jks` and the password** (a password manager and
one offline copy). Then give the workflow the three secrets (the file is read from stdin, nothing is stored in the repository):

```
base64 -w0 upload.jks | gh secret set ANDROID_KEYSTORE_BASE64
gh secret set ANDROID_KEYSTORE_ALIAS      # upload
gh secret set ANDROID_KEYSTORE_PASSWORD
```

From then on the workflow signs the APK and the AAB with the upload key. Without the secrets it signs the APK with a throwaway debug key
and leaves the AAB unsigned (so a test build can never be uploaded by mistake).

**An APK from GitHub and the Play app are signed with different keys** (Play signs with its own key). A phone that has one cannot be updated by the
other without uninstalling first (and the settings go). Decide whether GitHub releases carry an APK at all; the CI artifact is enough for testers.

## 3. Versions

`versionName` is the program version (`CPACK_PACKAGE_VERSION`), and `versionCode` is made from it: `major*1000000 + minor*10000 + patch*100`
(10.1.1 gives 10010100). Play wants a higher `versionCode` for every upload, so every upload needs a new program version (10.1.2,
10.2, ...). Never reuse a number, even for a build that was only uploaded to the internal track.

## 4. Release tracks

1. **Internal testing** (up to 100 testers by e-mail address, no review): upload the AAB of the CI run, install from the opt-in link on a phone. This is
   where the real-phone checks of BUILDING.md happen.
2. **Closed testing** (the 12 testers for 14 days, if the account needs it).
3. **Production.** The first review of a new app takes days. The *pre-launch report* of the Console runs the app on real devices and lists crashes and
   accessibility findings: read it for every upload.

Requirements the build already meets: target API 36, 64-bit libraries only, **16 KB page size** (the workflow fails the build when a library or the zip is not aligned).

## 5. Store listing (drafts to paste)

- **Name** (30 characters): `QSSTV-Experimental`
- **Short description** (80): `Send and receive slow-scan TV (SSTV) and digital SSTV pictures over amateur radio.`
- **Category:** Communication (or Tools). **Tags:** ham radio, SSTV.
- **Full description** (4000):

> QSSTV-Experimental sends and receives slow-scan television (SSTV) and digital SSTV (HAMDRM) pictures for amateur radio.
>
> Hold the phone next to your radio's speaker and watch pictures appear, or connect a USB audio and radio interface (such as a Digirig) and let the
> program key the transmitter and read the frequency. No account, no ads, nothing leaves your phone.
>
> - Receives and sends 48 analog SSTV modes (Robot, Scottie, Martin, PD, Wraase SC2, MMSSTV's MP, MR, ML and MC modes, and JB60) and HAMDRM digital pictures
> - Automatic mode detection, slant correction and picture saving; keeps receiving with the screen off
> - Works with the phone's microphone and speaker, a headset cable, or a USB audio interface
> - CAT frequency control and PTT through USB serial interfaces (CP210x, FTDI, CH340, Prolific)
> - Picture gallery, templates with your call sign, report, time and position, and sharing to other apps
> - Decode recordings of SSTV from any sound file
>
> Free software under the GNU GPL v3, based on QSSTV by Johan Maes, ON4QZ. Source code: github.com/jbdubbs/QSSTV-Experimental
>
> A licensed amateur radio operator is responsible for what is transmitted. Receiving needs no license.

- **Privacy policy URL:** `https://jbdubbs.github.io/QSSTV-Experimental/pages/privacy.html` (the guide is published from `main`: live after the merge of android-port).
- **Graphics:** app icon 512x512 PNG (`packaging/linux/icons/qsstv-experimental-512.png`), **feature graphic 1024x500 (still to make)**, phone screenshots (at least 2, 16:9 or 9:16;
  take them on a real phone: Receive with a picture, Transmit, Gallery, Settings) and, if tablets are supported, 7" and 10" tablet screenshots.

## 6. Data safety form

The program collects nothing on a server. Answer "the app does not collect or share"... except for the optional place lookup:

| Question | Answer |
|---|---|
| Does the app collect or share user data? | **Yes** (the optional lookup shares the position) |
| Location: approximate and precise | **Shared** with a third party (OpenStreetMap's Nominatim), only if the user switches on the city and state lookup. **Optional**. Purpose: app functionality. |
| Audio (voice or sound recordings) | **Not collected**: the microphone is decoded on the phone and not stored or sent |
| Photos | **Not collected**: the camera picture and the gallery pictures stay on the phone |
| Personal info, financial info, contacts, files, messages, app activity, web browsing, device IDs | Not collected |
| Is data encrypted in transit? | Yes (HTTPS to Nominatim) |
| Can users ask for data to be deleted? | Nothing is held by the developer; uninstalling removes the data on the phone |

Anything the user transmits by radio is public by its nature (see the privacy policy); the form is about data the app sends over the internet.

## 7. Permissions and declarations

| Permission | Why (for the Console and for reviewers) |
|---|---|
| `RECORD_AUDIO` | Receiving SSTV and DRM pictures from the radio's sound through the microphone or a USB audio interface |
| `CAMERA` | Taking a picture to send (asked when the camera button is used) |
| `ACCESS_FINE_LOCATION`, `ACCESS_COARSE_LOCATION` | The station's position for the picture (asked when the GPS button is switched on); **foreground only**, no background location permission |
| `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MICROPHONE`, `POST_NOTIFICATIONS`, `WAKE_LOCK` | The receiver keeps decoding with the screen off while a notification says so |
| `INTERNET`, `ACCESS_NETWORK_STATE` | The optional place lookup; the User's Guide server on the phone itself (127.0.0.1) and the CAT bridge to the USB adapter (127.0.0.1) |
| `MODIFY_AUDIO_SETTINGS` | Audio focus and routing while a picture is sent |

**Foreground service declaration (type `microphone`).** The Console asks what the service does and for a video.
Text: *"The user starts reception with the Receive tab. QSSTV-Experimental decodes slow-scan television audio from the microphone and has to keep listening when the screen turns off or
another app is in front, otherwise a picture that takes one to five minutes is lost. A notification is shown for as long as it runs, and More > Exit (or Stop) ends it."*
Video (under 2 minutes): open the app, allow the microphone, start receiving, play an SSTV recording from another device next to the phone, press Home, show the notification, turn the screen off and on and show that the picture went on, then Exit.

**Content rating questionnaire:** a utility app: no violence, no user-generated content shared between users, no purchases, no ads, no gambling. **Target audience:** 18 and over
(not designed for children). **Ads:** none. **News, government, health, finance, VPN:** no.

## 8. Before a first upload: still to do on the project side

- The real-phone pass of everything (BUILDING.md): RX/TX on the built-in speaker, DRM, USB adapter PTT and CAT, GPS, the camera, the screen-off soak.
- A feature graphic and screenshots, a contact address, the merge of `android-port` into `main` (so that the privacy policy URL is live).
- Decide about the APK on GitHub releases (section 2) and whether the version in Play follows the desktop releases.
