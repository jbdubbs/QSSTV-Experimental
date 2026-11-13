#!/bin/bash
# The whole Android build on this computer, in place of the GitHub workflow that used to do it: builds the app for the phone (arm64-v8a)
# and for the emulator (x86_64), runs the checks (16 KB page alignment, only the Material and Basic Quick Controls styles, the CAT bridge),
# signs the APKs and runs the smoke test (tests/androidsmoke/run.sh) on an emulator, which it starts when no device is connected.
#
#   tools/android-release.sh [-b BUILD] [-o OUT_DIR] [--abi arm64-v8a|x86_64] [--aab] [--no-smoke]
#
#   -b BUILD     build number 0-99 of this program version, the last two digits of the versionCode (packaging/android/PLAY.md, section 3); default 0
#   -o OUT_DIR   where the signed packages go; default ~/projects/qsstv-release-<version>/android
#   --abi        build one ABI only (default: both)
#   --aab        also build the Android App Bundle of arm64-v8a (Google Play); signed when ANDROID_UPLOAD_KEYSTORE, ANDROID_UPLOAD_ALIAS and
#                ANDROID_UPLOAD_PASS_FILE are set, else left unsigned
#   --no-smoke   skip the emulator test
#
# Needs the native libraries (tools/build-android-deps.sh, third_party/android-deps/<abi>), Qt 6.11.2 for Android, the SDK with NDK r27c, JDK 21
# and for the smoke test the AVD below (BUILDING.md). The APKs are signed with ~/.android/qsstv-sideload.jks (password in the file next to it),
# the key of the APKs on GitHub: a phone can only update an app that was signed with the same key. Without it the debug key is used.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

BUILD=0; OUT=""; ABIS="arm64-v8a x86_64"; AAB=0; SMOKE=1
while [ $# -gt 0 ]; do
  case "$1" in
    -b) BUILD=$2; shift 2 ;;
    -o) OUT=$2; shift 2 ;;
    --abi) ABIS=$2; shift 2 ;;
    --aab) AAB=1; shift ;;
    --no-smoke) SMOKE=0; shift ;;
    *) sed -n 2,17p "$0"; exit 2 ;;
  esac
done
[[ "$BUILD" =~ ^[0-9]+$ ]] && [ "$BUILD" -le 99 ] || { echo "the build number is 0 to 99" >&2; exit 2; }

VERSION=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\(.*\)")/\1/p' src/CMakeLists.txt)
OUT=${OUT:-$HOME/projects/qsstv-release-$VERSION/android}
QT_VERSION=${QT_VERSION:-6.11.2}
SDK=${ANDROID_HOME:-$HOME/Android/Sdk}
NDK=${ANDROID_NDK_ROOT:-$SDK/ndk/27.2.12479018}
BT=$SDK/build-tools/36.0.0
export JAVA_HOME=${JAVA_HOME:-$(ls -d "$HOME"/Android/jdk/jdk-21* | head -1)}
export PATH=$JAVA_HOME/bin:$SDK/platform-tools:$PATH
AVD=${AVD:-qsstv_api36}
mkdir -p "$OUT"
say() { printf '\n== %s\n' "$*"; }

if [ -f "$HOME/.android/qsstv-sideload.jks" ]; then
  KS=$HOME/.android/qsstv-sideload.jks; KS_ALIAS=sideload; KS_PASS=$HOME/.android/qsstv-sideload.pass
else
  echo "(no ~/.android/qsstv-sideload.jks: signing with the debug key)"
  KS=$HOME/.android/debug.keystore; KS_ALIAS=androiddebugkey; KS_PASS=""
fi
sign_args() { if [ -n "$KS_PASS" ]; then echo "--ks-pass file:$KS_PASS"; else echo "--ks-pass pass:android"; fi; }

declare -A QTABI=([arm64-v8a]=android_arm64_v8a [x86_64]=android_x86_64)
declare -A DIR=([arm64-v8a]=build-android-arm64 [x86_64]=build-android-x86_64)
declare -A SHORT=([arm64-v8a]=arm64 [x86_64]=x86_64)

for abi in $ABIS; do
  B=${DIR[$abi]}
  DEPS=$ROOT/third_party/android-deps/$abi
  [ -d "$DEPS/lib/pkgconfig" ] || { echo "no native libraries for $abi: tools/build-android-deps.sh" >&2; exit 1; }
  say "$VERSION build $BUILD, $abi: configure and build"
  PKG_CONFIG_LIBDIR=$DEPS/lib/pkgconfig "$HOME/Qt/$QT_VERSION/${QTABI[$abi]}/bin/qt-cmake" -S src -B "$B" -G Ninja \
    -DQT_HOST_PATH="$HOME/Qt/$QT_VERSION/gcc_64" -DANDROID_SDK_ROOT="$SDK" -DANDROID_NDK_ROOT="$NDK" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHAMLIB_ROOT="$DEPS" -DCMAKE_FIND_ROOT_PATH="$DEPS" -DQSSTV_ANDROID_BUILD="$BUILD" > "$OUT/configure-$abi.log" 2>&1
  # androiddeployqt copies into android-build/ and never removes what an older build left there (libraries of the Quick Controls styles the
  # program no longer imports, for one): the package must be made from nothing, as on a clean checkout
  rm -rf "$B/android-build"
  cmake --build "$B" > "$OUT/build-$abi.log" 2>&1 || { tail -20 "$OUT/build-$abi.log"; echo "the $abi build failed (log: $OUT/build-$abi.log)" >&2; exit 1; }
  if [ "$AAB" = 1 ] && [ "$abi" = arm64-v8a ]; then
    cmake --build "$B" --target aab >> "$OUT/build-$abi.log" 2>&1 || { tail -20 "$OUT/build-$abi.log"; echo "the app bundle failed" >&2; exit 1; }
  fi

  UNSIGNED=$ROOT/$B/android-build/build/outputs/apk/release/android-build-release-unsigned.apk
  say "$abi: checks"
  # Play rejects an upload with a library that is not aligned for 16 KB pages: every LOAD segment of every .so in the package
  LIBS=$(mktemp -d); trap 'rm -rf "$LIBS"' EXIT
  unzip -q "$UNSIGNED" 'lib/*' -d "$LIBS"
  READELF=$(ls "$NDK"/toolchains/llvm/prebuilt/*/bin/llvm-readelf)
  bad=0; n=0
  for f in "$LIBS"/lib/"$abi"/*.so; do
    n=$((n+1))
    a=$("$READELF" -lW "$f" | awk '$1=="LOAD"{print $NF}' | sort -u | tr '\n' ' ')
    case "$a" in "0x4000 ") ;; *) echo "NOT 16 KB aligned: $(basename "$f") ($a)"; bad=1 ;; esac
  done
  [ "$n" -gt 0 ] && [ "$bad" = 0 ] || { echo "16 KB alignment check FAILED" >&2; exit 1; }
  echo "  ok   $n libraries are 16 KB aligned"
  # a plain "import QtQuick.Controls" in any QML file makes androiddeployqt pack all five Controls styles (about 10 MB)
  if unzip -l "$UNSIGNED" | grep -E 'Controls2(Fusion|Imagine|Universal|FluentWinUI3)|Controls_(Fusion|Imagine|Universal|FluentWinUI3)'; then
    echo "an unused Quick Controls style is in the package: find the QML file that says 'import QtQuick.Controls' without .Material" >&2; exit 1
  fi
  echo "  ok   only the Material and Basic styles are packed"

  say "$abi: align and sign"
  NAME=QSSTV-Experimental-$VERSION-android-${SHORT[$abi]}
  rm -f "$OUT/$NAME.apk" "$OUT/$NAME.apk.idsig"
  # -P 16: the stored native libraries start on a 16 KB boundary in the zip as well
  "$BT/zipalign" -f -P 16 4 "$UNSIGNED" "$OUT/aligned-$abi.apk"
  # shellcheck disable=SC2046
  "$BT/apksigner" sign --ks "$KS" $(sign_args) --ks-key-alias "$KS_ALIAS" --v4-signing-enabled false --out "$OUT/$NAME.apk" "$OUT/aligned-$abi.apk"
  rm -f "$OUT/aligned-$abi.apk"
  "$BT/apksigner" verify --print-certs "$OUT/$NAME.apk" | sed -n 2p
  "$BT/zipalign" -c -P 16 4 "$OUT/$NAME.apk" && echo "  ok   zip aligned"
  "$BT/aapt2" dump badging "$OUT/$NAME.apk" | sed -n 1p | sed 's/ platformBuild.*//'
  if [ "$AAB" = 1 ] && [ "$abi" = arm64-v8a ]; then
    AABF=$ROOT/$B/android-build/build/outputs/bundle/release/android-build-release.aab
    if [ -n "${ANDROID_UPLOAD_KEYSTORE:-}" ]; then
      cp "$AABF" "$OUT/$NAME.aab"
      jarsigner -keystore "$ANDROID_UPLOAD_KEYSTORE" -storepass:file "$ANDROID_UPLOAD_PASS_FILE" -keypass:file "$ANDROID_UPLOAD_PASS_FILE" \
        -sigalg SHA256withRSA -digestalg SHA-256 "$OUT/$NAME.aab" "$ANDROID_UPLOAD_ALIAS" > /dev/null
      jarsigner -verify "$OUT/$NAME.aab" | head -2
    else
      cp "$AABF" "$OUT/$NAME-unsigned.aab"; echo "  (the bundle is not signed: no ANDROID_UPLOAD_KEYSTORE)"
    fi
  fi
done

say "CAT bridge test (Java)"
bash tests/catbridge/run.sh

if [ "$SMOKE" = 1 ] && [[ " $ABIS " == *" x86_64 "* ]]; then
  say "smoke test on an emulator"
  WAV=$OUT/smoke.wav
  (cd tests/jb60_loopback && ./build.sh > /dev/null)
  QT_QPA_PLATFORM=offscreen tests/jb60_loopback/loopback jb --image card --vis --wav "$WAV" > /dev/null
  STARTED=""
  if ! adb devices | awk 'NR>1 && $2=="device"' | grep -q .; then
    echo "starting the emulator $AVD"
    # -gpu host: on a Linux host with SELinux enforcing the software renderer crashes the emulator (BUILDING.md)
    "$SDK/emulator/emulator" -avd "$AVD" -no-window -gpu host -no-snapshot -noaudio -no-boot-anim > "$OUT/emulator.log" 2>&1 &
    STARTED=$!
  fi
  cleanup() { if [ -n "$STARTED" ]; then adb emu kill > /dev/null 2>&1 || kill "$STARTED" 2>/dev/null || true; fi; }
  trap cleanup EXIT
  SMOKE_OK=0
  bash tests/androidsmoke/run.sh "$OUT/QSSTV-Experimental-$VERSION-android-x86_64.apk" "$WAV" "$OUT/smoke-out" | tee "$OUT/smoke.log" || SMOKE_OK=1
  grep -q "FAIL" "$OUT/smoke.log" && SMOKE_OK=1
  [ "$SMOKE_OK" = 0 ] || { echo "THE SMOKE TEST FAILED (see $OUT/smoke.log and $OUT/smoke-out)" >&2; exit 1; }
fi

say "done"
ls -la "$OUT"/*.apk "$OUT"/*.aab 2>/dev/null | awk '{print $5, $9}' || true
