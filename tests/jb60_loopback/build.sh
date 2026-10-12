#!/bin/sh
# TX -> channel -> RX loopback test for the QSSTV-engine modes (JB60, with PD120 as a control).
# Compiles the real modebase/modejb60/modepd/sstvparam plus the real receive front end
# (dsp/downsamplefilter, dsp/filters videoFilter, dsp/filter, dsp/filterparam) against stub globals
# (see stubs/), so no GUI or sound card is needed. See README.md.
#
#   ./build.sh                 build ./loopback
#   ./loopback jb --suite      the README table for JB60 (use pd for the PD120 control)
#   ./loopback jb --image card --snr 25 --ssb --out /tmp/jb
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
if [ "$(uname)" = Darwin ]; then
    # Homebrew Qt ships frameworks (QT_PREFIX defaults to `brew --prefix qt@6`)
    QT_PREFIX=${QT_PREFIX:-$(brew --prefix qt@6)}
    QT_FLAGS="-F$QT_PREFIX/lib -I$QT_PREFIX/include"
    QT_LIBS="-F$QT_PREFIX/lib -framework QtWidgets -framework QtGui -framework QtCore -Wl,-rpath,$QT_PREFIX/lib"
    QT_FLAGS="$QT_FLAGS -I$QT_PREFIX/include/QtCore -I$QT_PREFIX/include/QtGui -I$QT_PREFIX/include/QtWidgets"
else
    QT=${QT_INCLUDE:-/usr/include/qt6}
    QT_FLAGS="-I$QT -I$QT/QtCore -I$QT/QtGui -I$QT/QtWidgets"
    QT_LIBS="-lQt6Widgets -lQt6Gui -lQt6Core"
fi
cd "$SRC"
g++ -O2 -fPIC -std=gnu++17 -w -DQT_NO_DEBUG -DIMAGE_DIR="\"$HERE/images\"" \
    -I"$HERE/stubs" -I. -Iutils -Idsp -Iconfig -Isstv \
    $QT_FLAGS \
    -o "$HERE/loopback" "$HERE/loopback.cpp" \
    sstv/modes/modebase.cpp sstv/modes/modejb60.cpp sstv/modes/modepd.cpp sstv/sstvparam.cpp \
    sstv/chromaedgeboost.cpp sstv/chromagridphase.cpp sstv/chromacompanding.cpp sstv/chromapseudoluma.cpp sstv/chromatriangledecimation.cpp \
    dsp/downsamplefilter.cpp dsp/filters.cpp dsp/filter.cpp dsp/filterparam.cpp \
    $QT_LIBS
