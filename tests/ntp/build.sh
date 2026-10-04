#!/bin/sh
# Build and run the NTP calibration maths test (pure C++, no Qt, no network, no sound card).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src/sound"
g++ -O2 -std=gnu++17 -Wall -I"$SRC" -o "$HERE/ntptest" "$HERE/ntptest.cpp" "$SRC/linefit.cpp"
"$HERE/ntptest"
