#!/bin/bash
# Builds the clean test transmissions for the slant sweep: a black picture with one vertical white line, encoded by the real
# QSSTV transmitter, one WAV per mode, into the directory given (default ./clean).
#   tests/slant/sweep/prep.sh [outdir]
# needs build-cmake/qsstv-experimental (QSSTV_BIN) and tests/slant/slantpng (tests/slant/loopback.sh builds it)
HERE=$(cd "$(dirname "$0")" && pwd)
Q=${QSSTV_BIN:-$HERE/../../../build-cmake/qsstv-experimental}
SP=$HERE/../slantpng
OUT=${1:-$HERE/clean}
[ -x "$Q" ] || { echo "no $Q"; exit 2; }
[ -x "$SP" ] || { echo "no $SP (build it: tests/slant/loopback.sh)"; exit 2; }
mkdir -p "$OUT"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T XDG_CONFIG_HOME=$T/.config XDG_DATA_HOME=$T/.local/share QT_QPA_PLATFORM=offscreen
# mode width height line column
for spec in "S1 320 256 160" "S2 320 256 160" "M1 320 256 160" "M2 320 256 160" "R36 320 240 160" "PD120 640 496 320" "PD50 320 256 160"; do
  set -- $spec
  m=$1
  "$SP" make "$T/$m.png" "$2" "$3" "$4" || exit 2
  "$Q" --encode "$T/$m.png" --mode "$m" --wav-out "$OUT/$m.wav" >/dev/null 2>&1 || { echo "encode $m failed"; exit 2; }
  echo "$m ${2}x${3}" > "$OUT/$m.size"
done
ls -l "$OUT"/*.wav
