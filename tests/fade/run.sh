#!/bin/bash
# A picture that fades into the noise and comes back must stay ONE picture (issue #129).
#
# Each case transmits a picture through the real transmitter (qsstv-experimental --encode), sinks the signal into noise
# for a while (wavtool.py fade), and decodes it headless with the real receiver (--batch). Checks: exactly one picture
# comes out, and the rows before and after the fade match the same picture decoded without the fade. A last case
# stops a transmission for good and sends a second picture, which must still be received (the wait for a fade must
# not swallow a new picture).
#
#   tests/fade/run.sh                              # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/fade/run.sh
#   FADE_CASES="R36:6" tests/fade/run.sh           # only these MODE:hold-seconds cases
#
# Robot 36 note: a Robot 36 picture that goes through noise can come out with everything below it moved up by whole line pairs
# (2, 4, 6 ... rows), with a fade of a second or two as well, so it is not caused by the wait for the signal. wavtool.py rows
# therefore fits each end of the picture on its own (6 rows, 8 px) and prints the move; the other modes never move.
#
# Needs: the built qsstv-experimental, ffmpeg (scales the test picture), python3 with numpy and Pillow.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to scale the test picture"; exit 2; }
REAL_HOME=$HOME
py() { HOME=$REAL_HOME python3 "$HERE/wavtool.py" "$@"; }     # the app below runs with a throw-away HOME; python keeps yours
python3 -c "import numpy, PIL" 2>/dev/null || { echo "python3 with numpy and Pillow is needed"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"
TMO=$(command -v timeout || command -v gtimeout)
run() { ${TMO:+$TMO 600} "$QSSTV" --batch "$@" 2>"$T/stderr" >/dev/null; }   # batch decode; pictures are written to the -o directory

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
npng() { ls "$1"/*.png 2>/dev/null | wc -l | tr -d ' '; }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

declare -A SIZE=([R36]=320x240 [M1]=320x256 [S1]=320x256 [SC2_180]=320x256 [PD120]=640x496)
prepare() {      # encode the picture once per mode, decode it clean
  local m=$1 size=${SIZE[$1]}
  [ -f "$T/$m.wav" ] && return 0
  ffmpeg -loglevel error -y -i "$HERE/../jb60_loopback/images/card.png" -vf "scale=${size/x/:}:flags=lanczos" "$T/$m.png" || return 1
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || return 1
  mkdir -p "$T/clean_$m"; run -o "$T/clean_$m" "$T/$m.wav"
  [ "$(npng "$T/clean_$m")" = 1 ] || { echo "  FAIL $m: the unfaded recording does not decode to one picture"; return 1; }
}

echo "== fades (mode:seconds at the bottom; 1.5 s ramps each way, noise 25 dB below the signal)"
for spec in ${FADE_CASES:-R36:3 R36:6 R36:12 M1:10 M1:15 S1:15 PD120:10 PD120:15}; do
  m=${spec%%:*}; hold=${spec##*:}
  prepare $m || { bad "$spec: could not prepare"; continue; }
  py fade "$T/$m.wav" "$T/f_$spec.wav" --start-frac 0.30 --hold $hold
  mkdir -p "$T/o_$spec"; run -o "$T/o_$spec" "$T/f_$spec.wav"
  n=$(npng "$T/o_$spec")
  if [ "$n" != 1 ]; then bad "$spec: $n pictures, want 1"; continue; fi
  r=$(py rows "$T/clean_$m"/*.png "$T/o_$spec"/*.png 0.2 0.2)
  top=$(echo "$r" | sed 's/.*top \([0-9.]*\).*/\1/'); bot=$(echo "$r" | sed 's/.*bottom \([0-9.]*\).*/\1/')
  if ge "$top" 0.9 && ge "$bot" 0.9; then ok "$spec: one picture ($r)"; else bad "$spec: one picture but rows differ ($r)"; fi
done

echo "== a fade that outlasts the picture: one picture, the part before the fade is there, no hang"
for spec in ${FADE_LONG:-R36:30 PD120:130}; do
  m=${spec%%:*}; hold=${spec##*:}
  prepare $m || { bad "$spec: could not prepare"; continue; }
  py fade "$T/$m.wav" "$T/fl_$spec.wav" --start-frac 0.3 --hold $hold
  mkdir -p "$T/ol_$spec"; run -o "$T/ol_$spec" "$T/fl_$spec.wav"; rc=$?
  n=$(npng "$T/ol_$spec")
  if [ "$n" != 1 ]; then bad "$spec: $n pictures, want 1 (exit $rc)"; continue; fi
  r=$(py rows "$T/clean_$m"/*.png "$T/ol_$spec"/*.png 0.2 0)
  top=$(echo "$r" | sed 's/.*top \([0-9.]*\).*/\1/')
  ge "$top" 0.9 && ok "$spec: one picture, saved ($r)" || bad "$spec: one picture but its top differs ($r)"
done

echo "== a transmission that ends for good, then a new picture"
for spec in ${FADE_JOIN:-R36:0.5 PD120:0.5}; do
  m=${spec%%:*}; frac=${spec##*:}
  prepare $m || { bad "$spec: could not prepare"; continue; }
  py join "$T/j_$spec.wav" "$T/$m.wav" $frac 5 "$T/$m.wav"
  mkdir -p "$T/oj_$spec"; run -o "$T/oj_$spec" "$T/j_$spec.wav"
  n=$(npng "$T/oj_$spec")
  last=$(ls "$T"/oj_$spec/*.png 2>/dev/null | sort -V | tail -1)
  if [ "$n" -ge 2 ] && [ -n "$last" ]; then
    r=$(py rows "$T/clean_$m"/*.png "$last" 1 1)
    top=$(echo "$r" | sed 's/.*top \([0-9.]*\).*/\1/')
    # 0.8, not 0.9: a picture that follows a lost sync is received slightly slanted (~0.89 of the rows match for PD120 even
    # before the fix for #129), but one that lost its first quarter to the wait for the faded one would score 0.75
    ge "$top" 0.8 && ok "$spec: $n pictures, the second is complete ($r)" || bad "$spec: second picture damaged ($r)"
  else bad "$spec: $n pictures, want the cut one and the new one"; fi
done

echo; echo "$pass passed, $fail failed"
[ $fail = 0 ]
