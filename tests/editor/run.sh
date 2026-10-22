#!/bin/sh
# Tests of the template editor (src/editor): undo/redo invariants, random operation sequences, the canvas driven with real
# mouse and keyboard events, the properties/layers panels, file handling and one check per defect of the old editor.
# Builds the editor standalone (no application globals, no hamlib) and runs it with QT_QPA_PLATFORM=offscreen.
#   tests/editor/run.sh
#   tests/editor/app.sh              the same editor inside the real application objects (needs build-cmake)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${TMPDIR:-/tmp}/editor-test.$$
trap 'rm -rf "$OUT"' EXIT
"$HERE/build.sh" "$OUT" "$HERE/test.cpp"
QT_QPA_PLATFORM=offscreen "$OUT/test" "$HERE/../template/fixtures"
