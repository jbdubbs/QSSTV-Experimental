#!/bin/bash
# Order of release tags in the update check (updateChecker::compareVersions, src/utils/updatecheck.cpp).
# Builds src/utils/updatecheck.cpp against a stub appglobal.h, so it needs only Qt6 Core and Network and moc.
#
#   tests/updatecheck/run.sh
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../../src/utils
MOC=${MOC:-$(command -v moc || command -v moc-qt6 || ls /usr/lib64/qt6/libexec/moc /usr/lib/qt6/libexec/moc 2>/dev/null | head -1)}
[ -x "$MOC" ] || { echo "moc not found (set MOC)"; exit 2; }
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
FLAGS=$(pkg-config --cflags --libs Qt6Core Qt6Network) || { echo "Qt6 Core and Network are needed"; exit 2; }
"$MOC" "$SRC/updatecheck.h" -o "$T/moc_updatecheck.cpp" || exit 2
g++ -std=c++17 -fPIC -I"$HERE/stub" -I"$SRC" $FLAGS -o "$T/test" "$HERE/test.cpp" "$SRC/updatecheck.cpp" "$T/moc_updatecheck.cpp" || exit 2
"$T/test"
