# Attribution and third-party licenses

QSSTV-Experimental is a fork of **QSSTV 9.5.11** by Johan Maes, ON4QZ (copyright 2000-2019,
<https://www.qsl.net/o/on4qz>) and, like QSSTV, is released under the **GNU General Public License v3**
(see `COPYING` and `LICENSE`). Changes made in this fork are Copyright 2026 Jason Weisberger (NT0Y) and
contributors, under the same license.

This file lists everything in the source tree, and everything shipped in binary releases, that comes from
somebody else. Original copyright notices inside the source files are kept and take precedence.

## Code in the source tree

| Component | Where | Author / origin | License |
|---|---|---|---|
| QSSTV 9.5.11 (all code not listed elsewhere) | `src/` | Johan Maes, ON4QZ, and QSSTV contributors (Jörg Wunsch DL8DTL, Daniele Forsi, hspil, Ruxton, AsciiWolf, Barry de Graaff, Greg Tangey, Sam Saccone) | GPL v3 |
| HAMDRM receiver (RXAMADRM) | `src/drmrx/` | M. Bos, PA0MBO | GPL (see the headers) |
| Viterbi decoder, `msd*`, `ofdm` helpers | `src/drmrx/` | Torsten Schorr, Univ. of Kaiserslautern (from *diorama*, with A. Dittrich) | GPL v2 or later |
| `crc8_c.cpp` | `src/drmrx/` | from diorama-1.1.1, A. Dittrich and T. Schorr | GPL v2 or later |
| DRM transmitter (from the Dream DRM software) | `src/drmtx/common/` | Volker Fischer, Andrew Murphy, Andrea Russo and others, copyright 2001-2007 | GPL v2 or later |
| `rfft`/`cfft` | `src/drmrx/newfft.cpp` | Douglas Scott, MiXViews, Regents of the University of California, 1993-94 | permissive notice, reproduced below |
| Reed-Solomon codec | `src/utils/rs.cpp`, `rs.h` | Phil Karn, KA9Q, 1999 (based on Morelos-Zaragoza and Thirumoorthy) | GPL |
| Maia XML-RPC | `src/xmlrpc/` | Frerich Raabe (2003), Sebastian Wiedenroth (2007) | BSD 2-clause (in the headers) |
| `QFtp`, `QUrlInfo` | `src/utils/qftp.*`, `qurlinfo.*` | Digia / The Qt Company, from Qt 4 | LGPL v2.1 / GPL v3 |
| Papirus icon theme | `src/icons/papirus/` | Papirus Development Team | GPL v3 (`src/icons/papirus/LICENSE`) |
| Test patterns | `src/icons/testpatterns/` | see `CREDITS.md` there; PM5544 is CC BY 2.5 (ebnz), SMPTE bars public domain (Denelson83) | as listed |

### Ideas and compatibility

- The JB60 chroma work includes a Wiener deconvolution stage whose idea was cross-checked against
  [Robot36](https://github.com/xdsopl/robot36) by Ahmet Inan (xdsopl), 0BSD. QSSTV-Experimental's code is its
  own implementation.
- Transmit tone levels and the end-of-image footer are chosen to match the behaviour of MMSSTV
  (Makoto Mori, JE3HHT) so the two programs interoperate. No MMSSTV code is included.
- Sample-rate calibration uses WWV time ticks and NTP; the tick-waterfall display follows the idea used in MMSSTV.

## Libraries used at build time or shipped in binary releases

Linux AppImage and Windows zip builds bundle these libraries unmodified. Each is under its own license; the
license texts ship with the library (Windows: next to the DLLs; AppImage: inside the image under
`usr/share/doc` where the distribution provides them) and are available at the project URLs below.

| Library | License | Source |
|---|---|---|
| Qt 6 (Core, Widgets, Network, Xml, SerialPort, Multimedia, Qt Image Formats) | LGPL v3 | <https://www.qt.io/> |
| Hamlib | LGPL v2.1 | <https://hamlib.github.io/> (Windows build vendored from commit in `third_party/hamlib-win64/VENDORED_COMMIT.txt`) |
| FFTW 3 | GPL v2 or later | <https://www.fftw.org/> |
| OpenJPEG | BSD 2-clause | <https://www.openjpeg.org/> |
| libsndfile, libogg, libvorbis, libFLAC | LGPL v2.1 / BSD | <https://libsndfile.github.io/libsndfile/>, <https://xiph.org/> |
| Qwt | Qwt License (LGPL with exceptions) | <https://qwt.sourceforge.io/> |
| KImageFormats (WebP/AVIF/HEIC plugins), Extra CMake Modules | LGPL v2.1+ / BSD | <https://invent.kde.org/frameworks/kimageformats> |
| libheif, libde265 | LGPL v3 | <https://github.com/strukturag/libheif>, <https://github.com/strukturag/libde265> |
| libavif, libaom | BSD 2-clause | <https://github.com/AOMediaCodec/libavif>, <https://aomedia.googlesource.com/aom/> |
| libwebp | BSD 3-clause | <https://chromium.googlesource.com/webm/libwebp> |

For the LGPL libraries, binary releases use dynamic linking, so you may replace them with your own build.
Corresponding source for the bundled versions is available from the URLs above; contact the maintainer if you
cannot obtain it.

## Notices reproduced from the source

### `src/drmrx/newfft.cpp` (MiXViews)

> Copyright (c) 1993, 1994 Regents of the University of California. Author: Douglas Scott.
> Permission to use, copy and modify this software and its documentation for research and/or educational
> purposes and without fee is hereby granted, provided that the above copyright notice appear in all copies and
> that both that copyright notice and this permission notice appear in supporting documentation. The author
> reserves the right to distribute this software and its documentation. The University of California and the
> author make no representations about the suitability of this software for any purpose, and in no event shall
> University of California be liable for any damage, loss of data, or profits resulting from its use. It is
> provided "as is" without express or implied warranty.

### Test patterns

PM5544: "Philips PM5544.svg" by ebnz, Wikimedia Commons, CC BY 2.5 (<https://creativecommons.org/licenses/by/2.5/>).
SMPTE color bars: "SMPTE Color Bars.svg" by Denelson83, Wikimedia Commons, public domain.
