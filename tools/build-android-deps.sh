#!/bin/bash
# Cross-compile the native libraries the Android build links -- FFTW (double and float), OpenJPEG,
# libaom + libavif (optional AVIF over DRM) and Hamlib -- for each Android ABI, and vendor the
# result into this repo's gitignored third_party/ tree. Everything is built STATIC and position
# independent (-fPIC), because it ends up inside libqsstv-experimental_<abi>.so; that also means no
# extra .so files to align (Google Play needs 16 KB page alignment, see ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES).
#
# Layout:  third_party/android-deps/<abi>/{include,lib,lib/pkgconfig,...}
# Sources and build trees: third_party/android-deps-src (disposable).
#
# Usage:
#   tools/build-android-deps.sh                          # every component, ABIs arm64-v8a and x86_64
#   ABIS="arm64-v8a" tools/build-android-deps.sh         # one ABI
#   ONLY="fftw openjpeg" tools/build-android-deps.sh     # a subset: fftw openjpeg aom avif hamlib
#   rm -rf third_party/android-deps*                     # to start over from clean
#
# Needs: the Android NDK (r27c, the one the official Qt 6.11 Android kits were built with:
#   ANDROID_NDK_ROOT, default ~/Android/Sdk/ndk/27.2.12479018), cmake, ninja, git, curl;
#   Hamlib also needs autoconf automake libtool (its ./bootstrap), and a local Hamlib checkout
#   (HAMLIB_SRC, the same one tools/build-hamlib-win64.sh uses). nasm makes libaom faster on x86_64.
#
# Then configure the app with the matching Qt kit, for example (see BUILDING.md):
#   PKG_CONFIG_LIBDIR=$PWD/third_party/android-deps/arm64-v8a/lib/pkgconfig \
#   ~/Qt/6.11.2/android_arm64_v8a/bin/qt-cmake -S src -B build-android-arm64 \
#     -DQT_HOST_PATH=$HOME/Qt/6.11.2/gcc_64 -DHAMLIB_ROOT=$PWD/third_party/android-deps/arm64-v8a ...

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)   # repo root
NDK=${ANDROID_NDK_ROOT:-$HOME/Android/Sdk/ndk/27.2.12479018}
API=${ANDROID_API:-28}                                   # Qt 6.11 and Hamlib's own Android CI both use 28 as the minimum
ABIS=${ABIS:-"arm64-v8a x86_64"}
ONLY=${ONLY:-"fftw openjpeg aom avif hamlib"}
BASE=${ANDROID_DEPS_PREFIX:-$HERE/third_party/android-deps}
WORK=${ANDROID_DEPS_WORK:-${BASE}-src}
HAMLIB_SRC=${HAMLIB_SRC:-$HOME/projects/hamlib-upstream-sstv}
JOBS=${JOBS:-$(nproc)}

# Pinned releases. libaom and libavif are the same tags tools/build-image-plugins-win64.sh builds;
# FFTW's CMake build only exists in the release tarball (the git tree needs OCaml to generate its codelets).
FFTW_VERSION=${FFTW_VERSION:-3.3.10}
FFTW_SHA256=${FFTW_SHA256:-56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467}   # matches Debian's fftw3_3.3.10.orig.tar.gz
OPENJPEG_TAG=${OPENJPEG_TAG:-v2.5.4}     # the app calls opj_decoder_set_strict_mode(): needs >= 2.5
LIBAOM_TAG=${LIBAOM_TAG:-v3.15.1}
LIBAVIF_TAG=${LIBAVIF_TAG:-v1.4.2}

TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake"
LLVM="$NDK/toolchains/llvm/prebuilt/linux-x86_64"
[ -f "$TOOLCHAIN_FILE" ] || { echo "NDK not found: $NDK (set ANDROID_NDK_ROOT)" >&2; exit 1; }
for tool in cmake ninja git curl; do
  command -v "$tool" >/dev/null 2>&1 || { echo "missing $tool" >&2; exit 1; }
done
want() { case " $ONLY " in *" $1 "*) return 0 ;; *) return 1 ;; esac; }
if want hamlib; then
  [ -d "$HAMLIB_SRC/.git" ] || { echo "HAMLIB_SRC not a git checkout: $HAMLIB_SRC" >&2; exit 1; }
  for tool in autoconf automake libtoolize; do
    command -v "$tool" >/dev/null 2>&1 || { echo "missing $tool (needed for Hamlib's ./bootstrap)" >&2; exit 1; }
  done
fi

mkdir -p "$BASE" "$WORK"

# ABI -> clang target triple (the armv7 triple differs from the NDK's "arm" naming)
triple() {
  case "$1" in
    arm64-v8a)   echo aarch64-linux-android ;;
    armeabi-v7a) echo armv7a-linux-androideabi ;;
    x86_64)      echo x86_64-linux-android ;;
    x86)         echo i686-linux-android ;;
    *) echo "unknown ABI $1" >&2; return 1 ;;
  esac
}

# clone <url> <tag> <dir>: shallow, idempotent
clone() {
  [ -d "$WORK/$3/.git" ] || git clone -q --depth 1 --branch "$2" "$1" "$WORK/$3"
}

# build <dir> <cmake args...>: configure, build and install one CMake project for $ABI into $PREFIX.
# The prefix is added to the find root so later components find the earlier ones (the NDK toolchain
# file restricts every find_*() to its sysroot otherwise).
build() {
  local dir=$1 tag=$2; shift 2
  cmake -S "$WORK/$dir" -B "$WORK/$dir/build-$tag-$ABI" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="android-$API" \
    -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" -DCMAKE_PREFIX_PATH="$PREFIX" -DCMAKE_FIND_ROOT_PATH="$PREFIX" \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    "$@"
  cmake --build "$WORK/$dir/build-$tag-$ABI" -j"$JOBS"
  cmake --install "$WORK/$dir/build-$tag-$ABI"
}

if want fftw; then
  if [ ! -d "$WORK/fftw-$FFTW_VERSION" ]; then
    echo "== fetching fftw-$FFTW_VERSION"
    curl -sSL "https://www.fftw.org/fftw-$FFTW_VERSION.tar.gz" -o "$WORK/fftw-$FFTW_VERSION.tar.gz"
    echo "$FFTW_SHA256  $WORK/fftw-$FFTW_VERSION.tar.gz" | sha256sum -c -
    tar -xzf "$WORK/fftw-$FFTW_VERSION.tar.gz" -C "$WORK"
  fi
fi
if want hamlib; then
  # a disposable worktree of the Hamlib checkout, so its own native build state is never touched
  HAMLIB_COMMIT=$(git -C "$HAMLIB_SRC" rev-parse --short HEAD)
  HAMLIB_WORKTREE="$WORK/hamlib"
  git -C "$HAMLIB_SRC" worktree remove --force "$HAMLIB_WORKTREE" >/dev/null 2>&1 || rm -rf "$HAMLIB_WORKTREE"
  git -C "$HAMLIB_SRC" worktree add --detach --quiet "$HAMLIB_WORKTREE" "$HAMLIB_COMMIT"
  trap 'git -C "$HAMLIB_SRC" worktree remove --force "$HAMLIB_WORKTREE" >/dev/null 2>&1 || true' EXIT
  (cd "$HAMLIB_WORKTREE" && ./bootstrap)
fi

for ABI in $ABIS; do
  PREFIX="$BASE/$ABI"
  HOST=$(triple "$ABI")
  mkdir -p "$PREFIX"
  export PKG_CONFIG_LIBDIR="$PREFIX/lib/pkgconfig"   # never resolve against the build host's .pc files
  echo
  echo "################ $ABI ($HOST, API $API) -> $PREFIX"

  if want fftw; then
    echo "== fftw $FFTW_VERSION: double precision (libfftw3) and single precision (libfftw3f)"
    # FFTW_ESTIMATE only and no threads (the app plans from several threads under its own locking)
    FFTW_ARGS=(-DBUILD_SHARED_LIBS=OFF -DBUILD_TESTS=OFF -DENABLE_THREADS=OFF -DENABLE_OPENMP=OFF)
    build "fftw-$FFTW_VERSION" double "${FFTW_ARGS[@]}" -DENABLE_FLOAT=OFF
    build "fftw-$FFTW_VERSION" float  "${FFTW_ARGS[@]}" -DENABLE_FLOAT=ON
  fi

  if want openjpeg; then
    echo "== openjpeg $OPENJPEG_TAG"
    clone https://github.com/uclouvain/openjpeg.git "$OPENJPEG_TAG" openjpeg
    build openjpeg all -DBUILD_SHARED_LIBS=OFF -DBUILD_CODEC=OFF -DBUILD_TESTING=OFF -DBUILD_DOC=OFF -DBUILD_JPIP=OFF
  fi

  if want aom; then
    echo "== libaom $LIBAOM_TAG"
    clone https://aomedia.googlesource.com/aom "$LIBAOM_TAG" libaom
    case "$ABI" in
      arm64-v8a)   AOM_CPU="-DAOM_TARGET_CPU=arm64" ;;
      armeabi-v7a) AOM_CPU="-DAOM_TARGET_CPU=arm" ;;
      x86_64)
        if command -v nasm >/dev/null; then AOM_CPU="-DAOM_TARGET_CPU=x86_64 -DENABLE_NASM=1"
        else AOM_CPU="-DAOM_TARGET_CPU=generic -DENABLE_NASM=0"; echo "WARNING: nasm not found, libaom builds without SIMD"; fi ;;
      *) AOM_CPU="-DAOM_TARGET_CPU=generic" ;;
    esac
    # the encoder is needed to send AVIF; the examples, tools and tests are not
    build libaom all -DBUILD_SHARED_LIBS=OFF -DENABLE_ENCODER=1 $AOM_CPU \
      -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTS=0 -DENABLE_TESTDATA=0 -DENABLE_TOOLS=0
  fi

  if want avif; then
    echo "== libavif $LIBAVIF_TAG"
    clone https://github.com/AOMediaCodec/libavif.git "$LIBAVIF_TAG" libavif
    build libavif all -DBUILD_SHARED_LIBS=OFF -DAVIF_BUILD_APPS=OFF -DAVIF_BUILD_TESTS=OFF \
      -DAVIF_BUILD_EXAMPLES=OFF -DAVIF_BUILD_MAN=OFF \
      -DAVIF_CODEC_AOM=SYSTEM -DAVIF_CODEC_AOM_ENCODE=ON \
      -DAVIF_CODEC_DAV1D=OFF -DAVIF_CODEC_LIBGAV1=OFF -DAVIF_CODEC_RAV1E=OFF -DAVIF_CODEC_SVT=OFF \
      -DAVIF_LIBYUV=OFF -DAVIF_LIBSHARPYUV=OFF -DAVIF_JPEG=OFF -DAVIF_ZLIBPNG=OFF
  fi

  if want hamlib; then
    echo "== hamlib $HAMLIB_COMMIT (static, no libusb: Android's USB host API replaces it)"
    # Hamlib's own Android CI recipe (.github/workflows/android-ndk.yml): clang from the NDK, --without-libusb.
    # -fPIC because the static archive is linked into a shared library.
    HB="$WORK/hamlib-build-$ABI"
    rm -rf "$HB"; mkdir -p "$HB"
    (
      cd "$HB"
      export CC="$LLVM/bin/${HOST}${API}-clang" CXX="$LLVM/bin/${HOST}${API}-clang++"
      export AR="$LLVM/bin/llvm-ar" AS="$LLVM/bin/llvm-as" RANLIB="$LLVM/bin/llvm-ranlib" STRIP="$LLVM/bin/llvm-strip"
      export CFLAGS="-fPIC -O2" CXXFLAGS="-fPIC -O2"
      "$WORK/hamlib/configure" --host="$HOST" --prefix="$PREFIX" --enable-static --disable-shared --without-libusb
      make -j"$JOBS"
      make install
    )
  fi

  {
    echo "abi:     $ABI ($HOST), android-$API"
    echo "ndk:     $NDK"
    want fftw    && echo "fftw:    $FFTW_VERSION"
    want openjpeg && echo "openjpeg: $OPENJPEG_TAG"
    want aom     && echo "libaom:  $LIBAOM_TAG"
    want avif    && echo "libavif: $LIBAVIF_TAG"
    want hamlib  && echo "hamlib:  $HAMLIB_SRC @ $HAMLIB_COMMIT"
    echo "built:   $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  } > "$PREFIX/VENDORED.txt"
done

echo
echo "Done. Vendored under $BASE:"
ls "$BASE"
