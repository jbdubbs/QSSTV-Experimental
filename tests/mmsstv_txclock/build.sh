#!/bin/sh
# Test that the calibrated transmit clock reaches the MMSSTV-engine TX bridge (see txclock_test.cpp).
# Needs mmsstv-core built (libmmsstv_core.a): MMSSTV_CORE_LIB (default ../../build-cmake/mmsstv-core/libmmsstv_core.a)
# and its sources for the headers: MMSSTV_CORE_DIR (default ../../../mmsstv-core).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
CORE=${MMSSTV_CORE_DIR:-$HERE/../../../mmsstv-core}
LIB=${MMSSTV_CORE_LIB:-$HERE/../../build-cmake/mmsstv-core/libmmsstv_core.a}
QT=${QT_INCLUDE:-/usr/include/qt6}
g++ -O2 -fPIC -std=gnu++17 -w -DQT_NO_DEBUG \
    -I"$HERE/stubs" -I"$SRC" -I"$SRC/utils" -I"$SRC/dsp" -I"$SRC/config" -I"$SRC/sstv" \
    -I"$CORE/src" -I"$CORE/compat" \
    -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -I"$QT/QtWidgets" \
    -o "$HERE/txclock_test" "$HERE/txclock_test.cpp" "$SRC/sstv/mmsstv_sstv_tx.cpp" "$LIB" \
    -lQt6Gui -lQt6Core
"$HERE/txclock_test"
