#!/bin/bash
# FAX480 as the SSTV Handbook (4.3.1) describes it, which is all there is: no other program to compare with (MMSSTV has no FAX480).
#
# The transmission is the VIS code (85, added to the mode later: the Handbook does not say where it goes; it comes first, so that a program that does
# not know it waits for the start tone as before), the start tone (1500 and 2300 Hz for 2.048 ms each, 1220 times: 4.997 s), 20 white phasing lines
# and the 480 lines of the picture; every line is a sync pulse (1200 Hz, 5.12 ms) and 512 pixels of 0.512 ms: 267.264 ms. QSSTV sent the tone with 2.05 ms tones, its lines
# with the sync after the pixels and 500 lines of the picture without phasing lines. Checks, with the real program:
#   - --encode of a 512 x 480 picture: the first sync pulse follows the start tone after 4.997 s, 500 pulses, one every 267.264 ms
#   - --batch (the mode forced and in Auto) receives it: 512 x 500, 20 white lines at the top, the picture below, its steps within 0.1 ms, and
#     in Auto the VIS code announces FAX480 (the lines are found by their pulses: the start tone is between the code and the first line)
#
#   tests/fax480/run.sh                         # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/fax480/run.sh
# Needs python3 with numpy, scipy and Pillow.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
REAL_HOME=$HOME
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"
py() { HOME=$REAL_HOME python3 "$@"; }
fail=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
within() { awk -v a="$1" -v c="$2" -v l="$3" 'BEGIN{exit !(a>=c-l && a<=c+l)}'; }

py -c "
from PIL import Image
W,H=512,480
im=Image.new('L',(W,H)); p=im.load()
for x in range(W):
  v=0 if x<W//4 else (96 if x<W//2 else (192 if x<3*W//4 else 255))
  for y in range(H): p[x,y]=v
im.convert('RGB').save('$T/fax.png')" || exit 2
"$QSSTV" --encode "$T/fax.png" --mode FAX480 --wav-out "$T/fax.wav" >/dev/null 2>&1 || { echo "cannot transmit the test picture"; exit 2; }

echo "== the transmission"
read -r tone first period pulses < <(py "$HERE/measure.py" "$T/fax.wav")
within "$tone" 4.9971 0.002 && ok "the start tone is $tone s (4.997 s)" || bad "the start tone is $tone s (4.997 s)"
within "$period" 267.264 0.005 && ok "a line is $period ms (267.264 ms)" || bad "a line is $period ms (267.264 ms)"
[ "$pulses" = 500 ] && ok "$pulses sync pulses (20 phasing lines and 480 lines)" || bad "$pulses sync pulses (500: 20 phasing lines and 480 lines)"

echo "== the reception"
for how in "--mode FAX480" ""; do
  label=${how:+forced}; label=${label:-Auto}
  h=$(mktemp -d -p "$T"); rm -rf "$T/o"; mkdir "$T/o"
  rm -f "$T/trace.tsv"
  HOME=$h XDG_CONFIG_HOME=$h/.config XDG_DATA_HOME=$h/.local/share QSSTV_RX_TRACE="$T/trace.tsv" "$QSSTV" --batch $how -o "$T/o" "$T/fax.wav" >/dev/null 2>&1
  grep -aq $'^[^\t]*\tVIS\t.*mode=FAX480\taccepted=1' "$T/trace.tsv" && ok "($label) the VIS code announces FAX480" || bad "($label) the VIS code is not accepted: $(grep -a VIS "$T/trace.tsv" | head -1 | cut -f2-8)"
  png=$(ls "$T/o"/*.png 2>/dev/null | head -1)
  if [ -z "$png" ]; then bad "($label) no picture received"; continue; fi
  read -r W H white first ms < <(py "$HERE/edges.py" "$png")
  if [ "$W" = 512 ] && [ "$H" = 500 ] && [ "$white" = 20 ] && [ "$first" = 20 ] && within "$ms" 0 0.1; then ok "($label) $W x $H, $white white lines at the top, steps $ms ms from where they were sent"
  else bad "($label) $W x $H, $white white lines at the top, the picture from row $first, steps $ms ms from where they were sent"; fi
done
echo "$fail failed"
[ $fail = 0 ]
