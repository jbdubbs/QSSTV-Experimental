#!/bin/sh
# Tests of src/template (the template document, the legacy .templ importer, the JSON format and the headless renderer):
#   - the renderer reproduces the legacy editor's output (golden images made by legacy-golden/gen.cpp)
#   - JSON round trip, value validation and limits, macro expansion
#   - fuzzing of the legacy loader (every truncation, thousands of byte mutations: no crash, no hang)
# Builds a small standalone binary; needs Qt6 Core/Gui headers (no application globals, no hamlib, no GUI session).
#   tests/template/run.sh          TPL_FUZZ_ROUNDS=50000 tests/template/run.sh   (longer fuzz)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
OUT=${TMPDIR:-/tmp}/template-test.$$
trap 'rm -f "$OUT"' EXIT
g++ -O1 -fPIC -std=gnu++17 -Wall -Wextra -Wno-unused-parameter -I"$SRC/template" -I"$SRC/utils" -isystem "$QT" -isystem "$QT/QtCore" -isystem "$QT/QtGui" \
    -o "$OUT" "$HERE/test.cpp" "$SRC/template/templatedoc.cpp" "$SRC/template/templatelegacy.cpp" "$SRC/template/templaterender.cpp" \
    "$SRC/utils/macroexpansion.cpp" -lQt6Gui -lQt6Core
"$OUT" "$HERE/fixtures" "$HERE/golden"
