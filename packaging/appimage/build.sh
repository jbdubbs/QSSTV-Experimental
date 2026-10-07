#!/bin/bash
# Build the glibc-2.39-floor, fully self-contained AppImage in an Ubuntu 24.04 container.
#   packaging/appimage/build.sh            # VERSION=10.0-Pre4 by default; output lands in the repo root
# Expects sibling checkouts ../hamlib-upstream-sstv (as the cmake build does).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PARENT=$(dirname "$HERE")
VERSION=${VERSION:-10.0-Pre4}
OUT_DIR=${OUT_DIR:-$HERE}
IMAGE=qsstv-appimage-builder

docker build -t "$IMAGE" "$HERE/packaging/appimage"
docker run --rm \
  -v "$HERE":/src/qsstv-experimental:ro \
  -v "$PARENT/hamlib-upstream-sstv":/src/hamlib-upstream-sstv:ro \
  -v "$OUT_DIR":/out \
  -e VERSION="$VERSION" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  "$IMAGE" bash /src/qsstv-experimental/packaging/appimage/inside.sh
echo "Built: $OUT_DIR/QSSTV-Experimental-$VERSION-x86_64.AppImage"
