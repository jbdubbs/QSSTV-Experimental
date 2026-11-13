#!/bin/bash
# A CAT start that fails lets go of everything, so a corrected start works at once (issue #211): the real rigControl, Hamlib's dummy rig,
# a pseudo terminal as the port of a radio that does not answer. Links the object files of an existing build:
# tests/catrestart/app.sh [build-dir]   (default: build-cmake)
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
timeout 120 "$SCRATCH/app"
