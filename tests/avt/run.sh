#!/bin/bash
# The AVT modes: our transmitter and receiver agree, the receiver reads the header the way MMSSTV sends it, and the VIS code announces them in Auto.
#
# QSSTV's AVT transmitter sent no digital header and a Martin-like line with gaps and a sync pulse, which no receiver (not even QSSTV's own) could
# read; the receiver gave up on the 300 ms leader of the header (a tone longer than a pulse aborted the picture), and in Auto it did not accept the
# VIS code of an AVT mode at all. Checks, with the real program:
#   - each AVT mode sent by --encode is received with --batch, with the mode forced and in Auto, at the size it was sent, its steps within 0.3 ms
#     of where they were sent
#   - a transmission made the way MMSSTV makes it (avtgen.py: leader and VIS code three times, 32 words, lines of R, G, B without gaps) is
#     received, whatever word of the header the receiver hears first (none lost, 10, 20 or 31 lost; with all 32 lost the picture starts where
#     the header ends, as a picture that a VIS code started runs to its end in every mode), also in Auto and after 30 s of noise
#   - a VIS code that no header follows: the next VIS code ends that picture, a Martin 1 that comes after it is received
#
#   tests/avt/run.sh                            # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/avt/run.sh
# Needs python3 with numpy and Pillow.
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
within() { awk -v a="$1" -v l="$2" 'BEGIN{exit !(a<=l && a>=-l)}'; }
# decode wav mode-option dir; the picture it writes is printed
# (every decode starts with settings of its own: a decode that forces a mode must not leave it behind for the next)
decode() { local h; h=$(mktemp -d -p "$T"); rm -rf "$3"; mkdir -p "$3"
  HOME=$h XDG_CONFIG_HOME=$h/.config XDG_DATA_HOME=$h/.local/share "$QSSTV" --batch $2 -o "$3" "$1" >/dev/null 2>&1; ls "$3"/*.png 2>/dev/null | head -1; }

echo "== our transmitter into our receiver"
for spec in AVT90:320:240:0.3906 AVT24:128:120:0.4883 AVT94:320:200:0.3906; do
  IFS=: read m w h px <<<"$spec"
  py -c "
from PIL import Image
w,h=$w,$h
im=Image.new('RGB',(w,h)); p=im.load()
for x in range(w):
  c=(255 if x>=w//4 else 0, 255 if x>=w//2 else 0, 255 if x>=3*w//4 else 0)
  for y in range(h): p[x,y]=c
im.save('$T/$m.png')" || exit 2
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || { bad "$m: no transmission"; continue; }
  for how in "--mode $m" ""; do
    label=${how:+forced}; label=${label:-Auto}
    png=$(decode "$T/$m.wav" "$how" "$T/o_$m")
    if [ -z "$png" ]; then bad "$m ($label): no picture received"; continue; fi
    read -r W H pxo ms < <(py "$HERE/edges.py" "$png" $px)
    if [ "$W" = $w ] && [ "$H" = $h ] && within "$ms" 0.3; then ok "$m ($label): $W x $H, steps $ms ms from where they were sent"; else bad "$m ($label): $W x $H, steps $ms ms ($pxo px) from where they were sent"; fi
  done
done

echo "== a transmission made the way MMSSTV makes it (AVT 90)"
for spec in "0 3 0 forced" "10 3 0 forced" "20 3 0 forced" "31 3 0 forced" "32 3 0 forced" "32 1 0 forced" "0 3 0 Auto" "10 3 0 Auto" "32 3 0 Auto" "0 3 30 Auto"; do
  read -r drop rep lead how <<<"$spec"
  py "$HERE/avtgen.py" "$T/spec.wav" $drop $rep $lead || exit 2
  opt=""; [ $how = forced ] && opt="--mode AVT90"
  png=$(decode "$T/spec.wav" "$opt" "$T/s")
  what="$drop words of the header lost, VIS code $rep times, $lead s of noise ($how)"
  if [ -z "$png" ]; then bad "$what: no picture received"; continue; fi
  read -r W H pxo ms < <(py "$HERE/edges.py" "$png" 0.3906)
  if [ "$H" = 240 ] && within "$ms" 0.3; then ok "$what: $W x $H, steps $ms ms from where they were sent"; else bad "$what: $W x $H, steps $ms ms from where they were sent"; fi
  rm -f "$T/spec.wav"
done

echo "== a VIS code that no header follows"
py -c "
from PIL import Image
w,h=320,256
im=Image.new('RGB',(w,h)); p=im.load()
for x in range(w):
  c=(255 if x>=w//4 else 0, 255 if x>=w//2 else 0, 255 if x>=3*w//4 else 0)
  for y in range(h): p[x,y]=c
im.save('$T/M1.png')" || exit 2
"$QSSTV" --encode "$T/M1.png" --mode M1 --wav-out "$T/M1.wav" >/dev/null 2>&1 || exit 2
py "$HERE/avtgen.py" "$T/vis.wav" 0 1 0 visonly || exit 2
py -c "
import wave, numpy as np
import struct
def rd(f):
    raw=open(f,'rb').read(); n,fs=struct.unpack('<HI',raw[22:28])      # (the length in the header of --encode's file is not reliable: take what is there)
    return np.frombuffer(raw[44:44+(len(raw)-44)//(2*n)*2*n],dtype=np.int16).reshape(-1,n)[:,0], fs
a,fs=rd('$T/vis.wav'); b,fs2=rd('$T/M1.wav'); assert fs==fs2
rng=np.random.default_rng(2); gap=(0.02*32000*rng.standard_normal(int(3*fs))).astype(np.int16)
y=np.concatenate([a,gap,b]); w=wave.open('$T/mixed.wav','wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(fs); w.writeframes(y.tobytes()); w.close()"
png=$(decode "$T/mixed.wav" "" "$T/m1")
if [ -n "$png" ] && [ "$(basename "$png" | grep -c M1)" = 1 ]; then
  read -r W H pxo ms < <(py "$HERE/edges.py" "$png" 0.4576)
  if [ "$H" = 256 ] && within "$ms" 0.6; then ok "the Martin 1 after it is received: $W x $H, steps $ms ms from where they were sent"; else bad "the Martin 1 after it: $W x $H, steps $ms ms from where they were sent"; fi
else bad "the Martin 1 after a VIS code of an AVT mode without a header is not received"; fi
echo "$fail failed"
[ $fail = 0 ]
