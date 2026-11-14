#!/bin/bash
# The PD transmitter puts the four segments of a line where the standard has them (#212).
#
# QSSTV used a 2.30 ms porch for PD (the standard's is 2.08 ms), which made the segments 0.05 ms short and put the second
# luminance segment 0.14 ms (half a pixel of PD50) too early relative to the first: the edges of a picture zig-zagged from row
# to row, and the colors were off the luminance. Each case transmits a picture that is black left of pixel 100 and white from
# there through the real transmitter (--encode), and measures the instants of the step in the two luminance segments.
#
#   tests/pdtiming/run.sh                       # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/pdtiming/run.sh
# Needs python3 with numpy, scipy and Pillow.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
REAL_HOME=$HOME
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"
fail=0
for spec in PD50:320x256 PD90:320x256 PD120:640x496; do
  m=${spec%%:*}; size=${spec#*:}
  HOME=$REAL_HOME python3 -c "
from PIL import Image
w,h=map(int,'$size'.split('x'))
im=Image.new('RGB',(w,h),(0,0,0)); im.paste((255,255,255),(100,0,w,h)); im.save('$T/$m.png')" || exit 2
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || { echo "  FAIL $m: no transmission"; fail=$((fail+1)); continue; }
  HOME=$REAL_HOME python3 "$HERE/measure.py" "$T/$m.wav" $m || fail=$((fail+1))
done
echo "$fail failed"
[ $fail = 0 ]
