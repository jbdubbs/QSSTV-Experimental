#!/bin/bash
# Pictures that arrive without a readable VIS code: found by the rhythm of their sync pulses (syncTrainDetector, src/sstv/synctrain.cpp).
#
# The pulse detector of the sync processor keeps every pulse of 4 to 25 ms in a table, and in the band noise of HF there are 16 to 29 of
# them per line: a chain of four pulses a line apart is refused when 20 other entries lie between them. Recorded on 2026-10-29: of 31
# real weak pictures the receiver started 22, the pictures MMSSTV started with the sync pulses of the line time (a "train") it did not.
# The train detector looks at the strength of 1200 Hz against 1900 Hz after a hard limiter (noise adds about equally to both) and
# compares the distances between the events with the line time of each mode. This test keeps it working:
#   1. Pictures without a VIS code (S2, M2, R36 and R72, 3 noise seeds each, the VIS code faded out, 5 s of noise in front): how many of
#      the 12 runs end up with at least half of the picture's lines in a picture of the right mode, and how many were started by the train.
#      R72 is there because its train (a pulse every 300 ms) also fits Robot 36 with every other line missed.
#   2. The same at -4 dB with the detector switched off (QSSTV_TRAIN_LEVEL=0): the detector must add pictures.
#   3. Off frequency: M2 and S2 at -4 dB with the whole signal 50 Hz below and above its frequency.
#   4. With RXTRACE_WAV=<recording from tests/rxtrace/capture.sh>: an hour of band noise must not start more than two trains.
# Pass marks are below the figures measured on 2026-10-30 (Sensitivity High): pictures with half their lines of 12 at 0/-2/-4/-6 dB: 12/12/10/9
# (detector off at -4 dB: 6), off frequency -50/+50 Hz: 6/5 of 6, 0 trains in the hour of band noise. Figures of 28 runs (7 modes x 4 seeds, sync only), train
# off -> on: >= 50% of the lines at 0/-2/-4/-6/-8 dB: 27/26/13/0/0 -> 28/28/24/17/2; MMSSTV (sync trains only or with VIS): 28/28/26/20/9.
# The day of 2026-10-29 replayed (31 real pictures): started 22 -> 29, saved with >= 50%: 20 -> 28, >= 90%: 15 -> 23, noise pictures the same.
#
#   tests/synctrain/run.sh                         # uses ../../build-cmake/qsstv-experimental, Sensitivity High
#   QSSTV_BIN=/path/to/qsstv-experimental tests/synctrain/run.sh
#   SENS=1 tests/synctrain/run.sh                  # Sensitivity 0 Low (no train detector), 1 Normal, 2 High (default), 3 DX
#   RXTRACE_WAV=~/projects/qsstv-captures/2026-10-27-hf/hour1/hf.wav tests/synctrain/run.sh
#
# Needs: the built qsstv-experimental, ffmpeg (scales the test picture), python3 with numpy and Pillow. About 4 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
HFN=$HERE/../sensitivity/hfnoise.py
WAVTOOL=$HERE/../fade/wavtool.py
SENS=${SENS:-2}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to scale the test picture"; exit 2; }
python3 -c "import numpy, PIL" 2>/dev/null || { echo "python3 with numpy and Pillow is needed"; exit 2; }
REAL_HOME=$HOME
hfn() { HOME=$REAL_HOME python3 "$HFN" "$@"; }          # the app below runs with a throw-away HOME; python keeps yours
wt() { HOME=$REAL_HOME python3 "$WAVTOOL" "$@"; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME/.config/qsstv-experimental"
printf '[RX]\nsensitivity=%s\nminCompletion=0\nautoSave=true\nautoSlantAdjust=true\nsstvModeIndexRx=0\n' "$SENS" > "$HOME/.config/qsstv-experimental/qsstv-experimental.conf"
TMO=$(command -v timeout || command -v gtimeout)
# dec FILE DIR [VAR=VALUE...]: decode FILE into DIR with a trace in DIR/trace.tsv (a fresh DIR per run: the trace is appended to)
dec() { local f=$1 d=$2; shift 2; mkdir -p "$d"; env "$@" QSSTV_RX_TRACE="$d/trace.tsv" ${TMO:+$TMO 600} "$QSSTV" --batch --min-complete 0 -o "$d" "$f" 2>"$T/stderr" >/dev/null; }

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

declare -A SIZE=([M2]=320x256 [S2]=320x256 [R36]=320x240 [R72]=320x240)
prepare() {      # encode the picture once per mode, and fade the VIS code (it ends 1.715 s into the file) and the calibration header out
  local m=$1 size=${SIZE[$1]}
  [ -f "$T/$m.wav" ] && return 0
  ffmpeg -loglevel error -y -i "$HERE/../jb60_loopback/images/card.png" -vf "scale=${size/x/:}:flags=lanczos" "$T/$m.png" || return 1
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || return 1
  wt fade "$T/$m.wav" "$T/${m}_novis.wav" --start 0.5 --ramp 0.01 --hold 1.185 --floor-db 60 --snr 60 --seed 3 >/dev/null || return 1
}

# case_run MODE SNR SEED SHIFT_HZ [VAR=VALUE...]: prints "percent via": the best picture of the right mode, and how the first picture was started
case_run() {
  local m=$1 snr=$2 seed=$3 shift_hz=$4; shift 4
  local d; d=$(mktemp -d "$T/r.XXXXXX")                 # a fresh directory per run
  hfn noise "$T/${m}_novis.wav" "$d/c.wav" --snr $snr --seed $seed --lead 5 --shift $shift_hz || return 1
  dec "$d/c.wav" "$d/o" "$@"
  local tr="$d/o/trace.tsv"
  local best via
  best=$(grep -a -P "\tSAVE\t[^\n]*mode=$m\t" "$tr" | sed -n 's/.*percent=\([0-9.]*\).*/\1/p' | sort -n | tail -1)
  via=$(grep -a -P "\tDETECT\t" "$tr" | head -1 | tr '\t' '\n' | sed -n 's/^via=//p' | head -1)
  rm -f "$d/c.wav"
  echo "${best:-0} ${via:--}"
}

MODES="S2 M2 R36 R72"
for m in $MODES; do prepare $m || { echo "could not prepare $m"; exit 2; }; done

# pass marks: SNR (dB) -> least number of the 12 runs with half the lines, and with a train start (Sensitivity High)
declare -A MINHALF=([0]=12 [-2]=12 [-4]=9 [-6]=7)
declare -A MINTRAIN=([0]=6 [-2]=6 [-4]=8 [-6]=4)

echo "== pictures without a VIS code (S2, M2, R36, R72, 3 seeds each; MMSSTV: 28/28 down to -3 dB, 25 of 28 at -4, 20 at -6)"
printf '  %-14s' "SNR dB"; for snr in 0 -2 -4 -6; do printf '%6s' $snr; done; echo
declare -A HALF TRAIN
for snr in 0 -2 -4 -6; do
  h=0; t=0
  for m in $MODES; do
    for seed in 1 2 3; do
      read -r pct via < <(case_run $m $snr $seed 0)
      ge "$pct" 50 && h=$((h+1))
      [ "$via" = train ] && t=$((t+1))
    done
  done
  HALF[$snr]=$h; TRAIN[$snr]=$t
done
printf '  %-14s' ">=50% lines"; for snr in 0 -2 -4 -6; do printf '%6s' "${HALF[$snr]}/12"; done; echo
printf '  %-14s' "started by train"; for snr in 0 -2 -4 -6; do printf '%6s' "${TRAIN[$snr]}"; done; echo
f0=$fail
if [ "$SENS" = 0 ]; then
  echo "  (Sensitivity Low: no train detector, pass marks not checked)"
else
  for snr in 0 -2 -4 -6; do
    ge "${HALF[$snr]}" "${MINHALF[$snr]}" || bad "$snr dB: ${HALF[$snr]}/12 pictures with half their lines, want at least ${MINHALF[$snr]}"
    ge "${TRAIN[$snr]}" "${MINTRAIN[$snr]}" || bad "$snr dB: ${TRAIN[$snr]} pictures started by the train, want at least ${MINTRAIN[$snr]}"
  done
  [ $fail = $f0 ] && ok "pictures without a VIS code at or above the pass marks"
fi

if [ "$SENS" != 0 ]; then
  echo "== the same at -4 dB with the detector switched off (QSSTV_TRAIN_LEVEL=0)"
  h=0
  for m in $MODES; do
    for seed in 1 2 3; do
      read -r pct via < <(case_run $m -4 $seed 0 QSSTV_TRAIN_LEVEL=0)
      ge "$pct" 50 && h=$((h+1))
    done
  done
  echo "  pictures with half their lines, detector off: $h/12 (on: ${HALF[-4]}/12)"
  if [ $((HALF[-4]-h)) -ge 3 ]; then ok "the detector adds at least 3 of 12 pictures at -4 dB"
  else bad "the detector adds only $((HALF[-4]-h)) of 12 pictures at -4 dB (on ${HALF[-4]}, off $h), want at least 3"; fi

  echo "== off frequency (M2 and S2, 3 seeds each, -4 dB, the whole signal 50 Hz below and above its frequency)"
  for sh in -50 50; do
    h=0
    for m in M2 S2; do
      for seed in 1 2 3; do
        read -r pct via < <(case_run $m -4 $seed $sh)
        ge "$pct" 50 && h=$((h+1))
      done
    done
    echo "  $sh Hz: $h/6 pictures with half their lines"
    ge "$h" 4 && ok "$sh Hz: at least 4 of 6" || bad "$sh Hz: $h/6 pictures with half their lines, want at least 4"
  done
fi

if [ -n "$RXTRACE_WAV" ]; then
  echo "== recording $RXTRACE_WAV (band noise)"
  dec "$RXTRACE_WAV" "$T/rn"
  n=$(grep -a -c -P '\tTRAIN\t' "$T/rn/trace.tsv")
  secs=$(python3 - "$RXTRACE_WAV" <<'EOF'
import sys, wave
w = wave.open(sys.argv[1])
print(int(w.getnframes() / w.getframerate()))
EOF
)
  echo "  $n trains in $secs s"
  ge 2 "$n" && ok "no more than two trains in an hour of band noise" || bad "$n trains in $secs s of band noise, want at most 2 an hour"
fi

echo; echo "$pass passed, $fail failed"
[ $fail = 0 ]
