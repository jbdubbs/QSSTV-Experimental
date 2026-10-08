#!/bin/bash
# DRM (digital SSTV) loopback test harness, headless (issue #61).
#
# qsstv --encode --drm writes a DRM transmission of a picture to a wav file with the real transmitter; qsstv --batch
# --drm decodes it with the real receiver and prints a "drm-stats:" line (sync / FAC / MSC percentages, MSC flaps,
# SNR). --rx-gain and --rx-noise (test aids, file source only) vary the level and add noise.
#
#   tests/drm/run.sh                    # the checks; exit status 0 when all pass
#   tests/drm/run.sh sweep              # report only: modes x gain x noise table, nothing is asserted
#   tests/drm/run.sh decode file.wav [qsstv --batch options]   # decode one recording, print its drm-stats
#   QSSTV_BIN=/path/to/qsstv tests/drm/run.sh
#
# Settings are never touched: the application runs with a throw-away HOME.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv}
IMG=${DRM_IMAGE:-$HERE/card160.png}   # small: 4-QAM transmissions of big pictures take many minutes
[ -x "$QSSTV" ] || { echo "qsstv not found at $QSSTV (set QSSTV_BIN)"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME"

# encode name [--drm-* options]  -> $T/name.wav
encode() { local n=$1; shift; timeout 900 "$QSSTV" --encode "$IMG" --drm --wav-out "$T/$n.wav" "$@" >/dev/null 2>"$T/enc.err" || { echo "encode $n failed"; cat "$T/enc.err"; return 1; }; }
# decode wav [options] -> $T/out.txt (stdout), exit status in $RC
# each decode gets its own HOME: a file saved by an earlier run would make the receiver report "already received"
decode() { local w=$1; shift; rm -rf "$T/o" "$T/home.d"; mkdir -p "$T/home.d"; HOME=$T/home.d XDG_CONFIG_HOME=$T/home.d/.config XDG_DATA_HOME=$T/home.d/.local/share timeout 900 "$QSSTV" --batch --drm -o "$T/o" "$@" "$w" >"$T/out.txt" 2>"$T/dec.err"; RC=$?; }
stat() { sed -n 's/.*drm-stats:.* '"$1"'=\([^ ]*\).*/\1/p' "$T/out.txt" | head -1; }   # stat msc -> 97%

if [ "$1" = decode ]; then
  shift; w=$1; shift
  decode "$w" "$@"; cat "$T/out.txt"; cat "$T/dec.err" | grep -v VDPAU; exit $RC
fi

if [ "$1" = sweep ]; then
  printf "%-28s %-9s %5s %5s %5s %5s %6s %6s %s\n" case extra time frame fac msc flaps snr rc/images
  for p in "B 4" "B 16" "B 64" "A 16" "E 16"; do
    set -- $p
    encode "m$1$2" --drm-mode $1 --drm-qam $2 || continue
    for extra in "" "--rx-gain -12" "--rx-gain -24" "--rx-gain 6" "--rx-noise -30" "--rx-noise -40"; do
      decode "$T/m$1$2.wav" $extra
      printf "%-28s %-14s %5s %5s %5s %5s %6s %6s %s/%s\n" "mode $1 qam $2" "$extra" "$(stat time)" "$(stat frame)" "$(stat fac)" "$(stat msc)" "$(stat msc_flaps)" "$(stat snr)" $RC "$(stat images)"
    done
  done
  exit 0
fi

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
pct() { echo "${1%\%}"; }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

for p in "B 4" "B 16" "B 64" "A 16" "E 16"; do
  set -- $p
  name="mode $1, ${2}-QAM"
  echo "== $name"
  encode "m$1$2" --drm-mode $1 --drm-qam $2 || { bad "$name: encode"; continue; }
  decode "$T/m$1$2.wav"
  [ "$RC" = 0 ] && ok "$name: exit code 0 (picture received)" || bad "$name: exit code $RC (picture not received)"
  ls "$T/o"/*_drm.* >/dev/null 2>&1 && ok "$name: file written" || bad "$name: no file written"
  grep -q "DRM, 160x124" "$T/out.txt" 2>/dev/null && ok "$name: picture size" || bad "$name: picture size ($(grep -o '(DRM[^)]*)' "$T/out.txt"))"
  # a clean loopback; the average includes the receiver's lock-in, so this is a floor with margin (measured 35 dB)
  ge "$(stat snr)" 30 && ok "$name: SNR $(stat snr) dB" || bad "$name: SNR $(stat snr) dB, want >= 30"
  # equalised MSC cell amplitude must be flat across the carriers: a wrong boosted-pilot table made the lowest carriers
  # of mode A 39% too large (measured 0.04-0.10 when right)
  ge 0.20 "$(stat amp_dev)" && ok "$name: MSC amplitude flat across carriers (max deviation $(stat amp_dev))" || bad "$name: MSC amplitude varies across carriers (max deviation $(stat amp_dev), want <= 0.20)"
  [ "$(stat msc_flaps)" = 0 ] && ok "$name: no MSC dropouts" || bad "$name: MSC dropped out $(stat msc_flaps) times"
done

# level and noise: the receiver must not depend on the input level, and must cope with a noisy channel
echo "== 16-QAM through a bad channel"
for extra in "--rx-gain -40" "--rx-gain 6" "--rx-noise -35"; do
  decode "$T/mB16.wav" $extra
  [ "$RC" = 0 ] && ok "$extra: picture received (SNR $(stat snr) dB)" || bad "$extra: exit code $RC"
done
echo; echo "$pass passed, $fail failed"
[ "$fail" = 0 ]
