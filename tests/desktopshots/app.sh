#!/bin/bash
# Screenshots of the desktop interface from a build tree, to compare two builds (this branch and main):
#   tests/desktopshots/app.sh <build-dir> <out-dir> [width height]
# then: cmp the files of the two out-dirs, or look at them. Needs a configured and built tree (the sources are read from the flags of that tree).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:?build dir}
OUT=${2:?output dir}
W=${3:-1100}; H=${4:-720}
cd "$B"
SCRATCH=$(mktemp -d); trap 'rm -rf "${SCRATCH:?}"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/shot.cpp" -o "$SCRATCH/shot.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/shot.o #; s#-o qsstv-experimental#-o $SCRATCH/shot#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
mkdir -p "$SCRATCH/home" "$OUT"
timeout 180 "$SCRATCH/shot" "$SCRATCH/home" "$OUT" "$W" "$H"
