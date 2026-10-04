#!/bin/sh
# Build and run the slant fit test (pure C++, no Qt, no sound card).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src/sound"
g++ -O2 -std=gnu++17 -Wall -I"$SRC" -o "$HERE/slanttest" "$HERE/slanttest.cpp" "$SRC/slantfit.cpp"
"$HERE/slanttest"
