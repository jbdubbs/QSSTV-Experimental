#!/bin/bash
# The Android sound backend's sample ring (src/sound/samplering.h) on the desktop: single-thread rules and a two thread stress test.
# No Qt, no sound card. Builds with -fsanitize=thread when SANITIZE=1 (slower).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -f "$OUT/test"; rmdir "$OUT"' EXIT
FLAGS="-O2"
[ "${SANITIZE:-0}" = 1 ] && FLAGS="-O1 -g -fsanitize=thread"
g++ $FLAGS -std=gnu++17 -Wall -pthread -I"$HERE/../../src/sound" -o "$OUT/test" "$HERE/test.cpp"
"$OUT/test"
