#!/bin/bash
# Build the glibc-2.39-floor, fully self-contained AppImage in an Ubuntu 24.04 container.
#   packaging/appimage/build.sh            # VERSION=10.0-Pre8 by default; output lands in the repo root
# Expects sibling checkouts ../hamlib-upstream-sstv (as the cmake build does).
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
PARENT=$(dirname "$HERE")
VERSION=${VERSION:-10.0-Pre8}
OUT_DIR=${OUT_DIR:-$HERE}
# ARCH=x86_64 (default) or aarch64/arm64; a foreign arch builds under QEMU user emulation (slow)
ARCH=${ARCH:-x86_64}
case "$ARCH" in
  x86_64|amd64)   ARCH=x86_64;  PLATFORM=linux/amd64; BARGS=() ;;
  aarch64|arm64)  ARCH=aarch64; PLATFORM=linux/arm64; BARGS=(--build-arg AQT_HOST=linux_arm64 --build-arg QT_ARCH=linux_gcc_arm64 --build-arg LD_ARCH=aarch64 --build-arg "AQT_EXTRA=--archives qtbase qtdeclarative qtsvg qtwayland qttranslations icu") ;;
  *) echo "unsupported ARCH=$ARCH" >&2; exit 1 ;;
esac
IMAGE=qsstv-appimage-builder-$ARCH

docker build --platform "$PLATFORM" "${BARGS[@]}" -t "$IMAGE" "$HERE/packaging/appimage"
docker run --rm --platform "$PLATFORM" \
  -v "$HERE":/src/qsstv-experimental:ro \
  -v "$PARENT/hamlib-upstream-sstv":/src/hamlib-upstream-sstv:ro \
  -v "$OUT_DIR":/out \
  -e VERSION="$VERSION" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
  "$IMAGE" bash /src/qsstv-experimental/packaging/appimage/inside.sh
echo "Built: $OUT_DIR/QSSTV-Experimental-$VERSION-$ARCH.AppImage"
