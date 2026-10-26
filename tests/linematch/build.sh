#!/bin/sh
# Build and run the line match test (pure C++, no Qt, no sound card).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src/sstv"
g++ -O2 -std=gnu++17 -Wall -I"$SRC" -o "$HERE/linematchtest" "$HERE/linematchtest.cpp"
"$HERE/linematchtest"
