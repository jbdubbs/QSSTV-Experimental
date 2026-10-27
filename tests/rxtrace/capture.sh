#!/bin/bash
# Record the real radio input and run the headless receiver on it at the same time, with the receiver trace (issue #135).
#
#   tests/rxtrace/capture.sh [-d seconds] [-s pipewire-source] [-n receiver-device] OUTDIR
#
# OUTDIR gets hf.wav (the raw input, 16 bit mono at the source's rate: the regression input for replay.sh), trace.tsv (the
# receiver's events, see src/sstv/rxtrace.h), pics/ (the pictures the receiver saves), receiver.log and session.txt.
#
# The receiver runs with a throw-away HOME that holds a COPY of your configuration (sensitivity, Save if Complete, mode, ...),
# with the serial PTT, CAT and XML-RPC switched off: a receiver never keys a radio, and a running QSSTV that holds the PTT
# port would make a second one stop behind an invisible "Unable to open serial port" box (--listen has no window).
#
# Defaults: one hour from "USB Audio Device Mono". QSSTV_BIN selects the binary (default ../../build-cmake/qsstv-experimental).
# Needs pw-record (or parecord) and a running PipeWire/PulseAudio server. Stop early with Ctrl+C / SIGTERM: the recording
# and the receiver are finished cleanly.
HERE=$(cd "$(dirname "$0")" && pwd)
CONF=${QSSTV_CONF:-$HOME/.config/qsstv-experimental/qsstv-experimental.conf}
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
SECONDS_TO_RUN=3600
SOURCE=alsa_input.usb-C-Media_Electronics_Inc._USB_Audio_Device-00.mono-fallback
DEVICE="USB Audio Device Mono"
while getopts "d:s:n:" o; do
  case $o in
    d) SECONDS_TO_RUN=$OPTARG ;;
    s) SOURCE=$OPTARG ;;
    n) DEVICE=$OPTARG ;;
    *) sed -n 2,13p "$0"; exit 2 ;;
  esac
done
shift $((OPTIND-1))
OUT=$1
[ -n "$OUT" ] || { sed -n 2,13p "$0"; exit 2; }
[ -r "$CONF" ] || { echo "no configuration at $CONF (set QSSTV_CONF)"; exit 2; }
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v pw-record >/dev/null || { echo "pw-record is needed"; exit 2; }
pactl list short sources 2>/dev/null | cut -f2 | grep -qx "$SOURCE" || { echo "no PipeWire source called $SOURCE (pactl list short sources)"; exit 2; }
mkdir -p "$OUT/pics" || exit 2
[ -e "$OUT/hf.wav" ] && { echo "$OUT/hf.wav exists already, use another directory"; exit 2; }

RXHOME=$OUT/home
mkdir -p "$RXHOME/.config/qsstv-experimental" || exit 2
sed -E 's/^(enableSerialPTT|enableCAT|enableXMLRPC)=.*/\1=false/' "$CONF" > "$RXHOME/.config/qsstv-experimental/qsstv-experimental.conf"
RATE=$(pactl list sources 2>/dev/null | awk -v s="$SOURCE" '$1=="Name:"{on=($2==s)} on&&/Sample Specification/{print $NF; exit}' | tr -d 'Hz')
{
  echo "start_utc   $(date -u +%FT%TZ)"
  echo "seconds     $SECONDS_TO_RUN"
  echo "source      $SOURCE (${RATE:-44100} Hz)"
  echo "device      $DEVICE"
  echo "binary      $QSSTV"
  echo "git         $(git -C "$HERE" rev-parse --short HEAD 2>/dev/null)$(git -C "$HERE" diff --quiet 2>/dev/null || echo ' + uncommitted changes')"
  echo "settings    $(grep -E '^(sensitivity|minCompletion|sstvModeIndexRx|wideVideoFilter|autoSlantAdjust)=' "$CONF" | tr '\n' ' ') (copy of $CONF, PTT/CAT off)"
} > "$OUT/session.txt"

PIDS=
cleanup() {
  kill -INT $PIDS 2>/dev/null
  wait 2>/dev/null
  echo "end_utc     $(date -u +%FT%TZ)" >> "$OUT/session.txt"
}
trap 'exit 130' INT TERM
trap cleanup EXIT

pw-record --target "$SOURCE" --rate "${RATE:-44100}" --channels 1 --format s16 "$OUT/hf.wav" 2> "$OUT/pw-record.log" &
PIDS="$!"
HOME="$RXHOME" XDG_CONFIG_HOME="$RXHOME/.config" XDG_DATA_HOME="$RXHOME/.local/share" QSSTV_RX_TRACE="$OUT/trace.tsv" "$QSSTV" --listen --device "$DEVICE" -o "$OUT/pics" --timeout "$SECONDS_TO_RUN" > "$OUT/receiver.log" 2>&1 &
PIDS="$PIDS $!"
# the receiver ends itself at --timeout; the recording is stopped with it
wait ${PIDS##* }
