#!/bin/bash
# Cross-compile Hamlib for 64-bit Windows from a local checkout, and vendor the result
# into this repo's build tree -- see Phase 1 of the cross-platform plan
# (docs, if committed, or ask: it was /home/jbdubbs/.claude/plans/crystalline-sleeping-thompson.md).
#
# The Windows qsstv build links this instead of any system/distro Hamlib, so the
# shipped app can never depend on -- or silently fall back to -- an externally
# installed Hamlib. Static by default: the whole library is baked into qsstv.exe, which
# is the strongest guarantee against resolving a different Hamlib at runtime (no DLL
# search order to worry about at all).
#
# Usage:
#   tools/build-hamlib-win64.sh                # uses the defaults below
#   HAMLIB_SRC=/path/to/checkout tools/build-hamlib-win64.sh
#   HAMLIB_SHARED=1 tools/build-hamlib-win64.sh # build a DLL instead (see README note)
#
# Needs (Fedora): mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads
#   (mingw64-libusb1, if installed, lets configure pick up the USB-CAT rig backends too;
#   otherwise they are skipped automatically -- not an error.)
#
# Re-run this only when intentionally picking up a newer Hamlib commit from HAMLIB_SRC.
# It never touches HAMLIB_SRC itself: HAMLIB_SRC already has its own native Linux build
# configured in-tree (config.status/Makefile), and Automake's generated configure
# refuses to VPATH-build out of a source directory that's already configured elsewhere
# ("source directory already configured; run make distclean there first") -- rightly so,
# since blowing that away with `make distclean` would force a from-scratch reconfigure
# of the native build qsstv's own Linux build links against. Instead this script builds
# from a disposable `git worktree` of HAMLIB_SRC at its current commit, so the original
# checkout's native build state is never touched.

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)   # repo root
HAMLIB_SRC=${HAMLIB_SRC:-$HOME/projects/hamlib-upstream-sstv}
HAMLIB_PREFIX=${HAMLIB_PREFIX:-$HERE/third_party/hamlib-win64}
HAMLIB_WORKTREE=${HAMLIB_WORKTREE:-${HAMLIB_SRC}-win64-src}
MINGW_HOST=${MINGW_HOST:-x86_64-w64-mingw32}
JOBS=${JOBS:-$(nproc)}
HAMLIB_SHARED=${HAMLIB_SHARED:-0}   # 1 = --enable-shared --disable-static instead (see README)

[ -d "$HAMLIB_SRC/.git" ] || { echo "HAMLIB_SRC not a git checkout: $HAMLIB_SRC" >&2; exit 1; }

for tool in "${MINGW_HOST}-gcc" "${MINGW_HOST}-g++" "${MINGW_HOST}-ar" "${MINGW_HOST}-ranlib"; do
  command -v "$tool" >/dev/null 2>&1 || {
    echo "missing $tool -- on Fedora: sudo dnf install mingw64-gcc mingw64-gcc-c++ mingw64-binutils mingw64-winpthreads" >&2
    exit 1
  }
done
for tool in autoconf automake libtoolize; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing $tool (needed for ./bootstrap)" >&2; exit 1; }
done

HAMLIB_COMMIT=$(git -C "$HAMLIB_SRC" rev-parse --short HEAD)
HAMLIB_DESCRIBE=$(git -C "$HAMLIB_SRC" describe --tags --always 2>/dev/null || echo unknown)
echo "Building Hamlib from $HAMLIB_SRC @ $HAMLIB_COMMIT ($HAMLIB_DESCRIBE) for $MINGW_HOST"

# Recreate the worktree fresh every run: cheap (shares objects with HAMLIB_SRC, no full
# copy) and guarantees it matches HAMLIB_SRC's current commit exactly.
git -C "$HAMLIB_SRC" worktree remove --force "$HAMLIB_WORKTREE" >/dev/null 2>&1 || rm -rf "$HAMLIB_WORKTREE"
git -C "$HAMLIB_SRC" worktree add --detach --quiet "$HAMLIB_WORKTREE" "$HAMLIB_COMMIT"
trap 'git -C "$HAMLIB_SRC" worktree remove --force "$HAMLIB_WORKTREE" >/dev/null 2>&1 || true' EXIT

cd "$HAMLIB_WORKTREE"
./bootstrap

if [ "$HAMLIB_SHARED" = "1" ]; then
  LINK_FLAGS=(--enable-shared --disable-static)
  echo "Building a shared libhamlib DLL (HAMLIB_SHARED=1) -- remember to bundle it next to"
  echo "qsstv.exe at packaging time (Phase 6/7); Windows resolves DLLs from the app"
  echo "directory before PATH, so the bundled copy still wins over anything else installed."
else
  LINK_FLAGS=(--enable-static --disable-shared)
fi

CC="${MINGW_HOST}-gcc" \
CXX="${MINGW_HOST}-g++" \
AR="${MINGW_HOST}-ar" \
RANLIB="${MINGW_HOST}-ranlib" \
PKG_CONFIG="${MINGW_HOST}-pkg-config" \
./configure \
  --host="$MINGW_HOST" \
  --prefix="$HAMLIB_PREFIX" \
  "${LINK_FLAGS[@]}"

make -j"$JOBS"
make install

{
  echo "source:  $HAMLIB_SRC"
  echo "commit:  $HAMLIB_COMMIT ($HAMLIB_DESCRIBE)"
  echo "host:    $MINGW_HOST"
  echo "linkage: $([ "$HAMLIB_SHARED" = "1" ] && echo shared || echo static)"
  echo "built:   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "$HAMLIB_PREFIX/VENDORED_COMMIT.txt"

echo
echo "Installed to $HAMLIB_PREFIX"
echo "Configure qsstv's CMake build with:"
echo "  -DHAMLIB_ROOT=$HAMLIB_PREFIX"
