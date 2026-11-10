#!/bin/sh
# Camera frame orientation (src/videocapt/frameimage.*): a phone's frame is rotated and, for the front camera, mirrored, and
# QVideoFrame::toImage() leaves that out. Builds a small standalone binary; needs the Qt6 Core/Gui/Multimedia headers.
#   tests/camorient/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
OUT=${TMPDIR:-/tmp}/camorient-test.$$
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
g++ -O1 -fPIC -std=gnu++17 -Wall -Wextra -I"$SRC/videocapt" \
    -isystem "$QT" -isystem "$QT/QtCore" -isystem "$QT/QtGui" -isystem "$QT/QtMultimedia" \
    -o "$OUT/camorient" "$HERE/test.cpp" "$SRC/videocapt/frameimage.cpp" \
    -lQt6Multimedia -lQt6Gui -lQt6Core
QT_QPA_PLATFORM=offscreen "$OUT/camorient"
