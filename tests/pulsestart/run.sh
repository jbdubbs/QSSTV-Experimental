#!/bin/bash
# A picture that the receiver finds by its sync pulses alone (no VIS code, or one that was missed) lands where the transmitter put it.
#
# The receiver puts the picture where the pulse it found ends, plus a compensation that depends on the mode (the layout of the line round its
# sync pulse; the slower sync filter of the narrow modes). It was one for all the wide modes, and a picture found by its pulses was 0.1 to 3 ms late
# (Scottie 1.5, Martin 0.9, Robot 72 2.2, MR and ML 2.5 to 3 ms), although one that a VIS code started was exact. Each case transmits a picture with
# a red, a green and a blue step through the real transmitter (--encode), decodes it as sent and with everything up to the end of its VIS code cut
# off, and compares where the steps are with where they were sent: within 0.3 ms both ways (one pixel is 0.2 to 1 ms).
#
#   tests/pulsestart/run.sh                     # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/pulsestart/run.sh
# Needs python3 with numpy and Pillow.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export SCR=$T
fail=0
# mode width height pixel_ms: one of each family
while read -r m w h px; do
  read -r mode vis pulse < <(HOME=$HOME python3 "$HERE/pulseonly.py" $m $w $h $px "$QSSTV" 2>/dev/null | tail -1)
  if awk -v a="$vis" -v b="$pulse" 'BEGIN{exit !(a!="nan" && b!="nan" && a<=0.3 && a>=-0.3 && b<=0.3 && b>=-0.3)}'; then
    echo "  ok   $m: VIS-started $vis ms, found by its pulses $pulse ms"
  else echo "  FAIL $m: VIS-started $vis ms, found by its pulses $pulse ms (within 0.3 ms)"; fail=$((fail+1)); fi
done <<'LIST'
S1 320 256 0.432
SC2-120 320 256 0.4891
M1 320 256 0.4576
R36 320 240 0.275
R72 320 240 0.43125
P3 640 496 0.2083
PD120 640 496 0.19
MP73 320 256 0.4375
MR90 320 256 0.534
ML240 640 496 0.3695
MP73-N 320 256 0.4375
MC110-N 320 256 0.4375
LIST
echo "$fail failed"
[ $fail = 0 ]
