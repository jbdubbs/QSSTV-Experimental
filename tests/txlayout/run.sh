#!/bin/bash
# The transmitter puts the sync, porches, separators and pixels of a Martin, Scottie or SC2 line where MMSSTV does.
#
# QSSTV's Martin 1/2 had no porch after the sync (the standard's is 0.572 ms) and a 5.0 ms sync (4.862), Scottie 1 a 0.8 ms gap in front of
# the sync and a 0.85 ms porch (1.5), SC2 a sync 0.5 ms short and no porch (0.5): the video was 0.3 to 1.0 ms early, up to 4 px on SC2-60.
# Each case transmits a picture with a red, a green and a blue step through the real transmitter (--encode) and compares the instants of
# the transitions of a line with those of the line MMSSTV 1.13 sends (a port of its LineMRT/LineSCT/LineSC2180/LineP/LineMP/LineMR).
#
#   tests/txlayout/run.sh                       # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/txlayout/run.sh
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
for spec in M1:320:256 M2:320:256 S1:320:256 S2:320:256 SDX:320:256 SC2-60:320:256 SC2-120:320:256 SC2-180:320:256 \
            P3:640:496 MP73:320:256 MR73:320:256 ML180:640:496 MP73-N:320:256 MC110-N:320:256 MC140-N:320:256 MC180-N:320:256; do
  IFS=: read m w h <<<"$spec"
  HOME=$REAL_HOME python3 -c "
from PIL import Image
w,h=$w,$h
im=Image.new('RGB',(w,h)); px=im.load()
for x in range(w):
  c=(255 if x>=w//4 else 0, 255 if x>=w//2 else 0, 255 if x>=3*w//4 else 0)
  for y in range(h): px[x,y]=c
im.save('$T/$m.png')" || exit 2
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || { echo "  FAIL $m: no transmission"; fail=$((fail+1)); continue; }
  if out=$(HOME=$REAL_HOME python3 "$HERE/audit.py" $m "$T/$m.wav" $w $h); then echo "  ok   $out"
  else echo "  FAIL $out"; fail=$((fail+1)); fi
done
echo "$fail failed"
[ $fail = 0 ]
