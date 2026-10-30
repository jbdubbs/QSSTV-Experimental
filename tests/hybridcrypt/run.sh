#!/bin/sh
# Direct check of src/utils/hybridcrypt.cpp, the encryption of the FTP login that a Hybrid transmission carries (issue #155).
# Builds a small standalone binary against stub globals; needs Qt6 Core.
#   tests/hybridcrypt/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
OUT=${TMPDIR:-/tmp}/hybridcrypt-test.$$
trap 'rm -f "$OUT"' EXIT
cd "$SRC"
g++ -O1 -fPIC -std=gnu++17 -w -I"$HERE/stubs" -Iutils -I"$QT" -I"$QT/QtCore" \
    -o "$OUT" "$HERE/test.cpp" utils/hybridcrypt.cpp -lQt6Core
"$OUT"
