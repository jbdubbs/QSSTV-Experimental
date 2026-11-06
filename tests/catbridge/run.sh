#!/bin/bash
# The Android CAT bridge (packaging/android/.../CatBridge.java): TCP port <-> serial line, tested on the desktop JDK with a fake radio.
#   tests/catbridge/run.sh      (needs javac and java, any JDK from 11 on)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../../packaging/android/src/io/github/jbdubbs/qsstv_experimental
OUT=$(mktemp -d)
trap 'rm -rf "${OUT:?}"' EXIT
JAVAC=${JAVA_HOME:+$JAVA_HOME/bin/}javac; JAVA=${JAVA_HOME:+$JAVA_HOME/bin/}java
"$JAVAC" -d "$OUT" "$SRC/SerialLink.java" "$SRC/CatBridge.java" "$HERE/CatBridgeTest.java"
"$JAVA" -cp "$OUT" CatBridgeTest
