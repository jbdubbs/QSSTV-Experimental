# QSSTV-Experimental

QSSTV-Experimental is a program for receiving and transmitting SSTV and HAMDRM (sometimes called DSSTV). It is compatible with
most of MMSSTV and EasyPal. It began as a fork of QSSTV but has since diverged substantially — new SSTV modes, an alternate
decode engine, decode-from-file support, and ongoing image-quality work.

## Decoding an SSTV recording from a file

**In the program:** *File > Decode SSTV From File...* (Ctrl+O), or the folder button next to Start/Stop on the Receive tab.
Pick one or more WAV files. They play through the normal receiver at about ten times real time (you watch the picture
build up), pictures are saved as usual when *Autosave* is on, and the sound card receiver resumes when the last file is done.
The Stop button cancels. Any WAV works: 8/16/24/32 bit PCM or float, mono or stereo (the first channel is used), any sample
rate (it is resampled to 48 kHz). A picture that is still being received when the recording ends is kept if enough of it
arrived (*Save if Complete*).

**From the command line:**

	qsstv --decode recording.wav              # window opens, you watch it decode, then the sound card receiver resumes
	qsstv recording.wav                       # same
	qsstv --batch -o pictures/ a.wav b.wav    # headless: no window, no sound card needed

`--batch` saves every picture as `<file>_<n>_<MODE>.png` in the output directory (default: the current directory), prints one
line per picture on stdout, reports problems on stderr and exits with a code you can script on:
0 = every file gave a picture, 1 = some file gave none, 2 = unusable file or bad option, 3 = timeout. It decodes much faster than real time,
uses Qt's offscreen platform so it also works over ssh, reads your settings but never writes them, and does not prune your image caches.

Options: `--mode PD120` (receive only that mode instead of auto detection; `--list-modes` shows the names),
`--engine qsstv|core|auto` (which receive engine, for this run only), `--wide-filter on|off|auto` (the wide video filter for the fast
modes, for this run only), `--timeout SECONDS` (batch: per-file limit, default the file's length + 30 s), `--help`.
For a completely clean run use a throw-away configuration: `HOME=$(mktemp -d) qsstv --batch ...`.

`tests/filedecode/run.sh` decodes generated recordings with the built program and checks pictures, exit codes and file formats
(needs the loopback harness in `tests/jb60_loopback`; ffmpeg is optional and adds the sample-rate and format variants; the mmsstv-core checks, including Auto Slant, use `encode_wav_tool` from the CMake build and ffmpeg).

## Installation

The build is CMake-based (`src/CMakeLists.txt`) and needs **Qt6** — Qt5 is no longer
supported. Audio and webcam capture go through Qt Multimedia now, so unlike older
QSSTV forks, **no PulseAudio, ALSA or V4L2 development packages are needed at all**,
on any OS.

### Linux dependencies

Fedora:
```
sudo dnf install cmake gcc-c++ pkgconf-pkg-config qt6-qtbase-devel qt6-qtmultimedia-devel \
    qt6-qtserialport-devel fftw-devel openjpeg-devel hamlib-devel
```

Debian/Ubuntu (24.04+ / bookworm+, for Qt6 packaging):
```
sudo apt install cmake pkg-config g++ qt6-base-dev qt6-base-dev-tools qt6-multimedia-dev \
    qt6-serialport-dev libfftw3-dev libopenjp2-7-dev libhamlib-dev
```

If your distro's hamlib package is too old (or missing), build it from source and point
CMake at it with `-DHAMLIB_ROOT=/path/to/hamlib/install` instead — see `tools/build-hamlib-win64.sh`
for the same idea applied to a Windows cross-build.

### macOS dependencies

```bash
brew install qt@6 fftw hamlib openjpeg pkg-config
```

(No PulseAudio to install or start any more — Qt Multimedia uses CoreAudio directly.)

### Windows

**Supported systems:** 64-bit Windows 10 version 1809 or newer, and Windows 11. This is
Qt 6's own minimum. Older systems fail before the program starts, with a missing-entry-point
error from a Qt DLL: Windows 7 and 8.x (`GetCurrentPackageFullName` / `WaitOnAddress`
missing from `KERNEL32.dll`) and the early Windows 10 builds such as 1507
(`D3D12SerializeVersionedRootSignature` missing from `Qt6Gui.dll`). 32-bit Windows is not
supported. Run `winver` to check your version.

There is no Windows installer released yet, but the app **does build and link
correctly** for 64-bit Windows via MinGW cross-compilation from Linux, verified on
Fedora:

```
sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads \
    mingw64-qt6-qtbase mingw64-qt6-qtmultimedia mingw64-qt6-qtserialport \
    mingw64-fftw mingw64-openjpeg mingw64-libusb1

# Hamlib is vendored, not taken from a system package -- see tools/build-hamlib-win64.sh
# (needs ~/projects/hamlib-upstream-sstv or HAMLIB_SRC pointed at a Hamlib checkout).
tools/build-hamlib-win64.sh

cmake -S src -B build-mingw64 \
    -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -DPKG_CONFIG_EXECUTABLE=/usr/bin/x86_64-w64-mingw32-pkg-config \
    -DHAMLIB_ROOT="$PWD/third_party/hamlib-win64"
cmake --build build-mingw64 -j$(nproc)

# Package it (ZIP always; also an NSIS installer if `sudo dnf install mingw64-nsis`
# is installed first):
cd build-mingw64 && cpack -G ZIP
```

This produces a self-contained `qsstv-9.0-win64.zip` (exe + all required Qt/MinGW/
FFTW/OpenJPEG/libusb/Hamlib DLLs and Qt plugins, verified complete against
`objdump -p qsstv.exe`'s actual dependency list). What hasn't been verified yet:
actually *running* it — this dev environment's Wine install hangs indefinitely on
first-run initialization regardless of this project, so real execution testing needs
an actual Windows machine or VM. MSYS2 (building natively on Windows instead of
cross-compiling) should work the same way with `mingw-w64-x86_64-`-prefixed package
names, but that path hasn't been tried by anyone working on this fork yet either.

### Compile and Install (Linux/macOS)

	cmake -S src -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j$(nproc)
	sudo cmake --install build

If Qt6, FFTW or OpenJPEG aren't found automatically, or you're using a from-source
Hamlib (see above), pass `-DCMAKE_PREFIX_PATH=...` / `-DHAMLIB_ROOT=...` to the first
command.

### Self-contained Linux AppImage

`tools/build-appimage-linux.sh` packages a build from `build-cmake/` (or `$BUILD_DIR`)
into a single-file `QSSTV-Experimental-<version>-x86_64.AppImage` that bundles Qt6 and
every other runtime dependency, needing no system Qt6 install to run. It downloads
`linuxdeploy`/`linuxdeploy-plugin-qt`/`appimagetool` (cached under
`~/.cache/qsstv-appimage-tools`) the first time it runs. See the script's own header
comment if a build ever segfaults on startup under every Qt platform backend — that's a
known, already-solved issue with how some versions of `linuxdeploy` patch RELR-relocated
binaries, not a QSSTV-Experimental bug.

### Debug Compile
If you have problems compiling the software, please give as much information as possible but at least:
* OS and version (e.g. Fedora 44, Ubuntu 24.04, Windows 11)
* Qt version (e.g. Qt 6.11)
* The actual compiler/linker error, not just "it doesn't build"

QtCreator can open `src/CMakeLists.txt` directly as a project. The debug build also
needs Qwt for the oscilloscope panel (`qwt-qt6-devel` / `libqwt-qt6-dev` /
`brew install qwt`) — pass `-DCMAKE_BUILD_TYPE=Debug` and use an external debugger
(such as gdb) as usual.
