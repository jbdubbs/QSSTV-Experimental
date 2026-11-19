#!/bin/bash
# The clock per sound device (issue #194): tests/deviceclocks/build.sh   (compiles deviceclocks.cpp alone, in a scratch directory)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../../src
SCRATCH=$(mktemp -d); trap 'rm -rf "$SCRATCH"' EXIT
# soundconfig.h is only needed for its extern declarations: a stand-in with the same four
mkdir -p "$SCRATCH/inc"
cat > "$SCRATCH/inc/soundconfig.h" <<'EOT'
#include <QString>
extern double rxClock;
extern double txClock;
extern QString inputAudioDevice;
extern QString outputAudioDevice;
EOT
# a copy next to it, so that the quoted include finds the stand-in first
cp "$SRC/config/deviceclocks.cpp" "$SRC/config/deviceclocks.h" "$SCRATCH/inc/"
c++ -std=c++17 -fPIC $(pkg-config --cflags Qt6Core) -I"$SCRATCH/inc" -I"$SRC" \
  "$HERE/test.cpp" "$SCRATCH/inc/deviceclocks.cpp" -o "$SCRATCH/test" $(pkg-config --libs Qt6Core)
"$SCRATCH/test"
