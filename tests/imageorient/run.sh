#!/bin/bash
# Do pictures stored sideways (EXIF/HEIF orientation) come out the right way up through --encode, and does HEIC open at all?
#   run.sh <qsstv-experimental binary> <work dir> [plain|orientation|all]
# Every picture is sent as JB60 into a wav (--encode), decoded again (--batch) and compared with the card it was made from:
# a picture that was not turned upright decodes to something far from the card. The pass mark is the round trip of the plain
# PNG card minus 3 dB, which covers the HEIC/JPEG loss. HEIC steps need macOS (sips, swiftc); the rest also runs on Linux.
#   plain        the formats this build reads, an upright HEIC (macOS)
#   orientation  JPEG (and on macOS HEIC) with orientation tags 6, 8 and 3
set -u
[ $# -ge 2 ] || { echo "usage: $0 <qsstv-experimental binary> <work dir> [plain|orientation|all]" >&2; exit 2; }
APP=$1; W=$2; WHAT=${3:-all}
HERE=$(cd "$(dirname "$0")" && pwd)
CARD=$HERE/../jb60_loopback/images/card.png
PY=${PYTHON:-python3}
export QT_QPA_PLATFORM=offscreen
export HOME=$W/home XDG_CONFIG_HOME=$W/home/config XDG_DATA_HOME=$W/home/data    # settings are only read, but keep them out of the way
mkdir -p "$W" "$HOME"
fails=0
PSNR=0

roundtrip() {   # name picture: sets PSNR
  local name=$1 pic=$2 wav=$W/$1.wav out=$W/$1.out png size
  PSNR=0
  rm -rf "$out"; mkdir -p "$out"
  if ! "$APP" --encode "$pic" --mode JB60 --wav-out "$wav" 2>"$W/$name.err"; then
    echo "  --encode failed for $name:"; tail -3 "$W/$name.err"; return 1
  fi
  size=$(wc -c <"$wav" | tr -d ' ')
  [ "$size" -gt 6000000 ] || { echo "  $name: wav is only $size bytes"; return 1; }
  "$APP" --batch -o "$out" "$wav" >"$W/$name.txt" 2>&1
  png=$(ls "$out"/*.png 2>/dev/null | head -1)
  [ -n "$png" ] || { echo "  $name: no picture decoded"; tail -3 "$W/$name.txt"; return 1; }
  PSNR=$("$PY" "$HERE/imgtool.py" psnr "$CARD" "$png")
}

check() {   # name picture
  local min
  min=$(awk -v b="$BASE" 'BEGIN{printf "%.2f", b-3}')
  roundtrip "$1" "$2"
  if awk -v p="$PSNR" -v m="$min" 'BEGIN{exit !(p>=m)}'; then
    echo "  ok    $1: luma PSNR $PSNR dB (pass mark $min)"
  else
    echo "  FAIL  $1: luma PSNR $PSNR dB (pass mark $min)"; fails=$((fails+1))
  fi
}

echo "== image formats this build reads"
formats=$(QSSTV_LOG_FORMATS=1 "$APP" --list-modes 2>&1 >/dev/null | sed -n 's/^image formats readable: //p')
for f in png jpg webp avif heic heif; do
  case " $formats " in *" $f "*) echo "  $f: yes";; *) echo "  $f: no";; esac
done
if [ "$(uname)" = Darwin ]; then
  for f in heic heif; do
    case " $formats " in *" $f "*) ;; *) echo "  FAIL  $f is not readable"; fails=$((fails+1));; esac
  done
fi

echo "== baseline: the card as PNG"
roundtrip card-png "$CARD" || { echo "  FAIL  the baseline round trip does not work"; exit 1; }
BASE=$PSNR
awk -v b="$BASE" 'BEGIN{exit !(b>=15)}' || { echo "  FAIL  baseline PSNR $BASE dB is too low to tell anything"; exit 1; }
echo "  luma PSNR $BASE dB"

if [ "$WHAT" = plain ] || [ "$WHAT" = all ]; then
  if [ "$(uname)" = Darwin ]; then
    echo "== upright HEIC (made by macOS)"
    sips -s format heic "$CARD" --out "$W/upright.heic" >/dev/null || { echo "  FAIL  sips cannot write HEIC"; fails=$((fails+1)); }
    [ -f "$W/upright.heic" ] && check upright-heic "$W/upright.heic"
  fi
fi

if [ "$WHAT" = orientation ] || [ "$WHAT" = all ]; then
  "$PY" "$HERE/imgtool.py" make "$CARD" "$W"
  echo "== sanity: Pillow turns the test JPEGs upright"
  for o in 6 8 3; do
    p=$("$PY" "$HERE/imgtool.py" upright "$W/orient$o.jpg" "$CARD")
    awk -v p="$p" 'BEGIN{exit !(p>=25)}' && echo "  ok    orientation $o: PSNR $p dB" || { echo "  FAIL  orientation $o: PSNR $p dB, the test pictures are wrong"; exit 1; }
  done
  echo "== JPEG with an EXIF orientation tag"
  for o in 6 8 3; do check "jpeg-orient$o" "$W/orient$o.jpg"; done
  if [ "$(uname)" = Darwin ]; then
    echo "== HEIC with an orientation tag (written by ImageIO)"
    swiftc -O -o "$W/make_heic" "$HERE/make_heic.swift" || { echo "  FAIL  cannot build make_heic"; exit 1; }
    for o in 6 8 3; do
      if "$W/make_heic" "$W/stored$o.png" "$W/orient$o.heic" $o; then check "heic-orient$o" "$W/orient$o.heic"
      else echo "  FAIL  heic-orient$o: could not make the test picture"; fails=$((fails+1)); fi
    done
  fi
fi

echo
[ "$fails" -eq 0 ] && echo "all checks passed" || echo "$fails check(s) failed"
[ "$fails" -eq 0 ]
