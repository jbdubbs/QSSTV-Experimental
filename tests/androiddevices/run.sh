#!/bin/bash
# The rule for which Android audio devices Settings offers (src/android/androiddevicetypes.h), on the desktop.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "${OUT:?}"' EXIT
c++ -std=c++17 -I"$HERE/../../src/android" "$HERE/test.cpp" -o "$OUT/test"
"$OUT/test"
