#!/bin/bash
# run1.sh WAV EXPECT_W EXPECT_H [AUTOSLANT=true] -> one TSV line: rows slope shift_px (QSSTV-Experimental, Auto slant on/off)
#   rows     = lines of the saved picture the line fit could use (found), of total
#   slope    = remaining slant of the white line in px per picture row, shift_px = slope * total rows (across the picture)
HERE=$(cd "$(dirname "$0")" && pwd)
Q=${QSSTV_BIN:-$HERE/../../../build-cmake/qsstv-experimental}
SP=$HERE/../slantpng
W=$(readlink -f "$1"); AS=${2:-true}
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T XDG_CONFIG_HOME=$T/.config XDG_DATA_HOME=$T/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$T/.config/qsstv-experimental" "$T/out"
cat > "$T/.config/qsstv-experimental/qsstv-experimental.conf" <<CONF
[RX]
autoSave=true
autoSlantAdjust=$AS
minCompletion=0
sensitivity=${SENS:-1}
sstvModeIndexRx=0
CONF
QSSTV_RX_TRACE=$T/trace.tsv timeout 400 "$Q" --batch --min-complete 0 -o "$T/out" "$W" >"$T/stdout" 2>"$T/stderr"
n=$(ls "$T"/out/*.png 2>/dev/null | wc -l)
if [ "$n" = 0 ]; then echo -e "none\t0\t0\t0\t0"; exit 0; fi
# the biggest picture (most rows)
best=""; bu=-1
for p in "$T"/out/*.png; do
  r=$("$SP" fit "$p")
  u=$(echo "$r" | sed 's/.*used=\([0-9]*\).*/\1/')
  if [ "$u" -gt "$bu" ]; then bu=$u; best="$r"; fi
done
slope=$(echo "$best" | sed 's/.*slope=\([-0-9.]*\).*/\1/')
used=$(echo "$best" | sed 's/.*used=\([0-9]*\).*/\1/')
total=$(echo "$best" | sed 's/.*total=\([0-9]*\).*/\1/')
valid=$(echo "$best" | sed 's/valid=\([01]\).*/\1/')
shift=$(awk -v s="$slope" -v t="$total" 'BEGIN{printf "%.2f",s*t}')
echo -e "$n\t$valid\t$used/$total\t$slope\t$shift"
