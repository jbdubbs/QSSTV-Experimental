#!/bin/sh
# JPEG 2000 vs AVIF at equal byte budgets (#108). Needs Qt6 Core/Gui, libopenjp2-dev and libavif-dev (with the libaom encoder).
#   tests/avif/run.sh [image ...]     (default: tests/jb60_loopback/images/card.png)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
OUT=${TMPDIR:-/tmp}/avif-test.$$
MOC=${MOC:-$(command -v moc-qt6 || echo /usr/lib64/qt6/libexec/moc)}
trap 'rm -f "$OUT" "$OUT.moc.cpp"' EXIT
cd "$SRC"
"$MOC" utils/jp2io.h -o "$OUT.moc.cpp"
g++ -O2 -fPIC -std=gnu++17 -w -I"$HERE/../jp2io/stubs" -Iutils -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" \
    -DHAVE_LIBAVIF $(pkg-config --cflags libopenjp2 libavif) \
    -o "$OUT" "$HERE/test.cpp" utils/avifenc.cpp utils/jp2io.cpp utils/color.cpp "$OUT.moc.cpp" \
    $(pkg-config --libs libopenjp2 libavif) -lQt6Gui -lQt6Core
[ $# -eq 0 ] && set -- "$HERE/../jb60_loopback/images/card.png"
QT_QPA_PLATFORM=offscreen "$OUT" "$@"
