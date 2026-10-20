#!/bin/sh
# Direct JPEG 2000 check of src/utils/jp2io.cpp (encode/decode round trip, truncated stream).
# Builds a small standalone binary against stub globals; needs libopenjp2-dev and Qt6 Core/Gui.
# moc (Qt6 libexec) is found via PATH or MOC=.
#   tests/jp2io/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
OUT=${TMPDIR:-/tmp}/jp2io-test.$$
MOC=${MOC:-$(command -v moc-qt6 || echo /usr/lib64/qt6/libexec/moc)}
trap 'rm -f "$OUT" "$OUT.moc.cpp"' EXIT
cd "$SRC"
"$MOC" utils/jp2io.h -o "$OUT.moc.cpp"
g++ -O1 -fPIC -std=gnu++17 -w -I"$HERE/stubs" -Iutils -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" \
    $(pkg-config --cflags libopenjp2) \
    -o "$OUT" "$HERE/test.cpp" utils/jp2io.cpp utils/color.cpp "$OUT.moc.cpp" \
    $(pkg-config --libs libopenjp2) -lQt6Gui -lQt6Core
QT_QPA_PLATFORM=offscreen "$OUT"
