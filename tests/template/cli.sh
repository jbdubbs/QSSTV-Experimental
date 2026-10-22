#!/bin/bash
# End-to-end test of the template command line of the real application:
#   qsstv-experimental --convert-template / --render-template
# The legacy .templ fixture is rendered through the CLI and compared with the golden images made by the old editor
# (tests/template/golden), directly and after a round trip through the native .qtempl format. Error handling and exit
# codes are checked too. The application runs with a throw-away HOME, so your settings and templates are not touched.
#
#   tests/template/cli.sh                          # uses ../../build-cmake/qsstv-experimental
#   QSSTV_BIN=/path/to/qsstv-experimental tests/template/cli.sh
#
# Needs: the built application and g++/Qt6 headers (for the small image comparison tool).
HERE=$(cd "$(dirname "$0")" && pwd)
QSSTV=${QSSTV_BIN:-$HERE/../../build-cmake/qsstv-experimental}
[ -x "$QSSTV" ] || { echo "qsstv-experimental not found at $QSSTV (set QSSTV_BIN)"; exit 2; }
QT=${QT_INCLUDE:-/usr/include/qt6}
FX=$HERE/fixtures; GD=$HERE/golden

T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
export HOME=$T/home XDG_CONFIG_HOME=$T/home/.config XDG_DATA_HOME=$T/home/.local/share QT_QPA_PLATFORM=offscreen
mkdir -p "$HOME" "$XDG_DATA_HOME/fonts"
cp "$FX/Montserrat-Bold.otf" "$XDG_DATA_HOME/fonts/"      # the fixture template uses it; fontconfig finds it here
g++ -O1 -fPIC -std=gnu++17 -w -isystem "$QT" -isystem "$QT/QtCore" -isystem "$QT/QtGui" -o "$T/psnr" "$HERE/psnr.cpp" -lQt6Gui -lQt6Core || { echo "cannot build the comparison tool"; exit 2; }

pass=0; fail=0
ok()  { echo "  ok   $1"; pass=$((pass+1)); }
bad() { echo "  FAIL $1"; fail=$((fail+1)); }
expect_exit() { [ "$2" = "$3" ] && ok "$1 (exit $3)" || bad "$1: exit $3, expected $2"; }
ge() { awk -v a="$1" -v b="$2" 'BEGIN{exit !(a>=b)}'; }
VARS="--template-var c=W1AW --template-var r=599 --template-var m=N0CALL --template-var t=12:34 --template-var d=2026/10/22"
cd "$T"

echo "== --convert-template"
"$QSSTV" --convert-template "$FX/synthetic.templ" --template-out syn.qtempl >/dev/null 2>&1; expect_exit "convert legacy to native" 0 $?
head -c 1 syn.qtempl | grep -q '{' && ok "native file is JSON" || bad "native file is not JSON"
cp "$FX/synthetic.templ" copy.templ
"$QSSTV" --convert-template copy.templ >/dev/null 2>&1; expect_exit "convert with default output name" 0 $?
[ -f copy.qtempl ] && ok "default output is next to the original" || bad "copy.qtempl missing"
cmp -s "$FX/synthetic.templ" copy.templ && ok "the original is untouched" || bad "the original changed"
"$QSSTV" --convert-template syn.qtempl --template-out again.qtempl >/dev/null 2>&1; expect_exit "convert native to native" 0 $?
cmp -s syn.qtempl again.qtempl && ok "native conversion is stable" || bad "native conversion changed the file"

echo "== --render-template against the golden images made by the legacy editor"
for src in "$FX/synthetic.templ" syn.qtempl; do
  name=$(basename "$src")
  for sz in 640x496 320x256 1024x768; do
    "$QSSTV" --render-template "$src" --template-base "$GD/base_$sz.png" --template-replay "$FX/replay.png" $VARS --template-out out.png >/dev/null 2>&1
    expect_exit "render $name at $sz" 0 $?
    p=$("$T/psnr" out.png "$GD/synthetic_${sz}_replay.png")
    ge "$p" 45 && ok "$name at $sz matches the legacy rendering ($p dB)" || bad "$name at $sz differs from the legacy rendering ($p dB)"
  done
done
"$QSSTV" --render-template syn.qtempl --template-base "$GD/base_640x496.png" $VARS --template-out out.png >/dev/null 2>&1
p=$("$T/psnr" out.png "$GD/synthetic_640x496_noreplay.png")
ge "$p" 45 && ok "no replay picture gives the hatched placeholder ($p dB)" || bad "placeholder rendering differs ($p dB)"
"$QSSTV" --render-template syn.qtempl --template-size 200x100 >/dev/null 2>&1; expect_exit "render without a base picture" 0 $?
[ -f syn.qtempl.png ] && ok "default output is <file>.png" || bad "syn.qtempl.png missing"

echo "== errors"
"$QSSTV" --render-template /etc/hostname >/dev/null 2>&1; expect_exit "not a template" 2 $?
"$QSSTV" --render-template "$T/nonexistent.templ" >/dev/null 2>&1; expect_exit "missing file" 2 $?
head -c 300 "$FX/synthetic.templ" > cut.templ
"$QSSTV" --render-template cut.templ >/dev/null 2>&1; expect_exit "truncated template" 2 $?
"$QSSTV" --render-template syn.qtempl --template-size 99999x5 >/dev/null 2>&1; expect_exit "absurd size" 2 $?
"$QSSTV" --render-template syn.qtempl --template-size abc >/dev/null 2>&1; expect_exit "malformed size" 2 $?
"$QSSTV" --render-template syn.qtempl --template-var cc=1 >/dev/null 2>&1; expect_exit "malformed macro" 2 $?
"$QSSTV" --render-template syn.qtempl --template-base "$T/nope.png" >/dev/null 2>&1; expect_exit "missing base picture" 2 $?
"$QSSTV" --render-template syn.qtempl --template-out /nonexistent/dir/o.png >/dev/null 2>&1; expect_exit "unwritable output" 1 $?
"$QSSTV" --help 2>&1 | grep -q -- "--render-template" && ok "--help lists the options" || bad "--help does not list --render-template"

echo; echo "passed $pass, failed $fail"
[ $fail -eq 0 ]
