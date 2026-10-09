#!/bin/bash
# Run two QSSTV instances connected by a PipeWire/PulseAudio virtual audio cable, to test
# TX -> RX end to end on one machine (e.g. calibration, issue #48).
#
#   tools/dual-instance-test.sh up      create the cable, start TX and RX instances
#   tools/dual-instance-test.sh down    stop the instances and remove the cable
#
# The cable is a null sink: the TX instance plays to "QSSTV_Cable" and the RX instance
# records from "Monitor of QSSTV_Cable". Each instance gets its own settings/data dir
# (default /tmp/qsstv-test/{tx,rx}) so they don't share ~/.config.
#
# First run only: in each instance set Options > Configuration > Sound
#   TX instance: output = QSSTV_Cable      RX instance: input = Monitor of QSSTV_Cable
# (and PTT = none). The choices persist in the per-instance dirs.
#
# Env: QSSTV_BIN (default build-cmake/qsstv), TEST_DIR (default /tmp/qsstv-test),
#      RX_FILTER (grep pattern for the RX terminal output, default CALDBG; "" = show all)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${QSSTV_BIN:-$ROOT/build-cmake/qsstv}"
DIR="${TEST_DIR:-/tmp/qsstv-test}"
SINK=qsstv_cable
FILTER="${RX_FILTER-CALDBG}"

down() {
  for i in tx rx; do
    [ -f "$DIR/$i.pid" ] && kill "$(cat "$DIR/$i.pid")" 2>/dev/null || true
    rm -f "$DIR/$i.pid"
  done
  [ -f "$DIR/module.id" ] && pactl unload-module "$(cat "$DIR/module.id")" 2>/dev/null || true
  rm -f "$DIR/module.id"
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
    echo "cable created (TX output: QSSTV_Cable, RX input: Monitor of QSSTV_Cable)"
    XDG_CONFIG_HOME="$DIR/tx/cfg" XDG_DATA_HOME="$DIR/tx/data" "$BIN" > "$DIR/tx.log" 2>&1 &
    echo $! > "$DIR/tx.pid"
    echo "TX instance pid $(cat "$DIR/tx.pid"), log $DIR/tx.log"
    trap down EXIT INT TERM
    export XDG_CONFIG_HOME="$DIR/rx/cfg" XDG_DATA_HOME="$DIR/rx/data"
    echo "RX instance in foreground (Ctrl-C stops both and removes the cable)"
    if [ -n "$FILTER" ]; then "$BIN" > >(grep --line-buffered "$FILTER") 2>&1 &
    else "$BIN" 2>&1 &
    fi
    echo $! > "$DIR/rx.pid"
    wait $!
    ;;
  *) echo "usage: $0 up|down"; exit 1 ;;
esac
