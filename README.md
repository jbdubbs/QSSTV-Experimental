# QSSTV
QSSTV is a program for receiving and transmitting SSTV and HAMDRM (sometimes called DSSTV). It is compatible with most of MMSSTV and EasyPal

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
(needs the loopback harness in `tests/jb60_loopback`; ffmpeg is optional and adds the sample-rate and format variants).

## Installation

### Dependencies 

For apt based distros you can install dependencies as follows:

```
apt install pkg-config g++ libfftw3-dev qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools libqt5svg5-dev libhamlib++-dev libasound2-dev libpulse-dev libopenjp2-7 libopenjp2-7-dev libv4l-dev build-essential
```

### macOS Dependencies

For macOS users, you can install dependencies using Homebrew:

```bash
brew install qt@5 fftw hamlib openjpeg pulseaudio qwt pkg-config
```

**Note:** You must have PulseAudio running for sound to work:
```bash
brew services start pulseaudio
```

### Compile and Install
	mkdir src/build
	cd src/build
	# For Linux
	qmake ..
	# For macOS
	/opt/homebrew/opt/qt@5/bin/qmake ..
	
	make -j2
	sudo make install

Note: make -j2, 2 is the number of cores to be used for parallel compiling. If you have more cores, use a higher number.

### Debug Compile
If you have problems compiling the software, please give as much information as possible but at least:
* Linux Distribution and Version (e.g. Ubuntu 18.04)
* QT Version (e.g. Qt 5.4.1)
* Screen dump of the compile process showing the error

If you want to be able to debug the program, the simplest way is to install QtCreator and from within QtCreator open a new project and point to the qsstv.pro file. Note: you will need to install doxygen and libqwt

`sudo apt-get install doxygen libqwt-qt5-dev`

You can also run qmake with the following attributes:

`qmake CONFIG+=debug`

and use an external debugger (such as gdb)
