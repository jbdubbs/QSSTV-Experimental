#!/bin/bash
# The first line of a picture starts with its sync pulse, right after the stop bit of the VIS code, as MMSSTV and the standard have it.
#
# QSSTV used to leave the sync pulse (and porch) of the first line out: the picture followed the 30 ms stop bit at once, and the receiver's
# constant from the end of the VIS code to the first line was measured on such signals. A picture that MMSSTV (or anything that follows the
# standard) sends, and whose pulses the receiver could not follow (a weak signal, no chain of three pulses), started 4.9 ms (Martin), 6 to 9.4
# (Robot, MP, MR, ML) or 20 ms (PD) early, a coloured bar on the left of the picture, 10 px on Martin (2026-11-19, issue #213). Two checks per mode:
#   - the 1200 Hz run after the data bits of the VIS code (stop bit + sync pulse) is as long as MMSSTV's (30 ms + the sync width)
#   - the receiver's pulses agree with where the VIS code puts the first line (pulses_vs_code_ms, the first ADOPT line of the receiver trace) to
#     0.5 ms, on QSSTV's own transmission and on the signal of MMSSTV's transmitter code (~/projects/mmsstv-core, skipped when it is not built)
#   - the start of a real Scottie 1 transmission of MMSSTV.EXE (mmsstv_scottie1_start.wav, recorded with MMSSTV's "record sound to a file" on 2026-11-20)
#     has the same stop bit + sync run, 38.5 ms. The code that the MMSSTV port is made from has no sync in front of Scottie's first line, the program has one.
#   - the narrow modes have the 1900 Hz sync pulse of the first line after the 24 bits of their VIS code, as the recorded MMSSTV.EXE MP110-N has
#     (mmsstv_mp110n_start.wav)
#   - the start of real B/W 8 and P3 transmissions of MMSSTV.EXE (mmsstv_bw8_start.wav, mmsstv_p3_start.wav) have 36 and 35.2 ms runs (the B/W and P modes
#     are not in the MMSSTV port)
# Modes MMSSTV has and that were measured: Martin 1/2, Robot 24/36/72, B/W 8 and 12, P3/P5 (and P7, the same), SC2-60 (and 120, 180), PD, MP, MR, ML, and Scottie 1 and 2 (recorded from MMSSTV.EXE; DX is the same).
# Modes that have not been measured keep the old start.
#
#   tests/leadingsync/run.sh                    # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental MMSSTV_ENCODER=/path/to/encode_wav_tool tests/leadingsync/run.sh
# Needs python3 with numpy and ImageMagick (magick).
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
ENC=${MMSSTV_ENCODER:-$HOME/projects/mmsstv-core/build/tests/encode_wav_tool}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v magick >/dev/null || { echo "ImageMagick (magick) is needed"; exit 2; }
REAL_HOME=$HOME
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"
fail=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
py() { HOME=$REAL_HOME python3 "$HERE/$1" "${@:2}"; }
# trace FILE.wav: pulses_vs_code_ms of the first ADOPT line ("-" if none)
pvc() { rm -rf "$T/o"; mkdir -p "$T/o"; QSSTV_RX_TRACE="$T/o/trace.tsv" timeout 600 "$QSSTV" --batch -o "$T/o" "$1" >/dev/null 2>&1
        grep -a -P '\tADOPT\t' "$T/o/trace.tsv" | head -1 | tr '\t' '\n' | sed -n 's/^pulses_vs_code_ms=//p' | head -1; }
near() { awk -v a="$1" -v b="$2" -v t="$3" 'BEGIN{d=a-b; exit !(a!="" && d<=t && d>=-t)}'; }
# mode:size:MMSSTV encoder name:expected stop-bit run (ms)
for spec in M1:320x256:martin1:34.9 M2:320x256:martin1:34.9 R24:160x120:robot24:36 R36:320x240:robot36:39 R72:320x240:robot72:39 \
            PD50:320x256:pd50:50 PD120:640x496:pd120:50 PD290:800x616:pd290:50 MP73:320x256:mp73:39 MP175:320x256:mp175:39 \
            MR73:320x256:mr73:39 MR175:320x256:mr175:39 ML180:640x496:ml180:39 ML320:640x496:ml320:39 S1:320x256:scottie1:39 S2:320x256:scottie2:39 BW8:160x120::36 BW12:160x120::36 P3:640x496::35.2 P5:640x496::37.8 P7:640x496::40.4 PD120W:768x432::50 SDX:320x256::39 SC2-60:320x256::35.5 SC2-120:320x256::35.5 SC2-180:320x256::35.5; do
  IFS=: read -r m size enc want <<<"$spec"
  magick -size "$size" xc:gray50 "$T/p.png"
  "$QSSTV" --encode "$T/p.png" --mode $m --wav-out "$T/q.wav" >/dev/null 2>&1 || { bad "$m: no transmission"; continue; }
  run=$(py stoprun.py "$T/q.wav")
  if near "$run" "$want" 1.2; then ok "$m: stop bit + sync $run ms (want $want)"; else bad "$m: stop bit + sync $run ms, want $want"; fi
  p=$(pvc "$T/q.wav")
  if near "$p" 0 0.5; then ok "$m: own transmission, pulses vs VIS code $p ms"; else bad "$m: own transmission, pulses vs VIS code ${p:--} ms (within 0.5)"; fi
  if [ -x "$ENC" ] && [ -n "$enc" ] && [ "$m" != S1 ] && [ "$m" != S2 ] && [ "$m" != M2 ] && [ "$m" != MP175 ] && [ "$m" != MR175 ] && [ "$m" != ML320 ] && [ "$m" != PD290 ]; then
    "$ENC" "$T/mm.wav" $enc >/dev/null 2>&1
    p=$(pvc "$T/mm.wav")
    if near "$p" 0 0.5; then ok "$m: MMSSTV's transmitter code, pulses vs VIS code $p ms"; else bad "$m: MMSSTV's transmitter code, pulses vs VIS code ${p:--} ms (within 0.5)"; fi
  fi
done
# the narrow modes (24 bit VIS code of 22 ms bits, 1900 Hz sync): the sync pulse of the first line follows the last bit (9 ms in MP-N, 8 ms in MC-N; measured 0.5 ms long)
for spec in MP73-N:9.5 MP110-N:9.5 MP140-N:9.5 MC110-N:8.5 MC140-N:8.5 MC180-N:8.5; do
  IFS=: read -r m want <<<"$spec"
  magick -size 320x256 xc:gray50 "$T/p.png"
  "$QSSTV" --encode "$T/p.png" --mode $m --wav-out "$T/q.wav" >/dev/null 2>&1 || { bad "$m: no transmission"; continue; }
  run=$(py narrowsync.py "$T/q.wav")
  if near "$run" "$want" 1.2; then ok "$m: 1900 Hz sync after the VIS code $run ms (want $want)"; else bad "$m: 1900 Hz sync after the VIS code $run ms, want $want"; fi
done
run=$(py narrowsync.py "$HERE/mmsstv_mp110n_start.wav")
if near "$run" 9.5 1.2; then ok "MMSSTV.EXE MP110-N (recorded): 1900 Hz sync after the VIS code $run ms (want 9.5)"; else bad "MMSSTV.EXE MP110-N (recorded): 1900 Hz sync after the VIS code $run ms, want 9.5"; fi
run=$(py stoprun.py "$HERE/mmsstv_scottie1_start.wav")
if near "$run" 38.5 1.2; then ok "MMSSTV.EXE Scottie 1 (recorded): stop bit + sync $run ms (want 38.5)"; else bad "MMSSTV.EXE Scottie 1 (recorded): stop bit + sync $run ms, want 38.5"; fi
run=$(py stoprun.py "$HERE/mmsstv_bw8_start.wav")
if near "$run" 36 1.2; then ok "MMSSTV.EXE B/W 8 (recorded): stop bit + sync $run ms (want 36)"; else bad "MMSSTV.EXE B/W 8 (recorded): stop bit + sync $run ms, want 36"; fi
run=$(py stoprun.py "$HERE/mmsstv_p3_start.wav")
if near "$run" 35.2 1.2; then ok "MMSSTV.EXE P3 (recorded): stop bit + sync $run ms (want 35.2)"; else bad "MMSSTV.EXE P3 (recorded): stop bit + sync $run ms, want 35.2"; fi
echo "$fail failed"
[ $fail = 0 ]
