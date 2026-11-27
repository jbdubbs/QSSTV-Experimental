#!/bin/bash
# Regression check for issue #216: the receiver must straighten a picture whose sound clock (or the transmitter's) is off by up to a few
# percent, like MMSSTV does (it handled +-2 % clean and 1 % at 5 dB SNR, checked with its own recordings 2026-11-26).
# Before the fix only about 0.2 % worked (the chain of sync pulses only holds pulses within 0.8 % of a line, and regression() threw the
# pulses of later lines out as false at 150 samples from the grid); a picture with a clock error of 0.5 % to 2 % came out sheared.
#
# Every case: a picture with one vertical line, sent by the real transmitter, played with the receiver clock off by D (impair.py), optionally
# with noise, decoded with the real receiver (--batch, Auto slant on); the line must come out within 2.5 px of straight over the picture.
#   tests/slant/sweep/check.sh      (needs build-cmake/qsstv-experimental or QSSTV_BIN, python3 with numpy and scipy; tests/slant/slantpng is built here)
HERE=$(cd "$(dirname "$0")" && pwd)
Q=${QSSTV_BIN:-$HERE/../../../build-cmake/qsstv-experimental}
[ -x "$Q" ] || { echo "qsstv-experimental not found at $Q (set QSSTV_BIN)"; exit 2; }
export QSSTV_BIN=$Q
QT=${QT_INCLUDE:-/usr/include/qt6}
SRC="$HERE/../../../src/sound"
g++ -O2 -fPIC -std=gnu++17 -w -I"$SRC" -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -o "$HERE/../slantpng" "$HERE/../slantpng.cpp" "$SRC/slantfit.cpp" -lQt6Gui -lQt6Core || exit 2
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/tmp"
"$HERE/prep.sh" "$T/clean" > /dev/null || exit 2
fail=0
# mode clock snr
while read -r m c s; do
  "$HERE/sweep.py" --clean "$T/clean" --tmp "$T/tmp" --out "$T/r.tsv" --modes "$m" --clocks "$c" --snrs "$s" --workers 1
  row=$(tail -1 "$T/r.tsv")
  valid=$(echo "$row" | cut -f6); rows=$(echo "$row" | cut -f7); shift=$(echo "$row" | cut -f9)
  got=${rows%/*}; tot=${rows#*/}
  if [ "$valid" = 1 ] && [ "$got" -ge $((tot/2)) ] && awk -v s="$shift" 'BEGIN{exit !(s<=2.5 && s>=-2.5)}'; then echo "  ok   $m clock $c snr $s: shift $shift px, rows $rows"
  else echo "  FAIL $m clock $c snr $s: shift $shift px, rows $rows"; fail=1; fi
done <<CASES
S2 0.005 none
S2 -0.005 none
S2 0.01 none
S2 -0.01 none
S2 0.02 none
R36 0.01 none
R36 -0.02 none
M2 0.01 none
PD50 0.02 none
S2 0.005 5
S2 0.01 5
R36 0.01 5
R36 -0.005 5
CASES
[ $fail = 0 ] && echo "all passed" || echo "FAILED"
exit $fail
