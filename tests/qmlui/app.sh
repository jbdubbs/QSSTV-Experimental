#!/bin/bash
# Screenshots of the Qt Quick interface (src/qml) on the desktop, offscreen, with the software scene graph (tests/qmlui/shot.cpp).
#   tests/qmlui/app.sh [build-dir] [out-dir]     build-dir default build-qml (cmake -S src -B build-qml -DQSSTV_QML_UI=ON)
# Sizes: a phone upright, a small phone, a phone on its side, a tablet; light and dark. The pictures are kept for looking at.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-qml}
OUT=${2:-}
cd "$B"
SCRATCH=$(mktemp -d -p "${TMPDIR:-/tmp}"); [ -n "$KEEP" ] || trap 'rm -rf "${SCRATCH:?}"' EXIT
[ -n "$OUT" ] || OUT=$(mktemp -d -t qsstv-qmlui-XXXXXX)
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -DSRC_TEMPLATE_FIXTURE=\\\"$HERE/../template/fixtures/synthetic.templ\\\" -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/shot.cpp" -o "$SCRATCH/shot.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/shot.o #; s#-o qsstv-experimental#-o $SCRATCH/shot#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
rc=0
run() {   # theme width height scenes...
  local theme=$1 w=$2 h=$3; shift 3
  mkdir -p "$SCRATCH/home-$theme-$w"
  QSSTV_QML_THEME=$theme timeout 120 "$SCRATCH/shot" "$SCRATCH/home-$theme-$w" "$OUT/$theme" "$w" "$h" "$@" || rc=1
}
SCENES=${QMLUI_SCENES:-"idle receiving options digital stopped quiet tx tx-picture tx-modes tx-details tx-digital tx-digital-details tx-tools tx-sending tx-sending-tool about gallery gallery-sent gallery-templates gallery-select gallery-viewer gallery-info gallery-delete saved gallery-empty settings settings-operator settings-pictures settings-format settings-position settings-advanced settings-audio settings-mic settings-radios mic-denied firstrun-1 firstrun-2"}
run light 411 800 $SCENES
run dark 411 800 $SCENES
run light 360 640 idle receiving tx-picture
run light 320 640 idle receiving tx-picture
run light 850 411 idle receiving tx-picture
run light 800 1200 idle receiving tx-picture
echo "screenshots: $OUT"
exit $rc
