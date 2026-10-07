#!/bin/bash
# Build a self-contained Linux AppImage for QSSTV-Experimental.
#
# Why this script exists and why it's not just "run linuxdeploy": a first attempt at this
# (2026-09-30) cost a multi-hour debugging session to a bug that looked, in turn, like a
# missing offscreen platform plugin, a bad IBus input-method plugin, and a fragile X11/xcb
# library bundle -- before the real cause turned up. See the full postmortem in Claude's
# project memory (qsstv_10_pre1_appimage.md) if this ever needs re-debugging; the short
# version, baked into this script so it never has to be re-derived:
#
#   linuxdeploy's own bundled `patchelf` (as of its "continuous" release) is old enough
#   that it corrupts RELR-relocated shared libraries when it rewrites their RPATH. RELR is
#   a glibc/binutils relocation-compression optimization that a modern toolchain (this was
#   found on Fedora 44 / GCC 16) enables by default -- so *every* library linuxdeploy
#   touches, including Qt6 itself, comes out silently broken: it loads, its constructor
#   even starts running, then it segfaults. The app would crash on startup under every Qt
#   platform backend (xcb against a real display, offscreen, no override at all), with the
#   crash *site* (per LD_DEBUG=libs) seeming to move between unrelated libraries from one
#   test to the next -- a classic symptom of "every patched file is a little bit broken",
#   not one single bad file. Chasing individual "bad" libraries (IBus's input-context
#   plugin looked especially guilty in isolation) was a dead end: once a *modern* patchelf
#   is used instead, nothing needs to be excluded at all.
#
# The fix is the $PATCHELF env var below, pointing linuxdeploy at a freshly-downloaded
# modern patchelf instead of trusting its own bundled copy. Confirmed (`strings` on the
# linuxdeploy binary) that it honors this override; not documented anywhere obvious.
#
# Usage:
#   tools/build-appimage-linux.sh                       # builds from build-cmake/ at the version below
#   BUILD_DIR=build-cmake VERSION=10.1-Pre1 tools/build-appimage-linux.sh
#
# Needs: the project already built in BUILD_DIR (cmake --build), network access (to fetch
# linuxdeploy/linuxdeploy-plugin-qt/appimagetool/patchelf, cached after the first run), and
# qmake6 on PATH (part of the qt6-qtbase-devel package already needed to build the project).
#
# Re-running is safe and idempotent: downloaded tools are cached under TOOLS_DIR and reused;
# the AppDir is rebuilt from scratch each time in a disposable temp directory.

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)   # repo root
BUILD_DIR=${BUILD_DIR:-$HERE/build-cmake}
VERSION=${VERSION:-10.0-Pre5}
TOOLS_DIR=${APPIMAGE_TOOLS_CACHE:-$HOME/.cache/qsstv-appimage-tools}
OUT_DIR=${OUT_DIR:-$HERE}
PATCHELF_VERSION=${PATCHELF_VERSION:-0.19.1}

[ -x "$BUILD_DIR/qsstv" ] || { echo "no built qsstv in $BUILD_DIR -- run cmake --build first" >&2; exit 1; }

mkdir -p "$TOOLS_DIR"
WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

fetch_appimage() {
  local url=$1 out=$2
  if [ ! -x "$out" ]; then
    echo "fetching $(basename "$out")..."
    curl -fL -o "$out" "$url"
    chmod +x "$out"
  fi
}

fetch_appimage "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
  "$TOOLS_DIR/linuxdeploy.AppImage"
fetch_appimage "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "$TOOLS_DIR/linuxdeploy-plugin-qt.AppImage"
fetch_appimage "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
  "$TOOLS_DIR/appimagetool.AppImage"
# linuxdeploy finds its Qt/appimage plugins by name on PATH (linuxdeploy-plugin-<name>),
# not by absolute path -- the plain names below must exist alongside the above.
ln -sf linuxdeploy-plugin-qt.AppImage "$TOOLS_DIR/linuxdeploy-plugin-qt"
ln -sf appimagetool.AppImage "$TOOLS_DIR/appimagetool"

PATCHELF_BIN="$TOOLS_DIR/patchelf-$PATCHELF_VERSION/bin/patchelf"
if [ ! -x "$PATCHELF_BIN" ]; then
  echo "fetching modern patchelf $PATCHELF_VERSION (linuxdeploy's own bundled copy corrupts RELR relocations -- see header comment)..."
  curl -fL -o "$WORK_DIR/patchelf.tar.gz" \
    "https://github.com/NixOS/patchelf/releases/download/$PATCHELF_VERSION/patchelf-$PATCHELF_VERSION-x86_64.tar.gz"
  mkdir -p "$TOOLS_DIR/patchelf-$PATCHELF_VERSION"
  tar -xzf "$WORK_DIR/patchelf.tar.gz" -C "$TOOLS_DIR/patchelf-$PATCHELF_VERSION"
fi

APPDIR="$WORK_DIR/AppDir"
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

QMAKE_BIN=$(command -v qmake6 || command -v qmake)
OFFSCREEN_PLUGIN="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)/platforms/libqoffscreen.so"
[ -f "$OFFSCREEN_PLUGIN" ] || { echo "warning: $OFFSCREEN_PLUGIN not found -- --batch headless mode (which auto-selects the offscreen platform, see chooseHeadlessPlatform() in src/main.cpp) will not work in this AppImage" >&2; OFFSCREEN_PLUGIN=; }

(
  cd "$WORK_DIR"
  export PATH="$TOOLS_DIR:$PATH"
  export APPIMAGE_EXTRACT_AND_RUN=1
  export QMAKE="$QMAKE_BIN"
  export VERSION="$VERSION"
  export NO_STRIP=1            # linuxdeploy-plugin-qt's own bundled `strip` also chokes on RELR sections
  export PATCHELF="$PATCHELF_BIN"
  [ -n "$OFFSCREEN_PLUGIN" ] && export EXTRA_PLATFORM_PLUGINS="$OFFSCREEN_PLUGIN"

  "$TOOLS_DIR/linuxdeploy.AppImage" --appdir "$APPDIR" \
    --executable "$APPDIR/usr/bin/qsstv" \
    --desktop-file "$APPDIR/usr/share/applications/qsstv.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/128x128/apps/qsstv.png" \
    --plugin qt \
    --output appimage
)

OUT_FILE="QSSTV-Experimental-$VERSION-x86_64.AppImage"
mv "$WORK_DIR/$OUT_FILE" "$OUT_DIR/$OUT_FILE"
echo "Built: $OUT_DIR/$OUT_FILE"
