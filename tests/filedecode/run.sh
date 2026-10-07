#!/bin/bash
# End-to-end test of decoding from a file with the real application:  qsstv --batch
#
# The loopback harness (tests/jb60_loopback) writes WAV files with the real transmitter preamble and VIS, and
# the built qsstv decodes them headless, so VIS detection, line sync, and the file
# reader are all exercised. The application runs with a throw-away HOME, so your settings are not touched.
#
#   tests/filedecode/run.sh                       # uses ../../build-qt6/qsstv
#   QSSTV_BIN=/path/to/qsstv tests/filedecode/run.sh
#
# Needs: the built qsstv, g++/Qt6 headers (for the harness), and ffmpeg (optional: adds format/rate variants).
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-qt6/qsstv}
LB="$HERE/../jb60_loopback/loopback"
[ -x "$QSSTV" ] || { echo "qsstv not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
[ -x "$LB" ] || (cd "$HERE/../jb60_loopback" && ./build.sh) || { echo "cannot build the loopback harness"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
# check "name" expected_exit actual_exit
expect_exit() { [ "$2" = "$3" ] && ok "$1 (exit $3)" || bad "$1: exit $3, expected $2"; }
luma() { "$LB" --compare "$1" "$2" | awk '{print $3}'; }          # luma PSNR of two PNGs
ge()   { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }         # a >= b
run()  { timeout 300 "$QSSTV" --batch "$@" 2>"$T/stderr"; }        # batch decode; stdout is the result lines

echo "== generating recordings"
for m in jb pd; do
  "$LB" $m --image card --vis --wav "$T/$m.wav" --out "$T/$m" > "$T/$m.harness"
  eval "harness_$m=$(awk '/PSNR/{print $9}' "$T/$m.harness")"
done
"$LB" jb --image card --vis --count 2 --wav "$T/jb2.wav" > /dev/null
"$LB" jb --image card --chroma-wide --vis --wav "$T/jbw.wav" --out "$T/jbw" > /dev/null   # JB60's "wide filter" is chroma-only
# Timing references (see the receive-timing check below): the harness decodes with the same trimmed RX back porch
# as the application but, unlike the application, has no sync-detector lag, so it must be delayed by that lag.
"$LB" jb --image card --tshift 2 --out "$T/jbref" > /dev/null
"$LB" jb --image card --chroma-wide --tshift 2 --out "$T/jbwref" > /dev/null

echo "== decode, QSSTV engine (VIS detection + sync through the real app)"
declare -A MODE=([jb]=JB60 [pd]=PD120)
for m in jb pd; do
  run -o "$T/o_$m" "$T/$m.wav"; rc=$?
  expect_exit "$m: exit code" 0 $rc
  png="$T/o_$m/${m}_1_${MODE[$m]}.png"
  if [ -f "$png" ]; then
    l=$(luma "$T/${m}_src.png" "$png"); h=$(eval echo \$harness_$m)
    ge "$l" "$(awk -v h="$h" 'BEGIN{print h-1.0}')" && ok "$m: luma PSNR $l dB (harness $h dB)" || bad "$m: luma PSNR $l dB, harness $h dB"
  else
    bad "$m: no ${MODE[$m]} picture written"
  fi
done

echo "== JB60 receive-timing regression"
# JB60's RX back porch (src/sstv/sstvparam.cpp) trims out the ~2.5 sample (12 kHz) lag between the sync
# detector's filter chain and the video filter's, so the application samples where an ideal receiver would. The
# loopback harness uses the same trimmed porch but starts its line timing from the transmitter (no sync detector,
# so no lag to cancel), which leaves it ~2 samples early; its reference is therefore generated with --tshift 2,
# the lag the trim cancels. Measured: the application matches that reference at 54 dB (tshift 1.5: 42, 2.5: 47);
# against an unshifted reference it reads ~31 dB however correct the timing is (issue #16 -- the check compared
# against the wrong reference). NOTE what this guards: the harness shares sstvparam.cpp, so changing the trim
# moves both sides and is NOT caught here (verified: bp 0.00208 still reads 54 dB). It catches a change in the
# sync detector / video chain lag relative to the harness's model, e.g. a filter or buffering change in the
# application. The wide check uses the harness's chroma-only wide track, which is what JB60's wide filter now is.
# 35 dB leaves headroom for machine to machine filter-arithmetic noise.
timing_check() {
  local label=$1 ref=$2 png=$3
  if [ -f "$png" ]; then
    local l=$(luma "$ref" "$png")
    ge "$l" 35 && ok "$label: matches the harness at the application's sync lag, luma PSNR $l dB" \
                || bad "$label: matches the harness at the application's sync lag, luma PSNR $l dB (want >= 35 dB)"
  else
    bad "$label: no picture to check"
  fi
}
timing_check "jb narrow filter" "$T/jbref_rx.png" "$T/o_jb/jb_1_JB60.png"
run --wide-filter on -o "$T/o_jbw" "$T/jbw.wav"
timing_check "jb wide filter" "$T/jbwref_rx.png" "$T/o_jbw/jbw_1_JB60.png"

echo "== JB60 right edge (issue #18)"
# JB60's last Cb slot sits against the sync pulse and used to decode as a green/yellow strip down the right edge
# (the application's front end smears it over ~5 Cb slots; the loopback harness alone under-sized the first
# fix). Row-averaged colour error of the last 8 columns against the source, via the application's own decode.
# Un-fixed it is ~60 counts on the card image; fixed it is within a few counts of the interior block.
edge_check() {
  local label=$1 src=$2 png=$3
  if [ -f "$png" ]; then
    read ec el ic il <<<"$("$LB" --edge "$src" "$png")"
    ge "15" "$ec" && ok "$label: right-edge colour error $ec (interior $ic)" \
                  || bad "$label: right-edge colour error $ec counts (interior $ic; want <= 15)"
  else
    bad "$label: no picture to check"
  fi
}
edge_check "jb narrow filter" "$T/jb_src.png" "$T/o_jb/jb_1_JB60.png"
edge_check "jb wide filter" "$T/jbw_src.png" "$T/o_jbw/jbw_1_JB60.png"

echo "== options"
run --mode PD120 -o "$T/o_mode" "$T/pd.wav"; expect_exit "forced correct mode" 0 $?
run --mode M1 -o "$T/o_wrongmode" "$T/pd.wav"; expect_exit "forced wrong mode finds nothing" 1 $?
run --wide-filter on -o "$T/o_wide" "$T/jb.wav"; expect_exit "wide filter" 0 $?
run -o "$T/o_two" "$T/jb2.wav"; rc=$?
n=$(ls "$T"/o_two/*.png 2>/dev/null | wc -l)
[ $rc = 0 ] && [ "$n" = 2 ] && ok "two pictures in one file -> $n files" || bad "two pictures in one file: exit $rc, $n files"

echo "== truncated recording (45% of the picture)"
python3 - "$T/jb.wav" "$T/trunc.wav" <<'EOF'
import sys
d=open(sys.argv[1],'rb').read()
open(sys.argv[2],'wb').write(d[:44+int((len(d)-44)*0.45)])
EOF
run -o "$T/o_trunc" "$T/trunc.wav"; expect_exit "truncated file still saves the partial picture" 0 $?

if command -v ffmpeg >/dev/null; then
  echo "== other formats (ffmpeg): must decode like the 48 kHz file"
  base=$(luma "$T/jb_src.png" "$T/o_jb/jb_1_JB60.png")
  while read -r name args; do
    ffmpeg -loglevel error -y -i "$T/jb.wav" $args "$T/$name.wav" || { bad "$name: ffmpeg failed"; continue; }
    [ "$name" = list ] && { grep -aq LIST "$T/list.wav" && ok "list: the file really has a LIST chunk" || bad "list: ffmpeg wrote no LIST chunk, test is void"; }
    run -o "$T/o_$name" "$T/$name.wav"; rc=$?
    png="$T/o_$name/${name}_1_JB60.png"
    if [ $rc = 0 ] && [ -f "$png" ]; then
      l=$(luma "$T/jb_src.png" "$png")
      ge "$l" "$(awk -v b="$base" 'BEGIN{print b-0.3}')" && ok "$name: luma PSNR $l dB (48 kHz file $base dB)" || bad "$name: luma PSNR $l dB, 48 kHz file $base dB"
    else
      bad "$name: exit $rc, no picture"
    fi
  done <<'EOF'
44k1     -ar 44100
22k05    -ar 22050
8k_u8    -ar 8000 -c:a pcm_u8
96k_24   -ar 96000 -c:a pcm_s24le
st_f32   -ac 2 -c:a pcm_f32le
list     -c:a pcm_s16le -metadata title=x
EOF

  echo "== compressed formats via Qt Multimedia (mp3, flac, ogg, aac)"
  for ext in mp3 flac ogg aac; do
    ffmpeg -loglevel error -y -i "$T/jb.wav" "$T/jbc.$ext" || { bad "$ext: ffmpeg cannot encode it, skipped"; continue; }
    run -o "$T/o_c$ext" "$T/jbc.$ext"; rc=$?
    png="$T/o_c$ext/jbc_1_JB60.png"
    if [ $rc = 0 ] && [ -f "$png" ]; then ok "$ext: decoded a picture"; else bad "$ext: exit $rc, no picture"; fi
  done
else
  echo "== (ffmpeg not installed: skipping the format/sample-rate variants)"
fi

echo "== bad input"
python3 -c "import wave; w=wave.open('$T/silence.wav','wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(48000); w.writeframes(b'\\0\\0'*48000*20); w.close()"
run -o "$T/o_sil" "$T/silence.wav"; expect_exit "silence: no picture" 1 $?
echo "this is not audio" > "$T/notwav.wav"
run "$T/notwav.wav"; expect_exit "not a WAV file" 2 $?
grep -q "undecodable audio" "$T/stderr" && ok "not a WAV: message names the problem" || bad "not a WAV: no useful message ($(cat "$T/stderr"))"
run "$T/missing.wav"; expect_exit "missing file" 2 $?
run; expect_exit "--batch without files" 2 $?
run --mode NOSUCH "$T/jb.wav"; expect_exit "unknown --mode" 2 $?

echo "== help works without a display"
out=$(env -u DISPLAY -u WAYLAND_DISPLAY -u QT_QPA_PLATFORM "$QSSTV" --help 2>&1); rc=$?
[ $rc = 0 ] && echo "$out" | grep -q -- "--batch" && ok "--help" || bad "--help (exit $rc)"
n=$(env -u DISPLAY -u WAYLAND_DISPLAY -u QT_QPA_PLATFORM "$QSSTV" --list-modes 2>&1 | grep -c JB60)
[ "$n" = 1 ] && ok "--list-modes lists JB60" || bad "--list-modes"

echo
echo "passed $pass, failed $fail"
[ $fail = 0 ]
