#!/bin/bash
# Build the Windows NSIS installer from an already-built build-mingw64/ tree.
# Fedora's mingw64-nsis ships only 64-bit stubs, but makensis still looks for the 32-bit ones at
# start-up (even for -VERSION), so we point NSISDIR at a copy with the 64-bit stubs aliased. The
# installer script itself selects "Target amd64-unicode" (src/CMakeLists.txt), so the aliases are
# never used to build anything.
set -euo pipefail
cd "$(dirname "$0")/.."
NSIS_COPY="$(mktemp -d)"
trap 'rm -rf "$NSIS_COPY"' EXIT
cp -r /usr/share/nsis/. "$NSIS_COPY"
for f in "$NSIS_COPY"/Stubs/*-amd64-unicode; do
  [ -e "${f/amd64/x86}" ] || cp "$f" "${f/amd64/x86}"
done
cd build-mingw64
NSISDIR="$NSIS_COPY" cpack -G NSIS
