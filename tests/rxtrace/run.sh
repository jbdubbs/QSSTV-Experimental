#!/bin/bash
# Noise on the band must not become pictures, and must not hold up the receiver for a real transmission (issue #135).
#
# Half an hour of synthetic noise (noisegen.py: white noise and sync-like tone bursts, about 50 a second, High sensitivity)
# makes the receiver start pictures on nothing. Checks, with the real receiver (--batch):
#   - the noise starts at least one picture (else the rest proves nothing), none of them is kept, and the receiver is
#     not tied up by them for long (before the fix: 3 pictures kept, 245 s held, the longest 89 s)
#   - a real Robot 36 transmission whose VIS code was missed (it can only be found by its sync pulses) that starts 15 s after
#     the receiver started on noise is received completely (before the fix it was still waiting for sync from the noise)
# With RXTRACE_WAV=<recording from capture.sh> a real recording is checked as well: nothing kept, held up for under 10%.
#
#   tests/rxtrace/run.sh                           # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/rxtrace/run.sh
#   RXTRACE_WAV=~/projects/qsstv-captures/2026-10-27-hf/hour1/hf.wav tests/rxtrace/run.sh
#
# Needs: the built qsstv-experimental, ffmpeg (scales the test picture), python3 with numpy and Pillow. About 2 minutes.
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
command -v ffmpeg >/dev/null || { echo "ffmpeg is needed to scale the test picture"; exit 2; }
REAL_HOME=$HOME
py() { HOME=$REAL_HOME python3 "$@"; }          # the app below runs with a throw-away HOME; python keeps yours
python3 -c "import numpy, PIL" 2>/dev/null || { echo "python3 with numpy and Pillow is needed"; exit 2; }

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME/.config/qsstv-experimental"
# Sensitivity High (2), Save if Complete 25 (the default): the setting on which noise got furthest
printf '[RX]\nsensitivity=2\nminCompletion=25\n' > "$HOME/.config/qsstv-experimental/qsstv-experimental.conf"
TMO=$(command -v timeout || command -v gtimeout)
# decode FILE into DIR with a trace in DIR/trace.tsv
dec() { mkdir -p "$2"; QSSTV_RX_TRACE="$2/trace.tsv" ${TMO:+$TMO 900} "$QSSTV" --batch -o "$2" "$1" 2>"$T/stderr" >/dev/null; }
brief() { python3 -I "$HERE/summarize.py" "$1/trace.tsv" --brief; }      # started saved busy-seconds longest-seconds
npng() { ls "$1"/*.png 2>/dev/null | wc -l | tr -d ' '; }
le()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a<=b)}'; }
ge()  { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }

echo "== synthetic noise"
py "$HERE/noisegen.py" "$T/noise.wav" --minutes 30 --rate 12000 --sigma 2400 --bursts-per-s 50 --burst-gain 2 --seed 1 || { echo "cannot make the noise"; exit 2; }
dec "$T/noise.wav" "$T/n"
read -r started saved busy longest < <(brief "$T/n")
[ "$started" -ge 1 ] && ok "the noise starts pictures ($started)" || bad "the noise starts no picture, the checks below prove nothing"
[ "$saved" = 0 ] && ok "none of them is kept" || bad "$saved pictures of noise kept"
le "$busy" 100 && ok "held up for $busy s in all" || bad "held up for $busy s in all (limit 100)"
le "$longest" 30 && ok "the longest wait for sync is $longest s" || bad "the longest wait for sync is $longest s (limit 30)"

echo "== a real transmission without its VIS code, 15 s after the receiver started on noise"
ffmpeg -loglevel error -y -i "$HERE/../jb60_loopback/images/card.png" -vf "scale=320:240:flags=lanczos" "$T/R36.png" || { echo "ffmpeg failed"; exit 2; }
"$QSSTV" --encode "$T/R36.png" --mode R36 --wav-out "$T/R36.wav" >/dev/null 2>&1 || { echo "cannot transmit the test picture"; exit 2; }
mkdir -p "$T/clean"; dec "$T/R36.wav" "$T/clean"
[ "$(npng "$T/clean")" = 1 ] || { echo "  FAIL the unmixed transmission does not decode to one picture"; exit 1; }
py "$HERE/../fade/wavtool.py" cut "$T/R36.wav" "$T/R36cut.wav" --start 2.05
args=""; for t in $(python3 -I "$HERE/summarize.py" "$T/n/trace.tsv" --windows 15); do args="$args --at $t:$T/R36cut.wav"; done
n=$(echo $args | grep -o -- "--at" | wc -l)
py "$HERE/inject.py" "$T/noise.wav" "$T/mixed.wav" $args > /dev/null || { echo "cannot mix the transmissions in"; exit 2; }
dec "$T/mixed.wav" "$T/m"
got=0; late=0
for p in "$T"/m/*_R36.png; do
  [ -f "$p" ] || continue
  r=$(py "$HERE/../fade/wavtool.py" rows "$T/clean"/*.png "$p" 0.2 0.2 --rows 40)
  bot=$(echo "$r" | sed 's/.*bottom \([0-9.]*\).*/\1/')
  lost=$(echo "$r" | sed -n 's/^top [0-9.]* (moved [-0-9]* px, \([-0-9]*\) rows).*/\1/p')
  ge "$bot" 0.9 || continue
  got=$((got+1))
  # the cut header costs 3 rows; a few more would be a late start, tens of rows a receiver that was still busy
  [ -n "$lost" ] && [ "${lost#-}" -gt 8 ] && late=$((late+1))
done
[ "$got" = "$n" ] && ok "all $n transmissions received" || bad "$got of $n transmissions received"
[ "$late" = 0 ] && ok "none of them started late" || bad "$late of them started late (more than 8 rows missing at the top)"

if [ -n "$RXTRACE_WAV" ]; then
  echo "== recording $RXTRACE_WAV"
  dec "$RXTRACE_WAV" "$T/r"
  read -r started saved busy longest < <(brief "$T/r")
  secs=$(py -c "import sys; sys.dont_write_bytecode=True; sys.path.insert(0,'$HERE/../fade'); import wavtool; x,r=wavtool.read_wav('$RXTRACE_WAV'); print('%.0f' % (len(x)/float(r)))")
  [ "$saved" = 0 ] && ok "$started pictures started on it, none kept" || bad "$saved of $started pictures kept"
  awk -v b="$busy" -v s="$secs" 'BEGIN{exit !(b<=0.10*s)}' && ok "held up for $busy s of $secs s" || bad "held up for $busy s of $secs s (limit 10%)"
fi

echo
echo "$pass passed, $fail failed"
[ $fail = 0 ]
