#!/bin/bash
# The live camera picture of WA8LMF Beacon Mode (#150) in the REAL application objects, offscreen, without a camera: frames are fed to the
# TX tab by hand (see app.cpp).
# Links the object files of an existing build: tests/beacon/app.sh [build-dir]   (default: build-cmake)
HERE=$(cd "$(dirname "$0")" && pwd)
APP_CPP="$HERE/app.cpp" exec "$HERE/../editor/app.sh" "$@"
