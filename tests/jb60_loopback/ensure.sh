#!/bin/sh
# Builds ./loopback when it is missing or older than a source it is compiled from (a stale harness tests an old transmitter: it hid a
# failing PD check for days). Used by the tests that run the harness.
HERE=$(cd "$(dirname "$0")" && pwd)
LB="$HERE/loopback"
if [ -x "$LB" ] && [ -z "$(find "$HERE/loopback.cpp" "$HERE/metrics.h" "$HERE/build.sh" "$HERE/stubs" "$HERE/../../src/sstv" "$HERE/../../src/dsp" "$HERE/../../src/utils" "$HERE/../../src/config" \
      -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.sh' \) -newer "$LB" 2>/dev/null | head -1)" ]; then
  exit 0
fi
echo "building the loopback harness (missing or older than its sources)" >&2
"$HERE/build.sh"
