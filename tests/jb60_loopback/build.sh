#!/bin/sh
# TX -> RX loopback test for the QSSTV-engine modes (JB60, with PD120 as a control).
# Compiles the real modebase/modejb60/modepd/sstvparam against stub globals (see stubs/),
# so no GUI or sound card is needed.
#
#   ./build.sh                 build ./loopback
#   ./loopback <jb|pd> [image 0-2] [noise Hz] [png prefix|-] [decim] [rx clock error]
#   e.g. QT_QPA_PLATFORM=offscreen ./loopback jb 0 40 out/jb 4
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
cd "$SRC"
g++ -O2 -fPIC -std=gnu++17 -w -DQT_NO_DEBUG \
    -I"$HERE/stubs" -I. -Iutils -Idsp -Iconfig -Isstv \
    -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -I"$QT/QtWidgets" \
    -o "$HERE/loopback" "$HERE/loopback.cpp" \
    sstv/modes/modebase.cpp sstv/modes/modejb60.cpp sstv/modes/modepd.cpp sstv/sstvparam.cpp \
    -lQt6Widgets -lQt6Gui -lQt6Core
