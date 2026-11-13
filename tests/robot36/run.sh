#!/bin/bash
# Robot modes against MMSSTV (issue #207): Robot 24, 36, 72 and B/W 8, 12.
#
#  1. TX levels: our --encode of flat pictures is analysed: the tone of the luminance and of the R-Y / B-Y parts must be what MMSSTV
#     sends (Y in studio swing 16..235, chroma centred on 128; levels.py).
#  2. RX: a transmission made the way MMSSTV makes it (mmsstv_robot.py, a port of its LineR24/R36/R72/RM) is decoded and the colours
#     of the patches of the test picture are checked (check_rx.py): MMSSTV's black and white must come out black and white.
#  3. Round trip: our own transmission decoded by our own receiver, the same check.
#  4. R36 timing of our transmission: 150 ms line, separator about 4.5 ms (analyze.py).
#
#   tests/robot36/run.sh                    # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/robot36/run.sh
#
# Needs: the built qsstv-experimental, python3 with numpy and Pillow. The application runs with a throw-away HOME.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
PY=python3
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export QT_QPA_PLATFORM=offscreen
app() { HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share "$QSSTV" "$@" 2>&1 | grep -v VDPAU; }
mkdir -p "$T/home"
pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }

$PY "$HERE/check_rx.py" --make "$T/test.png" || { echo "python3 with numpy and Pillow is needed"; exit 2; }
for c in "gray 128 128 128" "white 255 255 255" "black 0 0 0" "red 255 0 0" "blue 0 0 255" "orange 230 100 40"; do
  set -- $c; $PY -c "
from PIL import Image; Image.new('RGB',(320,240),($2,$3,$4)).save('$T/f_$1.png')"
done

echo "== 1. TX levels (flat pictures)"
for m in R36 R72 R24 BW8 BW12; do
  for c in "gray 128 128 128" "white 255 255 255" "black 0 0 0" "red 255 0 0" "blue 0 0 255" "orange 230 100 40"; do
    set -- $c
    app --encode "$T/f_$1.png" --mode $m --wav-out "$T/t.wav" >/dev/null
    out=$($PY "$HERE/levels.py" $m "$T/t.wav" $2 $3 $4); rc=$?
    [ $rc = 0 ] && ok "$m $1" || { bad "$m $1"; echo "$out"; }
  done
done

echo "== 2. RX of MMSSTV transmissions"
for m in R36 R72 R24 BW8 BW12; do
  g=; case $m in BW*) g=--gray;; esac
  $PY "$HERE/mmsstv_robot.py" $m "$T/test.png" "$T/mm_$m.wav"
  rm -rf "$T/o"; mkdir "$T/o"
  app --batch -o "$T/o" "$T/mm_$m.wav" >"$T/out.txt"
  f=$(ls "$T/o"/*.png 2>/dev/null | head -1)
  if [ -z "$f" ]; then bad "$m: no picture received"; continue; fi
  out=$($PY "$HERE/check_rx.py" "$f" $g) && ok "$m: levels of the picture" || { bad "$m: levels of the picture"; echo "$out"; }
done

echo "== 3. Round trip, our TX and our RX"
for m in R36 R72 R24 BW8 BW12; do
  g=; case $m in BW*) g=--gray;; esac
  app --encode "$T/test.png" --mode $m --wav-out "$T/rt_$m.wav" >/dev/null
  rm -rf "$T/o"; mkdir "$T/o"
  app --batch -o "$T/o" "$T/rt_$m.wav" >"$T/out.txt"
  f=$(ls "$T/o"/*.png 2>/dev/null | head -1)
  if [ -z "$f" ]; then bad "$m: no picture received"; continue; fi
  out=$($PY "$HERE/check_rx.py" "$f" $g) && ok "$m: levels of the picture" || { bad "$m: levels of the picture"; echo "$out"; }
done

echo "== 4. R36 timing of our transmission"
app --encode "$T/f_orange.png" --mode R36 --wav-out "$T/t.wav" >/dev/null
$PY "$HERE/analyze.py" "$T/t.wav" --lines 8 >"$T/an.txt"
per=$(sed -n 's/.*periods ms: //p' "$T/an.txt")
bp=$(echo $per | tr ' ' '\n' | awk '{s+=$1;n++} END{m=s/n; print (m<149.9||m>150.1)}')   # the first pulses are found 1 ms off, the mean is right
[ "$bp" = 0 ] && ok "line period 150 ms" || bad "line period: $per"
sep=$(sed -n 's/.*separator [0-9]* Hz \([0-9.]*\) ms.*/\1/p' "$T/an.txt")
bs=$(echo $sep | tr ' ' '\n' | awk 'BEGIN{b=0} {if($1<4.2||$1>4.8)b=1} END{print b}')
[ "$bs" = 0 ] && ok "separator about 4.5 ms" || bad "separator: $sep"

echo
echo "$pass passed, $fail failed"
[ $fail = 0 ]
