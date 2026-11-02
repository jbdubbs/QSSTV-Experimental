#!/bin/bash
# The DRM picture encode (AVIF or JPEG2000) runs in a worker thread, so switching the codec or moving the size slider does not freeze the
# GUI (#164): the REAL application objects, offscreen, with a throw-away HOME and temp directory; see app.cpp.
# Links the object files of an existing build: tests/drmasync/app.sh [build-dir]   (default: build-cmake)
HERE=$(cd "$(dirname "$0")" && pwd)
APP_CPP="$HERE/app.cpp" exec "$HERE/../editor/app.sh" "$@"
