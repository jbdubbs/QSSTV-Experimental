#!/bin/bash
# The CM108 GPIO PTT module (src/rig/cm108lines.cpp): report bytes, chip ids, the hidraw list from a fake /sys tree, the "auto" rule and an
# open that refuses what is not a CM108. Qt Core and the one source file; no hardware is touched. tests/cm108/app.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SCRATCH=$(mktemp -d); trap 'rm -rf "$SCRATCH"' EXIT
INC=$(pkg-config --cflags Qt6Core)
LIBS=$(pkg-config --libs Qt6Core)
c++ -std=c++17 -fPIC -I"$HERE/../../src/rig" $INC "$HERE/app.cpp" "$HERE/../../src/rig/cm108lines.cpp" $LIBS -o "$SCRATCH/app"
QT_QPA_PLATFORM=offscreen "$SCRATCH/app"
