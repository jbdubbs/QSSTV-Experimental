#!/bin/sh
# Build and run the WWV tick detector / sample-rate fit test (pure C++, no Qt, no sound card).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src/sound"
g++ -O2 -std=gnu++17 -Wall -I"$SRC" -o "$HERE/wwvtest" "$HERE/wwvtest.cpp" "$SRC/wwvtickdetector.cpp" "$SRC/wwvtickfit.cpp"
"$HERE/wwvtest"
