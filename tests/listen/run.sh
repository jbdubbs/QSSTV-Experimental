#!/bin/bash
# End-to-end test of receiving from the sound card on the command line with the real application:  qsstv-experimental --listen
#
# A PipeWire/PulseAudio null sink with a remapped monitor (a virtual microphone, as in tools/dual-instance-test.sh) is the
# "sound card": the loopback harness (tests/jb60_loopback) writes a recording with the real preamble and VIS, paplay plays
# it into the sink in real time and the built qsstv-experimental listens on the virtual microphone. The application runs
# with a throw-away HOME, so your settings are not touched. Takes about three minutes (two real time JB60 pictures).
#
#   tests/listen/run.sh                           # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/listen/run.sh
#
# Needs: the built qsstv-experimental, g++/Qt6 headers (for the harness), pactl + paplay and a running PipeWire/PulseAudio
# server. Without those it says so and exits 0 (skipped).
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
LB="$HERE/../jb60_loopback/loopback"
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
if ! command -v pactl >/dev/null || ! command -v paplay >/dev/null || ! pactl info >/dev/null 2>&1; then
  echo "skipped: needs pactl, paplay and a running PipeWire/PulseAudio server"
  exit 0
fi
[ -x "$LB" ] || (cd "$HERE/../jb60_loopback" && ./build.sh) || { echo "cannot build the loopback harness"; exit 2; }

T=$(mktemp -d)
SINK=qsstv_listen_test
MIC=QSSTV_Listen_Test_In
MODULES=
cleanup() {
  jobs -p | xargs -r kill 2>/dev/null
  for m in $MODULES; do pactl unload-module "$m" 2>/dev/null; done   # the microphone first, it depends on the sink
  rm -rf "$T"
}
trap cleanup EXIT
SINK_ID=$(pactl load-module module-null-sink sink_name=$SINK sink_properties=device.description=QSSTV_Listen_Test) || { echo "cannot create the virtual sink"; exit 2; }
MIC_ID=$(pactl load-module module-remap-source master=$SINK.monitor source_name=${SINK}_in source_properties=device.description=$MIC) || { echo "cannot create the virtual microphone"; exit 2; }
MODULES="$MIC_ID $SINK_ID"

# the sound server is found through XDG_RUNTIME_DIR, so pactl and paplay keep working with this HOME
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
expect_exit() { [ "$2" = "$3" ] && ok "$1 (exit $3)" || bad "$1: exit $3, expected $2"; }
ge()   { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }         # a >= b

# start a listener in the background; returns once it says it is listening. $1 = output dir, more arguments are passed on
listener_start() {
  local out=$1; shift
  : > "$T/stderr"
  "$QSSTV" --listen --device $MIC -o "$out" "$@" > "$T/stdout" 2> "$T/stderr" &
  LPID=$!
  for _ in $(seq 1 60); do grep -q 'listening on' "$T/stderr" && return 0; kill -0 $LPID 2>/dev/null || return 1; sleep 0.5; done
  return 1
}
listener_stop() {   # $1 = signal; sets LRC to the exit code
  kill -"$1" $LPID 2>/dev/null
  wait $LPID; LRC=$?
}

echo "== generating the recording"
"$LB" jb --image card --vis --wav "$T/jb.wav" --out "$T/jb" > "$T/jb.harness"
harness=$(awk '/PSNR/{print $9}' "$T/jb.harness")

echo "== option handling"
out=$("$QSSTV" --list-devices 2>/dev/null); rc=$?
expect_exit "--list-devices" 0 $rc
echo "$out" | cut -f1 | grep -qx "$MIC" && ok "--list-devices shows the virtual microphone" || bad "--list-devices does not show $MIC"
"$QSSTV" --listen --device no-such-input-xyz -o "$T/o" 2> "$T/stderr" >/dev/null; rc=$?
expect_exit "--device with an unknown name" 2 $rc
grep -q "$MIC" "$T/stderr" && ok "unknown --device lists the inputs" || bad "unknown --device did not list the inputs"
"$QSSTV" --listen "$T/jb.wav" 2>/dev/null >/dev/null; rc=$?
expect_exit "--listen with a file" 2 $rc
"$QSSTV" --device "$MIC" 2>/dev/null >/dev/null; rc=$?
expect_exit "--device without --listen" 2 $rc
"$QSSTV" --batch 2>/dev/null >/dev/null; rc=$?
expect_exit "--batch without a file" 2 $rc
"$QSSTV" --listen --min-complete 101 2>/dev/null >/dev/null; rc=$?
expect_exit "--min-complete above 100" 2 $rc
"$QSSTV" --min-complete 50 2>/dev/null >/dev/null; rc=$?
expect_exit "--min-complete without --batch/--listen" 2 $rc

echo "== silence: --timeout ends the run with exit code 1"
if listener_start "$T/o_silence" --timeout 4; then
  wait $LPID; rc=$?
  expect_exit "--timeout without a picture" 1 $rc
  [ ! -e "$T/o_silence" ] || [ -z "$(ls "$T/o_silence")" ] && ok "no file written" || bad "files written from silence"
else
  bad "the listener did not start: $(tail -3 "$T/stderr")"
fi

echo "== a serial PTT port that cannot be opened does not hold the receiver up (issue #136)"
# a window that holds the PTT port makes opening it fail just like a port that is not there; the receiver must not touch it
mkdir -p "$HOME/.config/qsstv-experimental"
printf '[radio1]\nenableSerialPTT=true\npttSerialPort=/dev/qsstv-no-such-port\n' > "$HOME/.config/qsstv-experimental/qsstv-experimental.conf"
if listener_start "$T/o_ptt" --timeout 4; then
  ok "the listener starts"
  wait $LPID; rc=$?
  expect_exit "--timeout with an unusable PTT port" 1 $rc
else
  bad "the listener did not start with an unusable PTT port: $(tail -3 "$T/stderr")"
  listener_stop KILL   # it sits behind a message box that nobody can answer, and ignores SIGINT
fi
rm -f "$HOME/.config/qsstv-experimental/qsstv-experimental.conf"

echo "== a picture through the virtual cable, stopped with SIGINT"
if listener_start "$T/o_full"; then
  paplay --device=$SINK "$T/jb.wav" &
  PPID_PLAY=$!
  wait $PPID_PLAY
  sleep 3   # the receiver sees the end of the picture a moment after the last line
  listener_stop INT
  expect_exit "SIGINT" 0 $LRC
  png=$(ls "$T"/o_full/JB60_[0-9]*_[0-9]*.png 2>/dev/null | head -1)
  if [ -n "$png" ]; then
    ok "picture written as $(basename "$png")"
    l=$("$LB" --compare "$T/jb_src.png" "$png" | awk '{print $3}')
    ge "$l" "$(awk -v h="$harness" 'BEGIN{print h-2.0}')" && ok "luma PSNR $l dB (harness $harness dB)" || bad "luma PSNR $l dB, harness $harness dB"
    [ "$(ls "$T/o_full" | wc -l)" = 1 ] && ok "exactly one picture" || bad "$(ls "$T/o_full" | wc -l) files for one picture"
    grep -q "^$MIC -> .*JB60_.*\.png (JB60, 640x496)$" "$T/stdout" && ok "result line on stdout" || bad "result line on stdout: $(cat "$T/stdout")"
  else
    bad "no JB60 picture written: $(tail -3 "$T/stderr")"
  fi
else
  bad "the listener did not start: $(tail -3 "$T/stderr")"
fi

echo "== a picture still being received is saved on SIGTERM"
if listener_start "$T/o_part"; then
  paplay --device=$SINK "$T/jb.wav" &
  PPID_PLAY=$!
  sleep 40   # about 60 % of the 61 s picture
  listener_stop TERM
  kill $PPID_PLAY 2>/dev/null; wait $PPID_PLAY 2>/dev/null
  expect_exit "SIGTERM" 0 $LRC
  png=$(ls "$T"/o_part/JB60_*.png 2>/dev/null | head -1)
  [ -n "$png" ] && ok "the unfinished picture was saved" || bad "no picture saved on SIGTERM: $(tail -3 "$T/stderr")"
else
  bad "the listener did not start: $(tail -3 "$T/stderr")"
fi

echo "== --min-complete: a picture below the limit is not saved"
if listener_start "$T/o_min" --min-complete 90; then
  paplay --device=$SINK "$T/jb.wav" &
  PPID_PLAY=$!
  sleep 40   # about 65 % of the picture, below the 90 % asked for; the default (25 %) saves it, see above
  listener_stop TERM
  kill $PPID_PLAY 2>/dev/null; wait $PPID_PLAY 2>/dev/null
  expect_exit "SIGTERM" 0 $LRC
  [ -z "$(ls "$T/o_min" 2>/dev/null)" ] && ok "nothing saved below --min-complete 90" || bad "saved $(ls "$T/o_min") although the picture was incomplete"
else
  bad "the listener did not start: $(tail -3 "$T/stderr")"
fi

echo "== the sound input disappearing ends the run with exit code 2"
if listener_start "$T/o_gone"; then
  pactl unload-module "$MIC_ID"; MODULES=$SINK_ID   # an unplugged USB card, as far as the application can tell
  for _ in $(seq 1 30); do kill -0 $LPID 2>/dev/null || break; sleep 0.5; done
  if kill -0 $LPID 2>/dev/null; then
    bad "still running 15 s after the input disappeared"
    listener_stop KILL
  else
    wait $LPID; rc=$?
    expect_exit "input disappeared" 2 $rc
    # either the audio stream reports the loss first ("sound device error") or the application notices the input is gone
    grep -q 'is gone\|sound device error' "$T/stderr" && ok "says what happened to the input" || bad "no message about the input: $(tail -3 "$T/stderr")"
  fi
else
  bad "the listener did not start: $(tail -3 "$T/stderr")"
fi

echo
echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
