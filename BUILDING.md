# Building QSSTV-Experimental

Most people want a [pre-built release](https://github.com/jbdubbs/QSSTV-Experimental/releases).
This page is for building from source.

The build uses CMake (`src/CMakeLists.txt`) and **Qt6**. Audio, webcam capture and serial PTT go through Qt
(Qt Multimedia, QSerialPort), so no PulseAudio, ALSA or V4L2 development packages are needed on any OS.

Requirements: Qt6 (Core, Widgets, Network, Xml, SerialPort, Multimedia), FFTW3 (double and float), OpenJPEG,
Hamlib, and CMake 3.16 or newer. Debug builds also need Qwt for the oscilloscope panel.

## Linux

Fedora:

```
sudo dnf install cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel qt6-qtmultimedia-devel \
    qt6-qtserialport-devel fftw-devel openjpeg-devel hamlib-devel
```

Debian/Ubuntu (24.04 or newer, for Qt6 packaging):

```
sudo apt install cmake pkg-config g++ qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev \
    qt6-serialport-dev libfftw3-dev libopenjp2-7-dev libhamlib-dev
```

Build and install:

```
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
```

### Hamlib from source

If your distribution's Hamlib is too old or missing, build Hamlib yourself and point CMake at the install prefix:

```
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release -DHAMLIB_ROOT=/path/to/hamlib/install
```

### Debug build

```
sudo dnf install qwt-qt6-devel        # Debian/Ubuntu: libqwt-qt6-dev
cmake -S src -B build-debug -DCMAKE_BUILD_TYPE=Debug
```

QtCreator can open `src/CMakeLists.txt` directly. If you have trouble compiling, please report the OS and version
(for example Fedora 44, Ubuntu 24.04, Windows 11), the Qt version, and the actual compiler or linker error.

### Flatpak naming

Flatpak requires the desktop file, icon and metainfo launchable to be named after the app ID
(`io.github.jbdubbs.qsstv-experimental`), and the license files to be installed in
`share/licenses/io.github.jbdubbs.qsstv-experimental`. Configure with `-DQSSTV_APPID_NAMES=ON` to install them that way;
the default keeps the short `qsstv-experimental.*` names and `share/doc/qsstv` that the AppImage and native installs use. Under Flatpak the app also
takes its Wayland app ID from `$FLATPAK_ID`.

## Linux AppImage

`packaging/appimage/build.sh` builds the self-contained AppImage inside an Ubuntu 24.04 container (glibc 2.39
floor, which is why the README asks for Ubuntu 24.04 or Fedora 40 and later) with the official Qt 6.11 binaries. It needs Docker and a Hamlib checkout at `../hamlib-upstream-sstv`
(a sibling of this repository), which it builds inside the container.

```
packaging/appimage/build.sh                  # x86_64, version 10.1 by default
VERSION=10.1 packaging/appimage/build.sh
ARCH=aarch64 packaging/appimage/build.sh     # arm64, built under QEMU emulation (slow)
```

The result is `QSSTV-Experimental-<version>-<arch>.AppImage` in the repository root (override with `OUT_DIR`).
The AppImage also bundles `kimageformats` and the libheif codec plugins, which provide the AVIF and HEIC formats.

For a feature-branch test artifact, set `QSSTV_APPIMAGE_NO_UPDATE=1` to omit the upstream update link (and optional delta-update metadata). The KG-STV AppImage workflow uses this setting and names its artifact with the source commit.

Next to the AppImage the build writes `<name>.AppImage.zsync` (delta-update data) and `<name>.AppImage.sha256`.
Upload all three to the GitHub release. The AppImage embeds the update information
`gh-releases-zsync|jbdubbs|QSSTV-Experimental|latest|QSSTV-Experimental-*-<arch>.AppImage.zsync`, so
`appimageupdatetool`, AppImageLauncher and Gear Lever can update it; GitHub's `latest` skips releases flagged
"pre-release", so the public release must be a normal one. Inspect with `./X.AppImage --appimage-updateinfo`.

### Desktop integration and updating (AppImage)

- On first start the AppImage offers to add a menu entry and icons (`~/.local/share/{applications,icons,metainfo}`).
  `Help > Desktop integration...` or `--install` / `--uninstall` does the same from the menu or a terminal; an entry
  whose AppImage was moved or replaced by a newer release is re-pointed automatically, and `--uninstall` removes the
  entries, icons and metadata of any release (it sweeps `qsstv-experimental*.desktop` and any entry launching a
  `QSSTV-Experimental*.AppImage`; `appimagekit_*` files from AppImageLauncher are left alone).
- `Help > Check for updates...` uses `appimageupdatetool` when it is installed (otherwise it opens the releases page).
- No FUSE (Ubuntu 24.04+, Fedora)? Run with `APPIMAGE_EXTRACT_AND_RUN=1` or `--appimage-extract-and-run`.

### arm64 (aarch64) AppImage

`ARCH=aarch64` (or `arm64`) builds the same AppImage for 64-bit ARM Linux (Raspberry Pi and other SBCs), named
`QSSTV-Experimental-<version>-aarch64.AppImage`. It uses the same scripts and the same bundled dependencies as the
x86_64 build; the arch-specific parts are Docker build arguments that `build.sh` sets for you.

On an x86_64 machine it builds under QEMU user emulation (`--platform linux/arm64`), so it needs Docker plus QEMU
binfmt support for aarch64 (on Fedora `qemu-user-static`; on Debian/Ubuntu `qemu-user-static` and `binfmt-support`;
check that `/proc/sys/fs/binfmt_misc/qemu-aarch64` exists). Expect a cold build to take one to two hours because
the Qt-dependent compile runs emulated; Docker caches the image layers, so later builds only redo the container step.
On a native arm64 host it is an ordinary, fast build with the same command.

```
ARCH=aarch64 VERSION=10.1 OUT_DIR=/some/dir packaging/appimage/build.sh
```

Notes on how the arm64 build differs (all handled in the scripts, listed here for when something breaks):

- **Qt:** the official `linux_arm64` / `linux_gcc_arm64` Qt 6.11.2 from `aqtinstall`, restricted with
  `--archives qtbase qtdeclarative qtsvg qtwayland qttranslations icu`. The automatically pulled `qttools` archive
  fails to extract under QEMU (`zipfile.BadZipFile`), and nothing here uses it. The install also uses the native
  `7z` (`--external 7z`) and retries up to three times.
- **linuxdeploy:** the `aarch64` AppImages. Their ELF header carries the `AI\x02` marker that stops QEMU's binfmt
  from executing them, so the Dockerfile zeroes those three bytes before `--appimage-extract`.
- **Extra libraries:** the official aarch64 Qt's `libQt6XcbQpa` links `libSM`/`libICE` (the x86_64 one does not), so
  `libsm6 libice6` are installed; nothing else extra is bundled, to stay in parity with x86_64.
- **Multimedia plugin:** the aarch64 Qt also ships a GStreamer plugin. `inside.sh` deletes it after the CMake
  configure step (deleting it earlier breaks Qt6Multimedia's CMake targets); only the FFmpeg backend is shipped,
  as on x86_64.

The container builds whatever is in the working tree, including uncommitted changes. For a release, build from a
clean export of the commit instead, for example:

```
mkdir -p /tmp/rel && git archive HEAD | tar -x -C /tmp/rel/qsstv-experimental   # plus a sibling /tmp/rel/hamlib-upstream-sstv
ARCH=aarch64 /tmp/rel/qsstv-experimental/packaging/appimage/build.sh
```

Verify the result before publishing: `file` shows an aarch64 ELF; the highest `GLIBC_` symbol (via `objdump -T`) is
at most 2.39; `ldd` of the binary and the Qt plugins shows nothing "not found" in a bare `ubuntu:24.04` arm64
container; and `QT_QPA_PLATFORM=offscreen ./QSSTV-Experimental-*.AppImage --batch some.wav` decodes a recording.
Emulation cannot test audio, so run RX and TX on real hardware as well.

## Windows (cross-compiled from Fedora)

The Windows build is a MinGW cross-compile.

1. Install the toolchain and libraries:

   ```
   sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads \
       mingw64-qt6-qtbase mingw64-qt6-qtmultimedia mingw64-qt6-qtserialport \
       mingw64-fftw mingw64-openjpeg mingw64-libusb1 mingw64-libvorbis
   ```

   `mingw64-libvorbis` lets Decode From File read Ogg Vorbis files; the build warns and carries on without it.
   `mingw64-nsis` provides `makensis`, which builds the Windows installer.

2. Cross-build Hamlib. It is vendored and linked statically, never taken from a system package. The script reads a
   Hamlib checkout from `$HAMLIB_SRC` (default `~/projects/hamlib-upstream-sstv`):

   ```
   tools/build-hamlib-win64.sh
   ```

3. Cross-build the AVIF and HEIC image plugins (libde265, libaom, libheif, libavif and the `kimg_avif` /
   `kimg_heif` plugins) into `third_party/imageformats-win64`. Run it once, before the main build; without it the
   package simply lacks those two formats:

   ```
   tools/build-image-plugins-win64.sh
   ```

4. Build and package:

   ```
   cmake -S src -B build-mingw64 \
       -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw64.cmake \
       -DCMAKE_BUILD_TYPE=Release \
       -DPKG_CONFIG_EXECUTABLE=/usr/bin/x86_64-w64-mingw32-pkg-config \
       -DHAMLIB_ROOT="$PWD/third_party/hamlib-win64"
   cmake --build build-mingw64 -j$(nproc)
   tools/package-win64.sh
   ```

   The output is `build-mingw64/QSSTV-Experimental-<version>-win64.exe`, an NSIS installer (the script works around
   Fedora's `makensis` lacking 32-bit stubs, so run it rather than `cpack` directly) that installs `qsstv-experimental.exe` and every required Qt, MinGW,
   FFTW, OpenJPEG and Hamlib DLL plus the Qt plugins.

5. Check that the package bundles every DLL it needs, including indirect ones:

   ```
   tools/check-win-deps.py build-mingw64/qsstv-experimental.exe
   ```

### KG-STV branch CI

`.github/workflows/kgstv-windows.yml` applies the same Fedora MinGW / static vendored Hamlib / AVIF+HEIC / NSIS process to `feature/kgstv-integration`. It produces an x86_64 **installer**, matching main's packaging rather than a portable self-extracting application. The source commit, pinned Hamlib commit and Fedora package versions accompany the installer and SHA-256 file in the Actions artifact.

A Windows runner extracts the actual installer and tests its application with compiler/Qt directories removed from PATH: packaged startup, both KG-STV modulation modes through production WAV TX/RX, native-codec image comparison and standalone text. These checks do not initialize audio or CAT/PTT, and do not establish real-device or original KG-STV outbound/BSR interoperability. The Windows test helper is a separate temporary CI artifact, not part of the installer. The workflow is started manually on the branch. Leave `installer_run` empty for a complete build, or supply a previous run ID to recheck its unchanged installer with a freshly compiled helper after test-tool fixes.

Supported systems: 64-bit Windows 10 version 1809 or newer, and Windows 11 (Qt 6's own minimum). Run `winver` to
check your version.

## Image formats

Pictures are loaded and saved through Qt's image plugins, so the available formats depend on the plugins your Qt
provides. PNG, JPEG, GIF and BMP are always available; JPEG 2000 is handled by OpenJPEG. The image dialogs list the
formats your build can read.

| Format | Linux AppImage | Windows installer | macOS app | Source build |
|---|---|---|---|---|
| WebP | yes | yes | yes | needs Qt's `qtimageformats` (Fedora `qt6-qtimageformats`) |
| AVIF | yes | yes | yes | opening files needs KDE `kimageformats` and libavif; AVIF over DRM needs only libavif |
| HEIC/HEIF | yes | yes | yes | needs `kimageformats` and libheif **with an HEVC decoder** (Fedora `libheif-freeworld`, Debian/Ubuntu `libheif-plugin-libde265`); on macOS nothing to install |

Fedora's stock libheif has no HEVC decoder, so HEIC loads there once a freeworld or HEVC-capable libheif is
installed; AVIF works either way. HEIC decoding uses libde265 (LGPL); HEVC patent exposure varies by jurisdiction.
On macOS, HEIC is read by Qt's own `qmacheif` plugin through Apple's image framework, which the bundle includes (`macdeployqt`
copies it), so the KDE plugin is built there for AVIF only.

A picture's EXIF/HEIF orientation is applied wherever it is loaded, including `--encode`. `tests/imageorient/run.sh` checks
that by sending sideways JPEGs (and, on macOS, HEIC files written by the system) through `--encode` and `--batch` and comparing
them with the original; the macOS CI job runs it on the bundled app, and it runs on Linux too (JPEG only, HEIC needs macOS).

Sending and receiving AVIF over DRM (Transmit > DRM > Encoding) does not use the plugin: the program links libavif directly
(Fedora `libavif-devel`, Debian/Ubuntu `libavif-dev`, Homebrew `libavif`), and libavif must be built with the libaom
encoder. Without libavif at build time the AVIF choice is grayed out and everything else works. All the release builds
include it. The macOS CI job checks an AVIF DRM round trip on the bundled app (`.github/workflows/macos.yml`).

## macOS

Apple Silicon (arm64) and Intel (x86_64): CI builds each natively (the Intel build runs on a
`macos-15-intel` runner; because Homebrew's libraries target the runner's OS, the Intel build requires macOS 15 or newer), builds and decodes test recordings on every change, and testers
have verified the app's operation on real Intel and Apple Silicon Macs. CI cannot cover live audio, the microphone prompt, the GUI or CAT.

```
brew install cmake pkg-config qt@6 fftw openjpeg hamlib libvorbis
cmake -S src -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build -j"$(sysctl -n hw.ncpu)"
```

This produces `build/qsstv-experimental.app`. To make it self-contained, run `"$(brew --prefix qt@6)/bin/macdeployqt" build/qsstv-experimental.app`
and sign it ad hoc with `codesign --force --deep --sign - build/qsstv-experimental.app`; `.github/workflows/macos.yml` does exactly
this, adds the icon, and publishes a `.zip` and `.dmg` as a workflow artifact (Actions > "macOS build" > a run >
Artifacts).

The build is not notarized, so Gatekeeper blocks the first launch of a downloaded copy. Either right-click the app,
choose Open, then Open again, or run `xattr -dr com.apple.quarantine QSSTV-Experimental.app`. On first use macOS asks
for microphone access; if you deny it, re-enable it under System Settings > Privacy & Security > Microphone.

Headless checks work as on Linux: `QSSTV_BIN=build/qsstv-experimental.app/Contents/MacOS/qsstv-experimental tests/filedecode/run.sh` (needs
Homebrew bash, `brew install bash`, since macOS ships bash 3.2). Ogg Vorbis files are decoded with libvorbisfile
because macOS's audio decoder does not handle Vorbis.
