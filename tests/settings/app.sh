#!/bin/bash
# The Transmit and Receive tab settings survive a restart, also when the program is killed (issue #149): the real application
# objects, offscreen, with a throw-away HOME. Phase "set" changes the settings and ends without a clean quit (no writeSettings(),
# like a Windows shutdown, End Task or a crash); phase "check" starts again on the same settings and looks at the boxes; phase "gone"
# starts once more after the selected template was deleted. The pair is run again with JPEG2000 as the picture encoding.
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
rc=0
run() {
  if [ -n "$GDB" ]; then gdb -batch -ex run -ex bt --args "$SCRATCH/app" "$@" || rc=1; else timeout 120 "$SCRATCH/app" "$@" || rc=1; fi
}
mkdir -p "$SCRATCH/home" "$SCRATCH/home-jp2"
echo "== phase 1: change the settings, then die without a clean quit"
run "$SCRATCH/home" set
cp -a "$SCRATCH/home" "$SCRATCH/home-gone"   # phase 2 changes settings of its own (it ends with Hybrid off)
echo
echo "== phase 2: start again (another template now sorts in front of the saved one)"
run "$SCRATCH/home" check
echo
echo "== phase 3: start again, on a copy, after the saved template was deleted"
run "$SCRATCH/home-gone" gone
echo
echo "== the same with JPEG2000, the default picture encoding (its size range comes from the picture)"
run "$SCRATCH/home-jp2" set jp2
echo
run "$SCRATCH/home-jp2" check jp2
exit $rc
