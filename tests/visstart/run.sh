#!/bin/bash
# A picture whose VIS code was received starts where the VIS code says, even if its first lines are in the noise.
#
# The receiver used to start a picture at the first pulse of a chain of sync pulses, and the VIS code only gave the mode. With
# the first lines in noise the picture started late, and everything below moved up by as many rows as there were lost lines
# (MMSSTV starts at the end of the VIS code and shows those lines as noise). It now starts the picture at the VIS code (DETECT
# via=vis-start, 2026-10-29) and runs it by the clock; a chain of pulses that forms later refines its position. Each case transmits a picture through the real
# transmitter (qsstv-experimental --encode), sinks the signal into noise right after the VIS code for N lines (wavtool.py
# fade), and decodes it headless with the real receiver (--batch). Checks:
#   - N lines in the noise: one picture, started via the VIS code, and the rows below the noise are where they are in the same
#     picture decoded without the noise (no vertical move at all)
#   - a dip in the 1900 Hz leader of the VIS code (50 and 80 ms of the 300 ms) does not hide the code
#   - a VIS code that no picture follows: the picture it started runs on, and the VIS code of a picture of another mode that
#     comes 40 s later ends it and is received whole (before, the receiver stayed held to the first mode and lost most of it)
#   - the same in noise (0 dB SNR) with the first lines of the second picture lost: the code of the second picture is accepted
#     while the first picture runs (it was checked against the mode of the first picture and thrown away, 2026-10-29: a real
#     Martin 1 that came after a Scottie 2 that was never sent was lost) and the picture starts at its code, not at its pulses,
#     in the right place (the code was taken to be 0.68 s earlier than it was, by the amount the receiver takes its buffers back)
#   - control: a VIS code destroyed by the noise gives a picture found by its sync pulses only, not moved back
# With RXTRACE_WAV=<recording from tests/rxtrace/capture.sh> a real recording of band noise is checked as well: it must not
# contain a single VIS code.
#
#   tests/visstart/run.sh                          # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/visstart/run.sh
#   VISSTART_CASES="M1:8" tests/visstart/run.sh    # only these MODE:lines-in-noise cases
#
# The test transmissions start with a calibration header, so the first line starts 1.715 s into the file (1.955 s with an
# extended VIS code: MP, MR and ML modes; 2.365 s with a narrow VIS code: the -N modes, MC110-N here).
#
# Needs: the built qsstv-experimental, ffmpeg (scales the test picture), python3 with numpy and Pillow. About 5 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
WAVTOOL=$HERE/../fade/wavtool.py
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to scale the test picture"; exit 2; }
REAL_HOME=$HOME
py() { HOME=$REAL_HOME python3 "$WAVTOOL" "$@"; }     # the app below runs with a throw-away HOME; python keeps yours
hfn() { HOME=$REAL_HOME python3 "$HERE/../sensitivity/hfnoise.py" "$@"; }
python3 -c "import numpy, PIL" 2>/dev/null || { echo "python3 with numpy and Pillow is needed"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"
TMO=$(command -v timeout || command -v gtimeout)
# dec FILE DIR: decode FILE into DIR, with a trace in DIR/trace.tsv
dec() { mkdir -p "$2"; QSSTV_RX_TRACE="$2/trace.tsv" ${TMO:+$TMO 600} "$QSSTV" --batch -o "$2" "$1" 2>"$T/stderr" >/dev/null; }
npng() { ls "$1"/*.png 2>/dev/null | wc -l | tr -d ' '; }
# fld TRACE EVENT KEY: the value of KEY in the first EVENT line of the trace
fld() { grep -a -P "\t$2\t" "$1" | head -1 | tr '\t' '\n' | sed -n "s/^$3=//p" | head -1; }
count() { grep -a -c -P "\t$2\t" "$1"; }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }

declare -A SIZE=([M1]=320x256 [R36]=320x240 [PD120]=640x496 [S1]=320x256 [MP73]=320x256 [MC110-N]=320x256)
declare -A LINEMS=([M1]=446.446 [R36]=150 [PD120]=508.48 [S1]=428.38 [MP73]=570 [MC110-N]=428.53)
declare -A START=([M1]=1.715 [R36]=1.715 [PD120]=1.715 [S1]=1.715 [MP73]=1.955 [MC110-N]=2.365)
prepare() {      # encode the picture once per mode, decode it clean
  local m=$1 size=${SIZE[$1]}
  [ -f "$T/$m.wav" ] && return 0
  ffmpeg -loglevel error -y -i "$HERE/../jb60_loopback/images/card.png" -vf "scale=${size/x/:}:flags=lanczos" "$T/$m.png" || return 1
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || return 1
  dec "$T/$m.wav" "$T/clean_$m"
  [ "$(npng "$T/clean_$m")" = 1 ] || { echo "  FAIL $m: the unfaded recording does not decode to one picture"; return 1; }
}

echo "== lines of a picture in the noise right after its VIS code"
for spec in ${VISSTART_CASES:-M1:8 M1:16 R36:30 PD120:10 S1:12 MP73:10 MC110-N:8}; do
  IFS=: read -r m n <<< "$spec"
  prepare $m || { bad "$spec: could not prepare"; continue; }
  hold=$(python3 -c "print(round($n*${LINEMS[$m]}/1000-0.1,3))")
  py fade "$T/$m.wav" "$T/n_$spec.wav" --start ${START[$m]} --ramp 0.05 --hold $hold --floor-db 60 --snr 25 --seed 3
  dec "$T/n_$spec.wav" "$T/o_$spec"
  tr="$T/o_$spec/trace.tsv"
  if [ "$(npng "$T/o_$spec")" != 1 ]; then bad "$spec: $(npng "$T/o_$spec") pictures, want 1"; continue; fi
  via=$(fld "$tr" DETECT via); bf=$(fld "$tr" DETECT backfill_lines)
  r=$(py rows "$T/clean_$m"/*.png "$T/o_$spec"/*.png 0 0.5 --px 6)
  bot=$(echo "$r" | sed 's/.*bottom \([0-9.]*\).*/\1/')
  if [ "$via" = vis-start ] && ge "$bot" 0.9; then ok "$spec: started at the VIS code, the $n lines in the noise are there ($r)"
  elif [ "$m" = MC110-N ] && [ "$via" = vis ] && [ "$bf" = "$n" ] && ge "$bot" 0.9; then ok "$spec: narrow code, found by its pulses and moved back, $bf lines recovered ($r)"   # the narrow code is still read from the frequency (strong signals only)
  else bad "$spec: via=$via, backfill_lines=$bf, rows: $r"; fi
done

echo "== a dip in the 1900 Hz leader of the VIS code"
prepare M1 && for spec in 1.25:0.05 1.22:0.08; do
  IFS=: read -r st hold <<< "$spec"
  py fade "$T/M1.wav" "$T/d_$spec.wav" --start $st --ramp 0.002 --hold $hold --floor-db 60 --snr 25 --seed 3
  dec "$T/d_$spec.wav" "$T/od_$spec"
  tr="$T/od_$spec/trace.tsv"
  if [ "$(npng "$T/od_$spec")" = 1 ] && [ "$(fld "$tr" DETECT via)" = vis-start ]; then ok "${hold}s dip at ${st}s: the VIS code is received"
  else bad "${hold}s dip at ${st}s: $(npng "$T/od_$spec") pictures, via=$(fld "$tr" DETECT via) (want vis-start)"; fi
done

echo "== a VIS code that no picture follows: the next VIS code ends the picture it started"
prepare M1 && prepare S1 && {
  py join "$T/ex.wav" "$T/M1.wav" 0.0165 40 "$T/S1.wav"      # the VIS code and the first line of a Martin 1, 40 s of noise, a Scottie 1
  dec "$T/ex.wav" "$T/oe"
  tr="$T/oe/trace.tsv"
  lines=$(grep -a -P '\tSAVE\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^lines=//p')
  last=$(grep -a -P '\tSAVE\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^mode=//p')
  [ "$(fld "$tr" DETECT via)" = vis-start ] && ok "the Martin 1 was started by its VIS code" || bad "the Martin 1 was not started by its VIS code"
  if [ "$last" = S1 ] && [ "$lines" = 256 ]; then ok "the Scottie 1 that follows is received whole ($lines lines)"
  else bad "the Scottie 1 that follows: last picture $last, $lines lines (want S1, 256 lines)"; fi
}

echo "== the same in noise: the code of the next picture is accepted while the first picture runs"
prepare M1 && prepare S1 && {
  py fade "$T/S1.wav" "$T/S1f.wav" --start 1.715 --ramp 0.002 --hold 3.5 --floor-db 60 --snr 25 --seed 3      # the first lines of the Scottie 1 are in the noise
  py join "$T/ex2.wav" "$T/M1.wav" 0.0165 40 "$T/S1f.wav"
  hfn noise "$T/ex2.wav" "$T/ex2n.wav" --snr 0 --seed 1 --lead 5
  dec "$T/ex2n.wav" "$T/on"
  tr="$T/on/trace.tsv"
  lines=$(grep -a -P '\tSAVE\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^lines=//p')
  last=$(grep -a -P '\tSAVE\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^mode=//p')
  vis=$(grep -a -P '\tVIS\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^accepted=//p')
  via=$(grep -a -P '\tDETECT\t' "$tr" | tail -1 | tr '\t' '\n' | sed -n 's/^via=//p')
  [ "$vis" = 1 ] && ok "the VIS code of the Scottie 1 is accepted while the Martin 1 runs" || bad "the VIS code of the Scottie 1 was thrown away (accepted=$vis)"
  refused=$(grep -a -c -P '\tADOPT\t.*accepted=0' "$tr")
  [ "$refused" = 0 ] && ok "the pulses of the Scottie 1 agree with the place its code puts it at" || bad "$refused chains of pulses of the Scottie 1 were refused: the picture started at the wrong place"
  if [ "$last" = S1 ] && [ "$lines" = 256 ] && [ "$via" = vis-start ]; then ok "the Scottie 1 is started by its code and received whole ($lines lines)"
  else bad "the Scottie 1 in noise: last picture $last, $lines lines, started via=$via (want S1, 256 lines, vis-start)"; fi
}

echo "== control: the VIS code destroyed"
prepare M1 && {
  py fade "$T/M1.wav" "$T/c1.wav" --start 0.7 --ramp 0.01 --hold 1.0 --floor-db 60 --snr 25 --seed 3
  py fade "$T/c1.wav" "$T/c2.wav" --start 1.715 --ramp 0.05 --hold 3.5 --floor-db 60 --snr 25 --seed 4
  dec "$T/c2.wav" "$T/oc"
  tr="$T/oc/trace.tsv"
  if [ "$(npng "$T/oc")" = 1 ] && [ "$(count "$tr" VIS)" = 0 ] && { [ "$(fld "$tr" DETECT via)" = sync ] || [ "$(fld "$tr" DETECT via)" = train ]; } && [ "$(fld "$tr" DETECT backfill_lines)" = 0 ]; then
    ok "no VIS code: found by its sync pulses (chain or train), not moved back"
  else bad "no VIS code: $(npng "$T/oc") pictures, VIS codes $(count "$tr" VIS), via=$(fld "$tr" DETECT via), backfill_lines=$(fld "$tr" DETECT backfill_lines)"; fi
}

if [ -n "$RXTRACE_WAV" ]; then
  echo "== recording $RXTRACE_WAV"
  dec "$RXTRACE_WAV" "$T/r"
  n=$(count "$T/r/trace.tsv" VIS)
  [ "$n" = 0 ] && ok "no VIS code in the band noise" || bad "$n VIS codes in the band noise"
fi

echo; echo "$pass passed, $fail failed"
[ $fail = 0 ]
