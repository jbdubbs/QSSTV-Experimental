#!/bin/bash
# Cross-compile the AVIF and HEIC Qt image plugins (KDE kimageformats' kimg_avif / kimg_heif)
# for 64-bit Windows, along with everything they need, and vendor the result into this repo's
# gitignored third_party/ tree -- the Windows package (src/CMakeLists.txt) picks the DLLs up
# from there. Fedora has no mingw packages for any of these, hence from source.
#
# What gets built (all CMake, so no meson/nasm is needed):
#   libde265  HEVC decoder (HEIC), static            -- LGPL
#   libaom    AV1 decoder (AVIF + HEIC-with-AV1), static, generic C (no nasm) -- BSD
#   libheif   shared libheif.dll with libde265+libaom linked in, so, unlike the Linux
#             AppImage, no runtime codec-plugin directory is needed on Windows
#   libavif   shared libavif.dll, decodes through libaom
#   ECM       KDE extra-cmake-modules (kimageformats' build system)
#   kimageformats  only kimg_avif.dll and kimg_heif.dll are kept
#
# Usage:
#   tools/build-image-plugins-win64.sh                 # uses the defaults below
#   JOBS=4 tools/build-image-plugins-win64.sh
#   rm -rf third_party/imageformats-win64*             # to start over from clean
#
# Needs (Fedora): mingw64-gcc mingw64-gcc-c++ mingw64-qt6-qtbase mingw64-qt6-qtbase-devel
#   (the mingw Qt's private headers) cmake git, and network access to clone the sources.
#
# Output: third_party/imageformats-win64/{bin/*.dll, plugins/imageformats/kimg_*.dll}
# Sources and build trees live in third_party/imageformats-win64-src (disposable).

set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)   # repo root
PREFIX=${IMAGEFORMATS_PREFIX:-$HERE/third_party/imageformats-win64}
WORK=${IMAGEFORMATS_WORK:-${PREFIX}-src}
MINGW_HOST=${MINGW_HOST:-x86_64-w64-mingw32}
MINGW_SYSROOT=${MINGW_SYSROOT:-/usr/$MINGW_HOST/sys-root/mingw}
TOOLCHAIN=${TOOLCHAIN:-/usr/share/mingw/toolchain-mingw64.cmake}
JOBS=${JOBS:-$(nproc)}

# Pinned releases. kimageformats must be new enough for the mingw Qt (6.11) and is the same tag
# the Linux AppImage builds (packaging/appimage/Dockerfile); ECM must match it.
LIBDE265_TAG=${LIBDE265_TAG:-v1.1.3}
LIBAOM_TAG=${LIBAOM_TAG:-v3.15.1}
LIBHEIF_TAG=${LIBHEIF_TAG:-v1.23.5}
LIBAVIF_TAG=${LIBAVIF_TAG:-v1.4.2}
KDE_TAG=${KDE_TAG:-v6.10.0}

mkdir -p "$PREFIX" "$WORK"

# Fedora's toolchain file assigns CMAKE_FIND_ROOT_PATH itself, silently discarding a -D value,
# so the vendored prefix is added through a wrapper toolchain instead -- otherwise find_package()
# never looks in it (kimageformats could not find libavif).
cat > "$WORK/toolchain.cmake" <<EOF
include("$TOOLCHAIN")
list(APPEND CMAKE_FIND_ROOT_PATH "$PREFIX")
list(APPEND CMAKE_PREFIX_PATH "$PREFIX")
EOF
TOOLCHAIN="$WORK/toolchain.cmake"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/lib64/pkgconfig"

# clone <url> <tag> <dir>: shallow, idempotent
clone() {
  [ -d "$WORK/$3/.git" ] || git clone -q --depth 1 --branch "$2" "$1" "$WORK/$3"
}

# build <dir> <cmake args...>: configure, build, install; the prefix is added to the find root
# so later components find the earlier ones and not a (nonexistent) host copy
build() {
  local dir=$1; shift
  cmake -S "$WORK/$dir" -B "$WORK/$dir/build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    "$@"
  cmake --build "$WORK/$dir/build" -j"$JOBS"
  cmake --install "$WORK/$dir/build"
}

echo "== libde265 $LIBDE265_TAG"
clone https://github.com/strukturag/libde265.git "$LIBDE265_TAG" libde265
build libde265 -DBUILD_SHARED_LIBS=OFF -DENABLE_DECODER=ON -DENABLE_ENCODER=OFF -DENABLE_SDL=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5

echo "== libaom $LIBAOM_TAG"
clone https://aomedia.googlesource.com/aom "$LIBAOM_TAG" libaom
build libaom -DBUILD_SHARED_LIBS=OFF -DENABLE_ENCODER=0 -DAOM_TARGET_CPU=generic \
  -DENABLE_DOCS=0 -DENABLE_EXAMPLES=0 -DENABLE_TESTS=0 -DENABLE_TESTDATA=0 -DENABLE_TOOLS=0 \
  -DENABLE_NASM=0

echo "== libheif $LIBHEIF_TAG"
clone https://github.com/strukturag/libheif.git "$LIBHEIF_TAG" libheif
# LIBDE265_STATIC_BUILD: libde265.a is static, but its header marks symbols dllimport otherwise
build libheif -DCMAKE_CXX_FLAGS=-DLIBDE265_STATIC_BUILD -DCMAKE_C_FLAGS=-DLIBDE265_STATIC_BUILD -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DWITH_EXAMPLES=OFF -DWITH_GDK_PIXBUF=OFF \
  -DENABLE_PLUGIN_LOADING=OFF \
  -DWITH_LIBDE265=ON -DWITH_LIBDE265_PLUGIN=OFF \
  -DWITH_AOM_DECODER=ON -DWITH_AOM_DECODER_PLUGIN=OFF -DWITH_AOM_ENCODER=OFF \
  -DWITH_X265=OFF -DWITH_DAV1D=OFF -DWITH_RAV1E=OFF -DWITH_SvtEnc=OFF -DWITH_KVAZAAR=OFF \
  -DWITH_OpenJPEG_DECODER=OFF -DWITH_OpenJPEG_ENCODER=OFF -DWITH_JPEG_DECODER=OFF \
  -DWITH_JPEG_ENCODER=OFF -DWITH_UNCOMPRESSED_CODEC=OFF -DWITH_FFMPEG_DECODER=OFF \
  -DWITH_LIBSHARPYUV=OFF

echo "== libavif $LIBAVIF_TAG"
clone https://github.com/AOMediaCodec/libavif.git "$LIBAVIF_TAG" libavif
build libavif -DBUILD_SHARED_LIBS=ON -DAVIF_BUILD_APPS=OFF -DAVIF_BUILD_TESTS=OFF \
  -DAVIF_BUILD_EXAMPLES=OFF -DAVIF_BUILD_MAN=OFF \
  -DAVIF_CODEC_AOM=SYSTEM -DAVIF_CODEC_AOM_ENCODE=OFF \
  -DAVIF_CODEC_DAV1D=OFF \
  -DAVIF_CODEC_LIBGAV1=OFF -DAVIF_CODEC_RAV1E=OFF -DAVIF_CODEC_SVT=OFF \
  -DAVIF_LIBYUV=OFF -DAVIF_LIBSHARPYUV=OFF -DAVIF_JPEG=OFF -DAVIF_ZLIBPNG=OFF

echo "== extra-cmake-modules $KDE_TAG"
clone https://invent.kde.org/frameworks/extra-cmake-modules.git "$KDE_TAG" ecm
build ecm -DBUILD_TESTING=OFF -DBUILD_HTML_DOCS=OFF -DBUILD_MAN_DOCS=OFF -DBUILD_QTHELP_DOCS=OFF

echo "== kimageformats $KDE_TAG"
clone https://invent.kde.org/frameworks/kimageformats.git "$KDE_TAG" kimageformats
# Qt6's CMake package config rewrites CMAKE_FIND_ROOT_PATH/MODE when found (dropping our prefix), so
# kimageformats' own find_package(libavif) never looks in it. Point it at the vendored copy
# explicitly, and ask for major version 1 up front (its 0.8.2 probe is rejected by libavif 1.x).
sed -i "s#^find_package(libavif 0.8.2 CONFIG QUIET)#set(libavif_DIR \"$PREFIX/lib/cmake/libavif\" CACHE PATH \"vendored libavif\" FORCE)\nfind_package(libavif 1 CONFIG QUIET)#" \
  "$WORK/kimageformats/CMakeLists.txt"
build kimageformats -DBUILD_TESTING=OFF -DKIMAGEFORMATS_HEIF=ON \
  -DKDE_INSTALL_QTPLUGINDIR="$PREFIX/plugins" -DKDE_INSTALL_PLUGINDIR="$PREFIX/plugins" \
  -DECM_DIR="$PREFIX/share/ECM/cmake" \
  -DCMAKE_MODULE_LINKER_FLAGS="-L$PREFIX/lib" -DCMAKE_SHARED_LINKER_FLAGS="-L$PREFIX/lib"

# Keep only what we ship: the two plugins and the DLLs they load (no codec CLI tools).
rm -f "$PREFIX"/bin/*.exe
for f in "$PREFIX"/plugins/imageformats/*; do
  case "$(basename "$f")" in kimg_avif.dll|kimg_heif.dll) ;; *) rm -f "$f" ;; esac
done

echo
echo "Done. Vendored under $PREFIX:"
ls "$PREFIX"/plugins/imageformats "$PREFIX"/bin
