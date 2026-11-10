#!/bin/bash
# The User's Guide web server (src/utils/guideserver.cpp) on a Qt resource tree, as on Android. No GUI, no network beyond loopback.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
LIBEXEC=$(qmake6 -query QT_HOST_LIBEXECS)
OUT=$(mktemp -d)
trap 'rm -f "${OUT:?}"/*; rmdir "${OUT:?}"' EXIT
"$LIBEXEC/moc" "$HERE/../../src/utils/guideserver.h" -o "$OUT/moc_guideserver.cpp"
"$LIBEXEC/rcc" --name guide "$HERE/guide.qrc" -o "$OUT/qrc_guide.cpp"
g++ -O1 -std=gnu++17 -fPIC -Wall $(pkg-config --cflags Qt6Core Qt6Network) -I"$HERE/../../src/utils" -I"$OUT" \
  -o "$OUT/test" "$HERE/test.cpp" "$HERE/../../src/utils/guideserver.cpp" "$OUT/moc_guideserver.cpp" "$OUT/qrc_guide.cpp" \
  $(pkg-config --libs Qt6Core Qt6Network)
cd "$HERE"   # the resource tree is compiled in; the working directory does not matter
QT_QPA_PLATFORM=offscreen "$OUT/test"
