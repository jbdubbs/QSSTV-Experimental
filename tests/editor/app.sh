#!/bin/bash
# Runs the template editor inside the real application objects, offscreen, with a throwaway HOME: opens it from the TX tab,
# saves, checks the gallery / TX template list, closes and reopens, and hands a picture to a fake external editor.
# Links the object files of an existing build: tests/editor/app.sh [build-dir]   (default: build-cmake)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-cmake}
cd "$B"
if [ -n "$KEEP" ]; then SCRATCH=$KEEP; mkdir -p "$SCRATCH"; else SCRATCH=$(mktemp -d); trap 'rm -rf "$SCRATCH"' EXIT; fi
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -I"$HERE/../../src/editor" -fPIC -c "${APP_CPP:-$HERE/app.cpp}" -o "$SCRATCH/app.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/app.o #; s#-o qsstv-experimental#-o $SCRATCH/app#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
rm -rf "$SCRATCH/home"
mkdir -p "$SCRATCH/home"
if [ -n "$GDB" ]; then gdb -batch -ex run -ex bt --args "$SCRATCH/app" "$SCRATCH/home" "$HERE/../template/fixtures"; exit; fi
timeout 120 "$SCRATCH/app" "$SCRATCH/home" "$HERE/../template/fixtures"
