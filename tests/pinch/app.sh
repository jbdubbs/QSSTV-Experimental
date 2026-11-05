#!/bin/bash
# The touch zoom of the picture viewer (tests/pinch/pinch.cpp): pinch in and out, pan with one finger, double tap.
#   tests/pinch/app.sh [build-dir]        build-dir default build-cmake (the desktop build, rebuilt first)
# Exits 1 when a check fails.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-cmake}
cd "$B"
SCRATCH=$(mktemp -d); trap 'rm -rf "${SCRATCH:?}"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/pinch.cpp" -o "$SCRATCH/pinch.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/pinch.o #; s#-o qsstv-experimental#-o $SCRATCH/pinch#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
mkdir -p "$SCRATCH/home"
timeout 120 "$SCRATCH/pinch" "$SCRATCH/home"
