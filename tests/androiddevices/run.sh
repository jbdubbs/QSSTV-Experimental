#!/bin/bash
# The rules for which Android audio devices Settings offers (src/android/androiddevicetypes.h) and for the names of USB serial
# ports (src/android/usbportname.h), on the desktop.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "${OUT:?}"' EXIT
c++ -std=c++17 -I"$HERE/../../src/android" "$HERE/test.cpp" -o "$OUT/devices"
"$OUT/devices"
c++ -std=c++17 -fPIC -I"$HERE/../../src/android" $(pkg-config --cflags Qt6Core) "$HERE/usbname.cpp" -o "$OUT/usbname" $(pkg-config --libs Qt6Core)
"$OUT/usbname"
