#!/bin/bash
# Runs inside the builder container (see build.sh).
set -euo pipefail
B=/build; mkdir -p $B/{qsstv-experimental,mmsstv-core,hamlib-upstream-sstv}
# copy sources; hamlib's generated configure/Makefiles are gitignored but present on the host
# (configured for the host distro), so exclude everything ignored plus prior build output
for d in qsstv-experimental mmsstv-core hamlib-upstream-sstv; do
  rsync -a --filter=':- .gitignore' --exclude=.git --exclude='build*' --exclude=install /src/$d/ $B/$d/
done

( cd $B/hamlib-upstream-sstv && ./bootstrap && ./configure --prefix=$B/hamlib-upstream-sstv/install \
    --disable-static --enable-shared && make -j"$(nproc)" && make install ) >/tmp/hamlib.log 2>&1 || { tail -40 /tmp/hamlib.log; exit 1; }

cmake -S $B/qsstv-experimental/src -B $B/qsstv-experimental/build-cmake -DCMAKE_BUILD_TYPE=Release \
  -DHAMLIB_ROOT=$B/hamlib-upstream-sstv/install -DCMAKE_PREFIX_PATH=$QT_ROOT
cmake --build $B/qsstv-experimental/build-cmake -j"$(nproc)"
APPDIR=$B/AppDir
cmake --install $B/qsstv-experimental/build-cmake --prefix $APPDIR/usr

# linuxdeploy's default excludelist drops these (assumed present on the host); bundle them all
# so the AppImage runs on a bare system. --library overrides the excludelist.
L=/usr/lib/x86_64-linux-gnu
FORCE=()
for pat in libasound libusb-1.0 libudev libEGL libGLX libGL libGLdispatch libOpenGL libX11 libX11-xcb \
  libxcb 'libxcb-*' libXau libXdmcp libbsd libmd libfontconfig libfreetype libharfbuzz libexpat libXrandr libXrender libXext libXfixes libSM libICE libdrm libfribidi libwayland-client; do
  for f in $L/$pat.so.*; do [ -e "$f" ] && FORCE+=(--library "$f"); done
done

# libheif dlopen()s its codec plugins (HEVC/AV1 decoders) at runtime, so linuxdeploy can't see
# them: pass each one with --library (bundles its codec deps), copy it where the hook below
# points LIBHEIF_PLUGIN_PATH, and set that path at startup.
HP=$L/libheif/plugins
mkdir -p $APPDIR/usr/lib/libheif/plugins $APPDIR/apprun-hooks
for f in $HP/*.so; do
  [ -e "$f" ] || continue
  cp "$f" $APPDIR/usr/lib/libheif/plugins/
  FORCE+=(--library "$f")
done
cat > $APPDIR/apprun-hooks/libheif-plugins.sh <<'H'
export LIBHEIF_PLUGIN_PATH="${APPDIR:-$this_dir}/usr/lib/libheif/plugins"
# the plugins' RUNPATH is only $ORIGIN, so their codec libs (libde265, ...) in usr/lib need this
export LD_LIBRARY_PATH="${APPDIR:-$this_dir}/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
H

cd $B
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE=$QT_ROOT/bin/qmake LD_LIBRARY_PATH=$QT_ROOT/lib VERSION NO_STRIP=1 PATCHELF=/usr/bin/patchelf
export EXTRA_PLATFORM_PLUGINS=libqoffscreen.so   # --batch headless mode needs offscreen
linuxdeploy --appdir $APPDIR "${FORCE[@]}" \
  --executable $APPDIR/usr/bin/qsstv \
  --desktop-file $APPDIR/usr/share/applications/qsstv.desktop \
  --icon-file $APPDIR/usr/share/icons/hicolor/128x128/apps/qsstv.png \
  --plugin qt --output appimage
OUT=QSSTV-Experimental-$VERSION-x86_64.AppImage
cp $OUT /out/$OUT
chown "$HOST_UID:$HOST_GID" /out/$OUT
