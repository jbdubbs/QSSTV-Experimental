#!/bin/bash
# How weak a signal the receiver can start a picture from (HF sensitivity, compared with MMSSTV on 2026-10-28).
#
# On 2026-10-28 MMSSTV picked up about ten very poor HF pictures where this receiver got one. Measured on synthetic noise (the
# same pictures through MMSSTV's real demodulator and through this receiver): the VIS code was decoded down to about -3 dB SNR
# by MMSSTV and only above about +17 dB here, and a picture whose sync pulses are found on one line in five (-4 dB) was cut into
# fragments and dropped. This test keeps those two numbers from getting worse and records how much better they get.
#
# Every case transmits a picture through the real transmitter (qsstv-experimental --encode), adds white noise limited to
# 300-2700 Hz at a set SNR in 2.4 kHz (hfnoise.py noise), and decodes it headless with the real receiver (--batch, with the
# receiver trace). SNR is the signal power over the noise power in 2.4 kHz, the way HF reports are given.
#   1. VIS: M2 and S1, 4 noise seeds each, only the first 12 s (5 s of noise, then the header): how many of the 8 runs decode
#      the VIS code of the right mode at each SNR.
#   2. Pictures: S2, M2 and R36 at 3 seeds, the whole picture, 5 s of noise in front: how many of the 9 runs end up with at
#      least half of the picture's lines in one received picture (the "Save if Complete" 50% test), and how many of them are
#      still recognizable (hfnoise.py corr: at least 30% of the detailed rows correlate with the transmitted picture). The
#      receiver is set to --min-complete 0 so the fragments can be counted.
#   3. Frequency offset: M2 and S2, 3 seeds each, at 0 dB with the whole signal 75, 50 Hz below and 50, 75 Hz above its frequency:
#      how many of the 6 runs decode the VIS code (a transmitter or a dial that is not on frequency; MMSSTV: the same, ~+-50 Hz).
#   4. With RXTRACE_WAV=<recording from tests/rxtrace/capture.sh>: band noise must not contain a VIS code.
# The pass marks are the lowest figures worth keeping: they are the measured baseline of 2026-10-28 (commit 8571de1, Sensitivity
# High) and are raised as the receiver improves. Baseline: VIS decoded of 8 at 24/20/16/12 dB: 8/7/3/0, none below; pictures
# with half their lines of 9 at 3/0/-2/-4/-6 dB: 8/6/3/0/0, recognizable 8/6/2/0/0, none started by the VIS code.
# After stages 2 and 3 (2026-10-29: the wide VIS code is read from the strength of its tones, three resonator banks 45 Hz apart; a
# picture whose VIS code was read starts at once and runs by the clock): VIS decoded of 8 at 24 ... -3 dB: all 8, 7 at -4, 6 at -5;
# pictures with half their lines of 9 at 3/0/-2/-4/-6 dB: 9/9/9/9/0, recognizable 9/9/9/9/0, all started by the VIS code to -4 dB.
# After stage 1 of the sensitivity work (2026-10-29; new ratios and line tolerance, false syncs counted by width, chain re-anchoring):
# pictures with half their lines 9/8/8/4/0, recognizable 9/7/7/1/0; the hour of band noise at High and Save if Complete 25: 16 pictures
# started, none kept, receiver busy 66 s of 3600 (before: 15, 0, 154 s). The MMSSTV figures (not part of the repo, they come from its demodulator built standalone,
# README in ~/projects/qsstv-captures/2026-10-28-mmsstv-sensitivity) are printed in the table for comparison.
#
#   tests/sensitivity/run.sh                       # uses ../../build-cmake/qsstv-experimental, Sensitivity High
#   QSSTV_BIN=/path/to/qsstv-experimental tests/sensitivity/run.sh
#   SENS=1 tests/sensitivity/run.sh                # Sensitivity 0 Low, 1 Normal, 2 High (default), 3 DX
#   SENS_PART=vis|pics|shift tests/sensitivity/run.sh    # only part 1, 2 or 3
#   SENS_VIS_SNRS="-3 -4 -5 -6" SENS_PART=vis tests/sensitivity/run.sh   # other SNRs for part 1 (pass marks only for the default ones)
#   RXTRACE_WAV=~/projects/qsstv-captures/2026-10-27-hf/hour1/hf.wav tests/sensitivity/run.sh
#
# Needs: the built qsstv-experimental, ffmpeg (scales the test picture), python3 with numpy and Pillow. About 5 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
HFN=$HERE/hfnoise.py
SENS=${SENS:-2}
PART=${SENS_PART:-all}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to scale the test picture"; exit 2; }
REAL_HOME=$HOME
py() { HOME=$REAL_HOME python3 "$HFN" "$@"; }     # the app below runs with a throw-away HOME; python keeps yours
python3 -c "import numpy, PIL" 2>/dev/null || { echo "python3 with numpy and Pillow is needed"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME/.config/qsstv-experimental"
printf '[RX]\nsensitivity=%s\nminCompletion=0\nautoSave=true\nautoSlantAdjust=true\nsstvModeIndexRx=0\n' "$SENS" > "$HOME/.config/qsstv-experimental/qsstv-experimental.conf"
TMO=$(command -v timeout || command -v gtimeout)
dec() { mkdir -p "$2"; QSSTV_RX_TRACE="$2/trace.tsv" ${TMO:+$TMO 600} "$QSSTV" --batch --min-complete 0 -o "$2" "$1" 2>"$T/stderr" >/dev/null; }

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

declare -A SIZE=([M2]=320x256 [S1]=320x256 [S2]=320x256 [R36]=320x240)
LEAD=5
prepare() {      # encode the picture once per mode
  local m=$1 size=${SIZE[$1]}
  [ -f "$T/$m.wav" ] && return 0
  ffmpeg -loglevel error -y -i "$HERE/../jb60_loopback/images/card.png" -vf "scale=${size/x/:}:flags=lanczos" "$T/$m.png" || return 1
  "$QSSTV" --encode "$T/$m.png" --mode $m --wav-out "$T/$m.wav" >/dev/null 2>&1 || return 1
}

# pass marks: SNR (dB) -> least number of runs
declare -A MINVIS=([24]=8 [20]=8 [16]=8 [12]=8 [8]=8 [4]=8 [0]=8 [-3]=8 [-5]=4)
declare -A MINHALF=([3]=9 [0]=9 [-2]=9 [-4]=8 [-6]=0)
declare -A MINSHIFT=([-75]=5 [-50]=5 [50]=5 [75]=2)

if [ "$PART" = all ] || [ "$PART" = vis ]; then
  echo "== VIS code decoded (M2 and S1, 4 seeds each; MMSSTV: all 8 down to -3 dB)"
  for m in M2 S1; do prepare $m || { bad "could not prepare $m"; }; done
  VISSNRS=${SENS_VIS_SNRS:-24 20 16 12 8 4 0 -3 -5}
  printf '  %-8s' "SNR dB"; for snr in $VISSNRS; do printf '%5s' $snr; done; echo
  declare -A VISCOUNT
  for snr in $VISSNRS; do
    n=0
    for m in M2 S1; do
      for seed in 1 2 3 4; do
        py noise "$T/$m.wav" "$T/v.wav" --snr $snr --seed $seed --lead $LEAD --length 12
        ov="$T/ov_${snr}_${m}_${seed}"          # a fresh directory per run: the trace file is appended to
        dec "$T/v.wav" "$ov"
        grep -a -q -P "\tVIS\t[^\n]*proc=W\tmode=$m\taccepted=1" "$ov/trace.tsv" && n=$((n+1))
      done
    done
    VISCOUNT[$snr]=$n
  done
  printf '  %-8s' "decoded"; for snr in $VISSNRS; do printf '%5s' "${VISCOUNT[$snr]}/8"; done; echo
  f0=$fail
  for snr in $VISSNRS; do
    if ge "${VISCOUNT[$snr]}" "${MINVIS[$snr]:-0}"; then :; else bad "VIS at $snr dB: ${VISCOUNT[$snr]}/8 decoded, want at least ${MINVIS[$snr]}"; fi
  done
  [ $fail = $f0 ] && ok "VIS decode rates at or above the pass marks"
fi

if [ "$PART" = all ] || [ "$PART" = pics ]; then
  echo "== pictures with at least half of their lines (S2, M2, R36, 3 seeds each; MMSSTV: 9/9 at -2 dB, 8-9/9 at -4, 6/9 at -6)"
  for m in S2 M2 R36; do prepare $m || { bad "could not prepare $m"; }; done
  printf '  %-12s' "SNR dB"; for snr in 3 0 -2 -4 -6; do printf '%6s' $snr; done; echo
  declare -A HALF REC VIA
  for snr in 3 0 -2 -4 -6; do
    half=0; rec=0; vis=0
    for m in S2 M2 R36; do
      for seed in 1 2 3; do
        py noise "$T/$m.wav" "$T/p.wav" --snr $snr --seed $seed --lead $LEAD
        op="$T/op_${snr}_${m}_${seed}"          # a fresh directory per run: the pictures are counted
        dec "$T/p.wav" "$op"
        tr="$op/trace.tsv"
        # the saved pictures of this mode, in order of their SAVE lines; the one with the most lines counts
        best=0; bestn=0; i=0
        while IFS=$'\t' read -r _ _ smode _ _ slines _ spct _; do
          i=$((i+1))
          [ "${smode#mode=}" = "$m" ] || continue
          pct=${spct#percent=}
          if awk -v a="$pct" -v b="$best" 'BEGIN{exit !(a>b)}'; then best=$pct; bestn=$i; fi
        done < <(grep -a -P '\tSAVE\t' "$tr" | cut -f2-)
        if ge "$best" 50; then half=$((half+1)); fi
        if [ "$bestn" -gt 0 ]; then
          f=$(ls "$op"/*.png 2>/dev/null | sed -n "${bestn}p")
          if [ -n "$f" ]; then
            r=$(py corr "$T/$m.png" "$f" | sed -n 's/^share \([0-9.]*\).*/\1/p')
            ge "${r:-0}" 0.30 && rec=$((rec+1))
          fi
        fi
        grep -a -q -P '\tDETECT\t[^\n]*via=vis' "$tr" && vis=$((vis+1))
      done
    done
    HALF[$snr]=$half; REC[$snr]=$rec; VIA[$snr]=$vis
  done
  printf '  %-12s' ">=50% lines"; for snr in 3 0 -2 -4 -6; do printf '%6s' "${HALF[$snr]}/9"; done; echo
  printf '  %-12s' "recognizable"; for snr in 3 0 -2 -4 -6; do printf '%6s' "${REC[$snr]}/9"; done; echo
  printf '  %-12s' "started by VIS"; for snr in 3 0 -2 -4 -6; do printf '%6s' "${VIA[$snr]}/9"; done; echo
  f0=$fail
  for snr in 3 0 -2 -4 -6; do
    if ge "${HALF[$snr]}" "${MINHALF[$snr]}"; then :; else bad "pictures at $snr dB: ${HALF[$snr]}/9 with half their lines, want at least ${MINHALF[$snr]}"; fi
  done
  [ $fail = $f0 ] && ok "picture rates at or above the pass marks"
fi

if [ "$PART" = all ] || [ "$PART" = shift ]; then
  echo "== VIS code decoded with the signal off frequency (M2 and S2, 3 seeds each, 0 dB)"
  for m in M2 S2; do prepare $m || { bad "could not prepare $m"; }; done
  printf '  %-8s' "shift Hz"; for sh in -75 -50 50 75; do printf '%5s' $sh; done; echo
  declare -A SHIFTCOUNT
  for sh in -75 -50 50 75; do
    n=0
    for m in M2 S2; do
      for seed in 1 2 3; do
        py noise "$T/$m.wav" "$T/v.wav" --snr 0 --seed $seed --lead $LEAD --length 12 --shift $sh
        ov="$T/os_${sh}_${m}_${seed}"
        dec "$T/v.wav" "$ov"
        grep -a -q -P "\tVIS\t[^\n]*proc=W\tmode=$m\taccepted=1" "$ov/trace.tsv" && n=$((n+1))
      done
    done
    SHIFTCOUNT[$sh]=$n
  done
  printf '  %-8s' "decoded"; for sh in -75 -50 50 75; do printf '%5s' "${SHIFTCOUNT[$sh]}/6"; done; echo
  f0=$fail
  for sh in -75 -50 50 75; do
    if ge "${SHIFTCOUNT[$sh]}" "${MINSHIFT[$sh]}"; then :; else bad "VIS at $sh Hz: ${SHIFTCOUNT[$sh]}/6 decoded, want at least ${MINSHIFT[$sh]}"; fi
  done
  [ $fail = $f0 ] && ok "VIS decode rates off frequency at or above the pass marks"
fi

if [ -n "$RXTRACE_WAV" ]; then
  echo "== recording $RXTRACE_WAV"
  dec "$RXTRACE_WAV" "$T/r"
  n=$(grep -a -c -P '\tVIS\t' "$T/r/trace.tsv")
  [ "$n" = 0 ] && ok "no VIS code in the band noise" || bad "$n VIS codes in the band noise"
fi

echo; echo "$pass passed, $fail failed"
[ $fail = 0 ]
