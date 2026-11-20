#!/bin/bash
# Every mode: our transmitter and our receiver agree.
#
# Each mode transmits a picture of its own size with three steps (red, green and blue, or three grays for the B/W modes and FAX480) through the real
# transmitter (--encode) and the receiver (--batch, Auto) reads it twice: as sent (the VIS code starts the picture) and with everything before the end
# of the VIS code cut off (the sync pulses find it; AVT has none). Checks: the receiver names the mode that was sent, the picture has the size it
# should (FAX480 comes out as 500 lines: the 20 white phasing lines are shown), and the steps are within 0.3 ms of where they were sent when the VIS code starts the picture and 0.45 ms when the pulses find it
# (one pixel is 0.2 to 1.1 ms). tests/txlayout checks the transmitter against MMSSTV for the modes that MMSSTV has. JB60 is measured on luma: its chroma
# (the red and blue steps) comes out about 0.85 ms later than its luma, a property of its own chroma slots and filters (tests/jb60_loopback).
#
#   tests/modes/run.sh                          # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/modes/run.sh
#   MODES="M1 S1" tests/modes/run.sh            # some of them
# Needs python3 with numpy and Pillow. It takes about 25 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export SCR=$T
fail=0
# mode, size sent, pixel ms, kind, size received, first row of the picture
while read -r m w h px kind rw rh r0; do
  [ -n "$MODES" ] && ! [[ " $MODES " == *" $m "* ]] && continue
  read -r mode det size vis pulse < <(HOME=$HOME python3 "$HERE/roundtrip.py" $m $w $h $px $kind $rw $rh $r0 "$QSSTV" 2>/dev/null | tail -1)
  bad=""
  [ "$det" = "$m" ] || bad="$bad detected as $det;"
  [ "$size" = "${rw}x${rh}" ] || bad="$bad size $size (expected ${rw}x${rh});"
  awk -v a="$vis" 'BEGIN{exit !(a!="nan" && a<=0.3 && a>=-0.3)}' || bad="$bad VIS start $vis ms;"
  # (the pulse detector reads a start that depends on the sample phase of its input, by up to 0.1 ms between phases on most modes and 0.3 on Robot 72 and
  # P7, so the limit for the start found by pulses is 0.45 ms; the VIS start has no such spread)
  if [ "$pulse" != "-" ]; then awk -v a="$pulse" 'BEGIN{exit !(a!="nan" && a<=0.45 && a>=-0.45)}' || bad="$bad pulse start $pulse ms;"; fi
  if [ -z "$bad" ]; then echo "  ok   $m: $size, VIS start $vis ms, pulse start $pulse ms"
  else echo "  FAIL $m:$bad (VIS start $vis ms, pulse start $pulse ms)"; fail=$((fail+1)); fi
done <<'LIST'
M1 320 256 0.4576 rgb 320 256 0
M2 320 256 0.2288 rgb 320 256 0
S1 320 256 0.432 rgb 320 256 0
S2 320 256 0.2752 rgb 320 256 0
SDX 320 256 1.08 rgb 320 256 0
SC2-60 320 256 0.2442 rgb 320 256 0
SC2-120 320 256 0.4891 rgb 320 256 0
SC2-180 320 256 0.7344 rgb 320 256 0
R24 160 120 0.55 rgb 160 120 0
R36 320 240 0.275 rgb 320 240 0
R72 320 240 0.43125 rgb 320 240 0
BW8 160 120 0.3681 gray 160 120 0
BW12 160 120 0.575 gray 160 120 0
P3 640 496 0.2083 rgb 640 496 0
P5 640 496 0.3125 rgb 640 496 0
P7 640 496 0.4167 rgb 640 496 0
PD50 320 256 0.286 rgb 320 256 0
PD90 320 256 0.532 rgb 320 256 0
PD120 640 496 0.19 rgb 640 496 0
PD120W 768 432 0.19 rgb 768 432 0
PD160 512 400 0.382 rgb 512 400 0
PD180 640 496 0.286 rgb 640 496 0
PD240 640 496 0.382 rgb 640 496 0
PD290 800 616 0.286 rgb 800 616 0
MP73 320 256 0.4375 rgb 320 256 0
MP115 320 256 0.6969 rgb 320 256 0
MP140 320 256 0.84375 rgb 320 256 0
MP175 320 256 1.0625 rgb 320 256 0
MR73 320 256 0.43125 rgb 320 256 0
MR90 320 256 0.534 rgb 320 256 0
MR115 320 256 0.6875 rgb 320 256 0
MR140 320 256 0.84 rgb 320 256 0
MR175 320 256 1.053 rgb 320 256 0
ML180 640 496 0.2758 rgb 640 496 0
ML240 640 496 0.3695 rgb 640 496 0
ML280 640 496 0.4336 rgb 640 496 0
ML320 640 496 0.4961 rgb 640 496 0
MP73-N 320 256 0.4375 rgb 320 256 0
MP110-N 320 256 0.6625 rgb 320 256 0
MP140-N 320 256 0.84375 rgb 320 256 0
MC110-N 320 256 0.4375 rgb 320 256 0
MC140-N 320 256 0.5625 rgb 320 256 0
MC180-N 320 256 0.725 rgb 320 256 0
FAX480 512 480 0.512 gray 512 500 20
AVT24 128 120 0.4883 rgb:nopulse 128 120 0
AVT90 320 240 0.3906 rgb:nopulse 320 240 0
AVT94 320 200 0.3906 rgb:nopulse 320 200 0
JB60 640 496 0.36 gray 640 496 0
LIST
echo "$fail failed"
[ $fail = 0 ]
