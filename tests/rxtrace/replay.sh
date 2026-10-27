#!/bin/bash
# Decode a recording from tests/rxtrace/capture.sh with the real receiver, once per Sensitivity and "Save if Complete" value
# and per binary, and count the pictures that come out (issue #135).
#
#   tests/rxtrace/replay.sh [-b label=binary]... [-s "0 1 2 3"] [-m "25 50"] [-j jobs] hf.wav OUTDIR
#
#   -b  binaries to compare, for example  -b head=build-cmake/qsstv-experimental -b before-129=/path/to/old/qsstv-experimental
#       (default: head=$QSSTV_BIN or ../../build-cmake/qsstv-experimental)
#   -s  Sensitivity values: 0 Low, 1 Normal, 2 High, 3 DX (default: all four)
#   -m  Save if Complete values in percent (default: 25 and 50)
#   -j  parallel decodes (default 6)
#
# Every decode runs with a throw-away HOME that holds a COPY of your configuration ($QSSTV_CONF, default
# ~/.config/qsstv-experimental/qsstv-experimental.conf) with the Sensitivity and Save if Complete changed and PTT/CAT off,
# so your settings are never touched. OUTDIR/<label>_s<sens>_m<min>/ gets the pictures; a binary that knows
# QSSTV_RX_TRACE also leaves trace.tsv there (tests/rxtrace/summarize.py).
HERE=$(cd "$(dirname "$0")" && pwd)
CONF=${QSSTV_CONF:-$HOME/.config/qsstv-experimental/qsstv-experimental.conf}
SENS="0 1 2 3"
MINS="25 50"
JOBS=6
BINS=()
while getopts "b:s:m:j:" o; do
  case $o in
    b) BINS+=("$OPTARG") ;;
    s) SENS=$OPTARG ;;
    m) MINS=$OPTARG ;;
    j) JOBS=$OPTARG ;;
    *) sed -n 2,19p "$0"; exit 2 ;;
  esac
done
shift $((OPTIND-1))
WAV=$1; OUT=$2
[ -r "$WAV" ] && [ -n "$OUT" ] || { sed -n 2,19p "$0"; exit 2; }
[ -r "$CONF" ] || { echo "no configuration at $CONF (set QSSTV_CONF)"; exit 2; }
[ ${#BINS[@]} -gt 0 ] || BINS=("head=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}")
for b in "${BINS[@]}"; do [ -x "${b#*=}" ] || { echo "${b#*=}: not executable"; exit 2; }; done
mkdir -p "$OUT" || exit 2
WAV=$(readlink -f "$WAV")

cell() {   # label binary sens min
  local label=$1 bin=$2 s=$3 m=$4 d="$OUT/${1}_s${3}_m${4}"
  mkdir -p "$d/home/.config/qsstv-experimental" "$d/pics"
  sed -E -e "s/^sensitivity=.*/sensitivity=$s/" -e "s/^minCompletion=.*/minCompletion=$m/" \
         -e 's/^(enableSerialPTT|enableCAT|enableXMLRPC)=.*/\1=false/' "$CONF" > "$d/home/.config/qsstv-experimental/qsstv-experimental.conf"
  local t0=$SECONDS
  HOME="$d/home" XDG_CONFIG_HOME="$d/home/.config" XDG_DATA_HOME="$d/home/.local/share" QT_QPA_PLATFORM=offscreen \
    QSSTV_RX_TRACE="$d/trace.tsv" "$bin" --batch -o "$d/pics" "$WAV" > "$d/stdout" 2> "$d/stderr"
  echo "$? $((SECONDS-t0))" > "$d/done"
}

for b in "${BINS[@]}"; do
  for s in $SENS; do
    for m in $MINS; do
      while [ "$(jobs -r | wc -l)" -ge "$JOBS" ]; do sleep 1; done
      cell "${b%%=*}" "${b#*=}" "$s" "$m" &
    done
  done
done
wait

names=(Low Normal High DX)
printf '\n%-14s %-7s %-9s %9s %9s\n' binary sens "min %" pictures "decode s"
for b in "${BINS[@]}"; do
  for s in $SENS; do
    for m in $MINS; do
      d="$OUT/${b%%=*}_s${s}_m${m}"
      read -r rc secs < "$d/done" 2>/dev/null || { rc=?; secs=?; }
      printf '%-14s %-7s %-9s %9s %9s\n' "${b%%=*}" "${names[$s]}" "$m" "$(ls "$d/pics" 2>/dev/null | wc -l)" "$secs"
    done
  done
done
