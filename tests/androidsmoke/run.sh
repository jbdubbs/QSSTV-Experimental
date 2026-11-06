#!/bin/bash
# Android smoke test on an emulator or a phone that adb can reach: the app installs and starts, opens its audio, takes a recording
# from another app (the VIEW intent of a file manager's "Open with"), decodes it and puts the picture into the phone's picture
# library, and does not crash meanwhile. What it does not cover: the real microphone and speaker, USB adapters, GPS, the camera.
#   tests/androidsmoke/run.sh <apk> <recording.wav> [out dir for the logcat and screenshots]
# The apk is the x86_64 debug-signed one for an emulator (CI), or the arm64 one for a phone. The recording is any SSTV wav the program
# decodes (tests/jb60_loopback/loopback jb --image card --vis --wav x.wav writes one). Needs adb (ADB=... overrides) and a running device.
set -u
[ $# -ge 2 ] || { echo "usage: $0 <apk> <recording.wav> [out dir]" >&2; exit 2; }
APK=$1; WAV=$2; OUT=${3:-$(mktemp -d)}
ADB=${ADB:-adb}
PKG=io.github.jbdubbs.qsstv_experimental
ACT=org.qtproject.qt.android.bindings.QtActivity
mkdir -p "$OUT"
fails=0
ok()  { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails+1)); }
wait_for() {   # seconds, description, command...: runs the command until it succeeds
  local limit=$1 what=$2; shift 2
  local i=0
  while [ $i -lt "$limit" ]; do
    if "$@" >/dev/null 2>&1; then ok "$what (${i}s)"; return 0; fi
    sleep 2; i=$((i+2))
  done
  bad "$what: not within ${limit}s"; return 1
}
log_has()  { "$ADB" logcat -d -s qsstv-audio qsstv-files 2>/dev/null | grep -q "$1"; }
library_has() { "$ADB" shell content query --uri content://media/external/images/media --projection _display_name:relative_path 2>/dev/null | grep -q "QSSTV-Experimental"; }
alive()    { [ -n "$("$ADB" shell pidof "$PKG" 2>/dev/null | tr -d '\r')" ]; }

echo "== device"
"$ADB" wait-for-device
wait_for 240 "the device finished booting" bash -c "[ \"\$($ADB shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')\" = 1 ]" || exit 1
echo "   $("$ADB" shell getprop ro.product.model | tr -d '\r'), Android $("$ADB" shell getprop ro.build.version.release | tr -d '\r') (API $("$ADB" shell getprop ro.build.version.sdk | tr -d '\r')), $("$ADB" shell getprop ro.product.cpu.abi | tr -d '\r')"

echo "== install and start"
"$ADB" uninstall "$PKG" >/dev/null 2>&1
"$ADB" install -r "$APK" 2>&1 | tail -1 | grep -q Success && ok "installed" || { bad "install"; exit 1; }
# the permission boxes are not what is tested here
"$ADB" shell pm grant "$PKG" android.permission.RECORD_AUDIO
"$ADB" shell pm grant "$PKG" android.permission.POST_NOTIFICATIONS 2>/dev/null
"$ADB" logcat -c
"$ADB" shell am start -n "$PKG/$ACT" >/dev/null
wait_for 90 "the program started" alive
wait_for 90 "the audio was opened" log_has "opened:"
sleep 5
alive && ok "still running after the start" || bad "the program died at the start"

echo "== decode a recording opened from another app"
"$ADB" push "$WAV" /data/local/tmp/smoke.wav >/dev/null && "$ADB" shell chmod 644 /data/local/tmp/smoke.wav
"$ADB" shell am start -a android.intent.action.VIEW -t audio/wav -d file:///data/local/tmp/smoke.wav -n "$PKG/$ACT" >/dev/null
wait_for 240 "the picture is in the picture library (Pictures/QSSTV-Experimental)" library_has
"$ADB" exec-out screencap -p > "$OUT/smoke.png" 2>/dev/null
sleep 3; "$ADB" exec-out screencap -p > "$OUT/smoke-b.png" 2>/dev/null   # a second and third look: a half-drawn frame is not what the screen shows later
sleep 3; "$ADB" exec-out screencap -p > "$OUT/smoke-c.png" 2>/dev/null
alive && ok "still running after the decode" || bad "the program died during the decode"
# Back on the first screen puts the program in the background and keeps it (and the receiver) running
"$ADB" shell input keyevent KEYCODE_BACK
sleep 3
alive && ok "still running after Back" || bad "the program died on Back"

echo "== log"
"$ADB" logcat -d > "$OUT/logcat.txt" 2>&1
if grep -E "FATAL EXCEPTION|Fatal signal|SIGSEGV|SIGABRT" "$OUT/logcat.txt" | grep -v "^---" | head -3 | grep .; then bad "a crash is in the log (above)"; else ok "no crash in the log"; fi
echo "   logcat and a screenshot are in $OUT"
[ $fails -eq 0 ] && echo "androidsmoke: all checks passed" || { echo "androidsmoke: $fails checks failed"; exit 1; }
