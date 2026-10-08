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

## Linux AppImage

`packaging/appimage/build.sh` builds the self-contained AppImage inside an Ubuntu 24.04 container (glibc 2.39
floor) with the official Qt 6.11 binaries. It needs Docker and a Hamlib checkout at `../hamlib-upstream-sstv`
(a sibling of this repository), which it builds inside the container.

```
packaging/appimage/build.sh                  # x86_64, version 10.0-Pre7 by default
VERSION=10.0-Pre8 packaging/appimage/build.sh
ARCH=aarch64 packaging/appimage/build.sh     # arm64, built under QEMU emulation (slow)
```

The result is `QSSTV-Experimental-<version>-<arch>.AppImage` in the repository root (override with `OUT_DIR`).
The AppImage also bundles `kimageformats` and the libheif codec plugins, which provide the AVIF and HEIC formats.

## Windows (cross-compiled from Fedora)

The Windows build is a MinGW cross-compile.

1. Install the toolchain and libraries:

   ```
   sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads \
       mingw64-qt6-qtbase mingw64-qt6-qtmultimedia mingw64-qt6-qtserialport \
       mingw64-fftw mingw64-openjpeg mingw64-libusb1 mingw64-libvorbis
   ```

   `mingw64-libvorbis` lets Decode From File read Ogg Vorbis files; the build warns and carries on without it.
   `mingw64-nsis` is optional and adds an NSIS installer next to the ZIP.

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
   cd build-mingw64 && cpack -G ZIP
   ```

   The output is `QSSTV-Experimental-<version>-win64.zip`, containing `qsstv.exe` and every required Qt, MinGW,
   FFTW, OpenJPEG and Hamlib DLL plus the Qt plugins.

5. Check that the package bundles every DLL it needs, including indirect ones:

   ```
   tools/check-win-deps.py build-mingw64/qsstv.exe
   ```

Supported systems: 64-bit Windows 10 version 1809 or newer, and Windows 11 (Qt 6's own minimum). Run `winver` to
check your version.

## Image formats

Pictures are loaded and saved through Qt's image plugins, so the available formats depend on the plugins your Qt
provides. PNG, JPEG, GIF and BMP are always available; JPEG 2000 is handled by OpenJPEG. The image dialogs list the
formats your build can read.

| Format | Linux AppImage | Windows ZIP | Source build |
|---|---|---|---|
| WebP | yes | yes | needs Qt's `qtimageformats` (Fedora `qt6-qtimageformats`) |
| AVIF | yes | yes | needs KDE `kimageformats` and libavif |
| HEIC/HEIF | yes | yes | needs `kimageformats` and libheif **with an HEVC decoder** (Fedora `libheif-freeworld`, Debian/Ubuntu `libheif-plugin-libde265`) |

Fedora's stock libheif has no HEVC decoder, so HEIC loads there once a freeworld or HEVC-capable libheif is
installed; AVIF works either way. HEIC decoding uses libde265 (LGPL); HEVC patent exposure varies by jurisdiction.

## macOS

Not yet supported; a macOS build and release are pending.
