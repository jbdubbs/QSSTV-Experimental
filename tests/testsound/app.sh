#!/bin/bash
# Settings > Audio and radio > Test sound sends a short SSTV picture, B/W 8 (issue #185): the real application objects, offscreen, with a
# throw-away HOME. The picture is recorded to a wav file (no sound card here), checked for its length, restoring of the Transmit screen, the
# digital page, Stop, refusal and a missing sound output (app.cpp), and then decoded by the application itself: it must come back as B/W 8.
# Links the object files of an existing Qt Quick build: tests/testsound/app.sh [build-dir]   (default: build-qml, cmake -S src -B build-qml -DQSSTV_QML_UI=ON)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-qml}
cd "$B"
SCRATCH=$(mktemp -d); [ -n "$KEEP" ] || trap 'rm -rf "${SCRATCH:?}"' EXIT
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/app.cpp" -o "$SCRATCH/app.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/app.o #; s#-o qsstv-experimental#-o $SCRATCH/app#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
mkdir -p "$SCRATCH/home"
rc=0
timeout 300 "$SCRATCH/app" "$SCRATCH/home" || rc=$?
[ -n "$KEEP" ] && echo "scratch kept: $SCRATCH"
[ $rc -eq 0 ] || exit $rc
# the recording, decoded by the application itself (headless batch decode): a B/W 8 picture of 160 x 120
echo
echo "== the recording decodes as B/W 8"
mkdir -p "$SCRATCH/dec-home" "$SCRATCH/out"
HOME="$SCRATCH/dec-home" XDG_CONFIG_HOME="$SCRATCH/dec-home/config" XDG_DATA_HOME="$SCRATCH/dec-home/data" QT_QPA_PLATFORM=offscreen \
  timeout 120 "$B/qsstv-experimental" --batch --out-dir "$SCRATCH/out" "$SCRATCH/home/sample1.wav" > "$SCRATCH/decode.txt" 2> "$SCRATCH/decode.err" || true
cat "$SCRATCH/decode.txt"
if grep -qiE "BW8|B/W 8" "$SCRATCH/decode.txt" && ls "$SCRATCH"/out/* > /dev/null 2>&1; then echo "  ok   decoded as B/W 8"; else echo "  FAIL not decoded as B/W 8"; cat "$SCRATCH/decode.err" | tail -5; exit 1; fi
