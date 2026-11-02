#!/bin/bash
# The GPS inside the real application objects, offscreen, with a fake receiver on a pseudo terminal: the GPS button (one switch,
# the boxes follow the receiver), the %a / %b macros, Start and the beacon that never wait, persistence and Configuration > GPS
# (see app.cpp).
# Links the object files of an existing build: tests/gps/app.sh [build-dir]   (default: build-cmake)
HERE=$(cd "$(dirname "$0")" && pwd)
APP_CPP="$HERE/app.cpp" exec "$HERE/../editor/app.sh" "$@"
