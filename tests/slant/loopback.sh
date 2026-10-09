#!/bin/bash
# End-to-end check of the Calibrate > "Other SSTV station" measurement with the real application.
# A PD120 recording of a picture with one vertical line is decoded (QSSTV engine, Auto Slant off) with the
# receive clock deliberately set wrong; the line slants, and the clock the tab would propose
# (rxclock * (1 + slope * rowsPerPeriod / pixelsPerPeriod)) must come back to 48000 within a few ppm.
#
#   tests/slant/loopback.sh                       # uses ../../build-cmake/qsstv
#   QSSTV_BIN=/path/to/qsstv tests/slant/loopback.sh
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv}
LB="$HERE/../jb60_loopback/loopback"
[ -x "$QSSTV" ] || { echo "qsstv not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
[ -x "$LB" ] || (cd "$HERE/../jb60_loopback" && ./build.sh) || exit 2
SRC="$HERE/../../src/sound"
QT=${QT_INCLUDE:-/usr/include/qt6}
g++ -O2 -fPIC -std=gnu++17 -w -I"$SRC" -I"$QT" -I"$QT/QtCore" -I"$QT/QtGui" -o "$HERE/slantpng" "$HERE/slantpng.cpp" "$SRC/slantfit.cpp" -lQt6Gui -lQt6Core || exit 2

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME/.config/ON4QZ" "$T/out"
CONF=$HOME/.config/ON4QZ/$(basename "$(find "$HERE/../../src" -name appglobal.cpp | head -1 | xargs grep -o 'CONFIGVERSION *= *"[^"]*"' | head -1 | sed 's/.*"\(.*\)"/qsstv_\1.conf/')")
[ -f "$CONF" ] || CONF=$HOME/.config/ON4QZ/qsstv_9.0.conf

"$HERE/slantpng" make "$T/line.png" 640 496 320
"$LB" pd --image "$T/line.png" --vis --wav "$T/pd120.wav" > /dev/null

# PD120: 2 picture rows per line period; 0.50848 s per period, 0.1216 s for 640 pixels -> 2676.1 pixels per period
fail=0
for d in 0.0001 -0.0001 0.0003; do
  clk=$(awk -v d=$d 'BEGIN{printf "%.6f",48000*(1+d)}')
  printf '[SOUND]\nrxclock=%s\n' "$clk" > "$CONF"
  rm -f "$T"/out/*
  "$QSSTV" --batch --out-dir "$T/out" "$T/pd120.wav" > /dev/null 2>&1
  res=$("$HERE/slantpng" fit "$T"/out/*.png)
  slope=$(echo "$res" | sed 's/.*slope=\([-0-9.]*\).*/\1/')
  new=$(awk -v c="$clk" -v s="$slope" 'BEGIN{printf "%.3f",c*(1+s*2/2676.1)}')
  err=$(awk -v n="$new" 'BEGIN{printf "%.1f",(n/48000-1)*1e6}')
  if awk -v e="$err" 'BEGIN{exit !(e<5 && e>-5)}'; then echo "  ok   rxclock offset $d: proposed $new Hz (residual $err ppm)"
  else echo "  FAIL rxclock offset $d: proposed $new Hz (residual $err ppm) [$res]"; fail=1; fi
done

# Several pictures in one recording (Listen stays on between pictures): from the 2nd on, a picture sent by the real
# transmitter is shifted sideways a few pixels and shows the end of the previous line in its last column, which must
# not hide the line.
printf '[SOUND]\nrxclock=48000\n' > "$CONF"
"$QSSTV" --encode "$T/line.png" --mode PD120 --wav-out "$T/tx.wav" > /dev/null 2>&1
python3 - "$T/tx.wav" "$T/three.wav" <<'PY'
import sys,wave
raw=open(sys.argv[1],'rb').read()[44:]   # 48 kHz 16 bit stereo; the streamed header has no valid length
o=wave.open(sys.argv[2],'wb'); o.setnchannels(2); o.setsampwidth(2); o.setframerate(48000)
sil=b'\0'*(4*48000*8)
o.writeframes(raw+sil+raw+sil+raw+sil); o.close()
PY
rm -f "$T"/out/*
"$QSSTV" --batch --out-dir "$T/out" "$T/three.wav" > /dev/null 2>&1
n=0
for f in "$T"/out/*.png; do
  n=$((n+1))
  res=$("$HERE/slantpng" fit "$f")
  if echo "$res" | grep -q "valid=1"; then echo "  ok   picture $n of a multi picture recording: line found"
  else echo "  FAIL picture $n of a multi picture recording: no line [$res]"; fail=1; fi
done
[ $n -eq 3 ] || { echo "  FAIL expected 3 pictures, got $n"; fail=1; }
exit $fail
