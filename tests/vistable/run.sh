#!/bin/bash
# Every mode must have a VIS code of its own, and the receiver must read back the mode the transmitter sent (issue #146).
#
# MR175 had the code of MR140 in the mode table, and lookupVIS() returns the first match, so a transmission of one mode announced
# the other and the receiver was held to the wrong mode. Two checks:
#   - the VIS codes in SSTVTable (src/sstv/sstvparam.cpp) are all different (0 is "no VIS code": FAX480 and the "No Mode" row)
#   - for every mode of --list-modes: transmit a picture with the real transmitter (--encode), keep the first 8 s (the whole
#     VIS code), decode it headless with the real receiver (--batch) and compare the mode of the first VIS code seen with the
#     mode sent
#
#   tests/vistable/run.sh                          # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/vistable/run.sh
#   VISTABLE_MODES="MR140 MR175" tests/vistable/run.sh     # only these modes
#
# Modes in EXCEPTIONS are known not to be read back; one that is read back after all is reported, so it can come off the list.
#
# Needs: the built qsstv-experimental, ffmpeg (cuts the transmission short), python3. About 3 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
TABLE=$HERE/../../src/sstv/sstvparam.cpp
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
[ -r "$TABLE" ] || { echo "mode table not found at $TABLE"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to cut the transmissions short"; exit 2; }
command -v python3 >/dev/null || { echo "python3 is needed"; exit 2; }

declare -A EXCEPTIONS=(
  [FAX480]="no VIS code"
  [MC110-N]="its VIS code is never decoded (#145)"
  [MC140-N]="its VIS code is never decoded (#145)"
  [MC180-N]="its VIS code is never decoded (#145)"
)

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export QT_QPA_PLATFORM=offscreen
TMO=$(command -v timeout || command -v gtimeout)

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }

echo "== the VIS codes in the mode table are all different"
dups=$(python3 -I - "$TABLE" <<'EOF'
import re, sys, collections
row = re.compile(r'^\s*\{"[^"]*"\s*,\s*"([^"]*)"\s*,\s*\w+\s*,\s*[\d.]+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*(0[xX][0-9A-Fa-f]+)')
codes = collections.defaultdict(list)
n = 0
for line in open(sys.argv[1], errors='replace'):
    m = row.match(line)
    if m:
        n += 1
        code = int(m.group(2), 16)
        if code:
            codes[code].append(m.group(1))
if n < 40:
    print('PARSE %d rows' % n)
for code, names in sorted(codes.items()):
    if len(names) > 1:
        print('0x%X %s' % (code, ' '.join(names)))
EOF
)
case "$dups" in
  PARSE*) bad "could not read the mode table ($dups)" ;;
  "")     ok "no VIS code is used twice" ;;
  *)      while read -r line; do bad "one VIS code for several modes: $line"; done <<< "$dups" ;;
esac

echo "== every mode is read back as the mode that was sent"
modes=${VISTABLE_MODES:-$("$QSSTV" --list-modes 2>/dev/null | cut -f1 | grep -E '^[A-Za-z0-9-]+$')}
[ -n "$modes" ] || { bad "--list-modes lists no mode"; modes=; }
PIC=$HERE/../jb60_loopback/images/card.png

roundtrip() {      # roundtrip MODE: leaves the mode of the first VIS code in $T/MODE/seen ("encode" if the encoder failed, "none")
  local m=$1 d=$T/$1
  mkdir -p "$d/out" "$d/home"
  export HOME=$d/home XDG_CONFIG_HOME=$d/home/.config XDG_DATA_HOME=$d/home/.local/share TMPDIR=$d
  if ! "$QSSTV" --encode "$PIC" --mode "$m" --wav-out "$d/full.wav" >/dev/null 2>&1; then echo encode > "$d/seen"; return; fi
  ffmpeg -loglevel error -y -t 8 -i "$d/full.wav" "$d/clip.wav" 2>/dev/null
  : > "$d/full.wav"                                   # the whole transmission is large, only the clip is needed
  QSSTV_RX_TRACE="$d/trace.tsv" ${TMO:+$TMO 120} "$QSSTV" --batch -o "$d/out" "$d/clip.wav" >/dev/null 2>&1
  local seen
  seen=$(grep -a -P '\tVIS\t' "$d/trace.tsv" 2>/dev/null | head -1 | tr '\t' '\n' | sed -n 's/^mode=//p' | head -1)
  echo "${seen:-none}" > "$d/seen"
}

for m in $modes; do
  roundtrip "$m" &
  while [ "$(jobs -rp | wc -l)" -ge 4 ]; do sleep 0.2; done
done
wait

for m in $modes; do
  seen=$(cat "$T/$m/seen" 2>/dev/null)
  if [ "$seen" = "$m" ]; then
    if [ -n "${EXCEPTIONS[$m]}" ]; then ok "$m: read back after all (${EXCEPTIONS[$m]}): take it off the exception list"
    else ok "$m"; fi
  elif [ -n "${EXCEPTIONS[$m]}" ]; then
    echo "  skip $m: ${EXCEPTIONS[$m]} (read as: ${seen:-nothing})"
  else
    bad "$m is read back as: ${seen:-nothing}"
  fi
done

echo; echo "$pass passed, $fail failed"
[ $fail = 0 ]
