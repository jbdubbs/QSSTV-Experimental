#!/bin/bash
# The wait cursor (issue #195: Qt's wait cursor crashed a macOS 26 start, once per launch, in txWidget::slotSizeApply).
#   unit.cpp  src/utils/busycursor.h on its own, built twice: as it is, and with -DQSSTV_NO_WAIT_CURSOR (the macOS behaviour)
#   app.cpp   the real application objects, offscreen, throw-away HOME: no wait cursor while the program starts, one when the user moves the size slider
# Links the object files of an existing build: tests/busycursor/app.sh [build-dir]   (default: build-cmake)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-cmake}
cd "$B"
SCRATCH=$(mktemp -d -p "${TMPDIR:-/tmp}"); trap 'rm -rf "${SCRATCH:?}"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
UTILS=$HERE/../../src/utils
for v in "" "-DQSSTV_NO_WAIT_CURSOR"; do
  eval /usr/bin/c++ $CXXF $FLAGS $INCS $v -I"$UTILS" -fPIC "$HERE/unit.cpp" -o "$SCRATCH/unit" -lQt6Gui -lQt6Core
  "$SCRATCH/unit"
done
eval /usr/bin/c++ $CXXF $FLAGS $INCS -I"$UTILS" -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/app.cpp" -o "$SCRATCH/app.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/app.o #; s#-o qsstv-experimental#-o $SCRATCH/app#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
mkdir -p "$SCRATCH/home"
timeout 120 "$SCRATCH/app" "$SCRATCH/home"
