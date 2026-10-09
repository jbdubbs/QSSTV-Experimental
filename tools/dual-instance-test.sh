#!/bin/bash
# Run two QSSTV instances connected by a PipeWire/PulseAudio virtual audio cable, to test
# TX -> RX end to end on one machine (e.g. calibration, issue #48).
#
#   tools/dual-instance-test.sh up      create the cable, start TX and RX instances
#   tools/dual-instance-test.sh down    stop the instances and remove the cable
#
# The cable is a null sink: the TX instance plays to "QSSTV_Cable" and the RX instance
# records from the virtual microphone "QSSTV_Cable_In" (a remapped monitor). Each instance gets its own settings/data dir
# (default /tmp/qsstv-test/{tx,rx}) so they don't share ~/.config.
#
# The devices are pre-set on first use (TX output = QSSTV_Cable, RX input = QSSTV_Cable_In).
# Other settings (e.g. PTT = none) you change in each instance; QSSTV saves them only when
# you quit it from its own window, so do that rather than Ctrl-C if you want them kept.
# Delete $TEST_DIR/<tx|rx>/cfg to start an instance from scratch.
#
# Env: QSSTV_BIN (default build-cmake/qsstv), TEST_DIR (default /tmp/qsstv-test),
#      RX_FILTER (grep pattern for terminal output of both instances, default: all output)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${QSSTV_BIN:-$ROOT/build-cmake/qsstv}"
DIR="${TEST_DIR:-/tmp/qsstv-test}"
SINK=qsstv_cable
FILTER="${RX_FILTER-}"

down() {
  for i in tx rx; do
    [ -f "$DIR/$i.pid" ] && kill "$(cat "$DIR/$i.pid")" 2>/dev/null || true
    rm -f "$DIR/$i.pid"
  done
  for m in source.id module.id; do   # source first, it depends on the sink
    [ -f "$DIR/$m" ] && pactl unload-module "$(cat "$DIR/$m")" 2>/dev/null || true
    rm -f "$DIR/$m"
  done
  echo "stopped"
}

case "${1:-up}" in
  down) down ;;
  up)
    command -v pactl >/dev/null || { echo "pactl not found"; exit 1; }
    [ -x "$BIN" ] || { echo "QSSTV binary not found: $BIN"; exit 1; }
    mkdir -p "$DIR"/{tx,rx}/{cfg,data}
    down >/dev/null
    pactl load-module module-null-sink sink_name=$SINK \
      sink_properties=device.description=QSSTV_Cable > "$DIR/module.id"
    # Qt only lists real sources as input devices, not sink monitors, so expose the
    # monitor as a proper virtual microphone.
    pactl load-module module-remap-source master=$SINK.monitor source_name=qsstv_cable_in \
      source_properties=device.description=QSSTV_Cable_In > "$DIR/source.id"
    echo "cable created (TX output: QSSTV_Cable, RX input: QSSTV_Cable_In)"
    # QSSTV only writes its settings on a clean quit, which killing it here is not, so
    # pre-seed the audio devices on first use (an existing conf is left alone).
    seed() {  # <instance> <input device> <output device>
      local f="$DIR/$1/cfg/ON4QZ/qsstv_9.0.conf"
      [ -f "$f" ] && return
      mkdir -p "$(dirname "$f")"
      printf '[SOUND]\ninputAudioDevice=%s\noutputAudioDevice=%s\n' "$2" "$3" > "$f"
    }
    seed tx default QSSTV_Cable
    seed rx QSSTV_Cable_In default
    trap down EXIT INT TERM
    # Both instances run together; each one's full output goes to $DIR/<tx|rx>.log and
    # (filtered by RX_FILTER, prefixed [TX]/[RX]) to this terminal.
    for i in tx rx; do
      P=$(echo "$i" | tr a-z A-Z)
      XDG_CONFIG_HOME="$DIR/$i/cfg" XDG_DATA_HOME="$DIR/$i/data" "$BIN" 2>&1 \
        > >(tee "$DIR/$i.log" | grep --line-buffered -e "${FILTER:-.}" | sed -u "s/^/[$P] /") &
      echo $! > "$DIR/$i.pid"
    done
    echo "TX and RX instances running; Ctrl-C here stops both and removes the cable"
    wait
    ;;
  *) echo "usage: $0 up|down"; exit 1 ;;
esac
