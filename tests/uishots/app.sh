#!/bin/bash
# Screenshots and size checks of the real main window in the desktop and touch shells (tests/uishots/shot.cpp).
#   tests/uishots/app.sh [build-dir] [out-dir]        build-dir default build-cmake, out-dir default a new folder under /tmp
# Prints each page's minimum size and exits 1 if a touch shell's window will not get as narrow as the screen.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-cmake}
OUT=${2:-}
cd "$B"
SCRATCH=$(mktemp -d); trap 'rm -rf "${SCRATCH:?}"' EXIT
[ -n "$OUT" ] || OUT=$(mktemp -d -t qsstv-uishots-XXXXXX)   # kept, so the pictures can be looked at
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/shot.cpp" -o "$SCRATCH/shot.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/shot.o #; s#-o qsstv-experimental#-o $SCRATCH/shot#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
rc=0
# one run per size: the main window keeps its size between pages, so each size starts afresh
run() {
  local shell=$1 w=$2 h=$3; shift 3
  mkdir -p "$SCRATCH/home-$shell-$w"
  timeout 120 "$SCRATCH/shot" "$SCRATCH/home-$shell-$w" "$OUT" "$shell" "$w" "$h" "$@" || rc=1
}
if [ -n "${SIZES:-}" ]; then
  for spec in $SIZES; do IFS=: read -r shell w h <<< "$spec"; run "$shell" "$w" "$h" rx tx gallery gallerytabs more settings galleryselect overscroll; done
else
  run desktop 900 600 rx tx gallery
  run phone 411 800 rx tx gallery gallerytabs more settings galleryselect overscroll
  run phone 360 640 rx tx gallery gallerytabs more settings galleryselect overscroll
  run phone 850 411 rx tx gallery gallerytabs more settings galleryselect overscroll
  run tablet 800 1200 rx tx gallery gallerytabs more settings galleryselect overscroll
fi
echo "screenshots: $OUT"
exit $rc
