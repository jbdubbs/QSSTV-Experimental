#!/bin/bash
# AVIF as a picture file format with no Qt AVIF plugin (issue #189, Android): the format lists, the save and the gallery reads.
# Links the object files of an existing build, which must have libavif and the Qt Quick interface (QSSTV_QML_UI=ON): tests/imagefmt/app.sh [build-dir]   (default: build-qml)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-qml}
cd "$B"
SCRATCH=$(mktemp -d -p "${TMPDIR:-/tmp}"); trap 'rm -rf "${SCRATCH:?}"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -fPIC -c "$HERE/app.cpp" -o "$SCRATCH/app.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/app.o #; s#-o qsstv-experimental#-o $SCRATCH/app#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK
mkdir -p "$SCRATCH/home"
HOME="$SCRATCH/home" timeout 120 "$SCRATCH/app"
