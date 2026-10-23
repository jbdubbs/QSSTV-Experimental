#!/bin/sh
# Tests of src/gps: the NMEA parser and the one-shot GPS reader (the reader talks to a pseudo terminal that the test feeds).
# Builds a small standalone binary; needs Qt6 Core/Gui/Widgets/SerialPort headers and moc (Qt6 libexec; or MOC=).
#   tests/gps/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
MOC=${MOC:-$(command -v moc-qt6 || echo /usr/lib64/qt6/libexec/moc)}
OUT=${TMPDIR:-/tmp}/gps-test.$$
mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT
"$MOC" "$SRC/gps/gpsreader.h" -o "$OUT/moc_gpsreader.cpp"
g++ -O1 -fPIC -std=gnu++17 -Wall -Wextra -Wno-unused-parameter -I"$SRC/gps" -I"$SRC/config" -I"$SRC/utils" \
    -isystem "$QT" -isystem "$QT/QtCore" -isystem "$QT/QtGui" -isystem "$QT/QtWidgets" -isystem "$QT/QtSerialPort" \
    -o "$OUT/gpstest" "$HERE/test.cpp" "$SRC/gps/nmeaparser.cpp" "$SRC/gps/gpsreader.cpp" "$OUT/moc_gpsreader.cpp" \
    -lQt6SerialPort -lQt6Widgets -lQt6Gui -lQt6Core
"$OUT/gpstest"
