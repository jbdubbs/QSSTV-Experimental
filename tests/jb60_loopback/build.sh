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
QT=${QT_INCLUDE:-/usr/include/qt6}
cd "$SRC"
g++ -O2 -fPIC -std=gnu++17 -w -DQT_NO_DEBUG -DIMAGE_DIR="\"$HERE/images\"" \
    -I"$HERE/stubs" -I. -Iutils -Idsp -Iconfig -Isstv \
    -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -I"$QT/QtWidgets" \
    -o "$HERE/loopback" "$HERE/loopback.cpp" \
    sstv/modes/modebase.cpp sstv/modes/modejb60.cpp sstv/modes/modepd.cpp sstv/sstvparam.cpp \
    sstv/videofilterselection.cpp sstv/chromadeconvolution.cpp sstv/chromaedgeboost.cpp sstv/chromagridphase.cpp \
    dsp/downsamplefilter.cpp dsp/filters.cpp dsp/filter.cpp dsp/filterparam.cpp \
    -lQt6Widgets -lQt6Gui -lQt6Core
