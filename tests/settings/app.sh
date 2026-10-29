#!/bin/bash
# The Transmit and Receive tab settings survive a restart, also when the program is killed (issue #149): the real application
# objects, offscreen, with a throw-away HOME. Phase "set" changes the settings and ends without a clean quit (no writeSettings(),
# like a Windows shutdown, End Task or a crash); phase "check" starts again on the same settings and looks at the boxes.
# Links the object files of an existing build: tests/settings/app.sh [build-dir]   (default: build-cmake)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-cmake}
cd "$B"
SCRATCH=$(mktemp -d); trap 'rm -rf "$SCRATCH"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/app.cpp" -o "$SCRATCH/app.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/app.o #; s#-o qsstv-experimental#-o $SCRATCH/app#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
mkdir -p "$SCRATCH/home"
echo "== phase 1: change the settings, then die without a clean quit"
timeout 120 "$SCRATCH/app" "$SCRATCH/home" set
echo
echo "== phase 2: start again"
timeout 120 "$SCRATCH/app" "$SCRATCH/home" check
