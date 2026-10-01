# Builder image for the QSSTV-Experimental AppImage. Ubuntu 24.04 on purpose: the AppImage
# can't run on a system older than the glibc it was built against (24.04 = glibc 2.39), so
# building here, not on a rolling distro, is what sets the compatibility floor.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential g++ gcc make cmake pkg-config git wget curl ca-certificates file rsync \
    autoconf automake libtool gettext texinfo libusb-1.0-0-dev libltdl-dev \
    qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-serialport-dev qt6-multimedia-dev libxkbcommon-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
    libfftw3-dev libasound2-dev libpulse-dev libopenjp2-7-dev libv4l-dev libjpeg-dev \
    patchelf desktop-file-utils appstream \
    && rm -rf /var/lib/apt/lists/*
# linuxdeploy tools, extracted (no FUSE inside a container)
WORKDIR /opt/linuxdeploy
RUN wget -q -O linuxdeploy.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
 && wget -q -O linuxdeploy-plugin-qt.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage \
 && chmod +x *.AppImage \
 && ./linuxdeploy.AppImage --appimage-extract >/dev/null && mv squashfs-root linuxdeploy.dir \
 && ./linuxdeploy-plugin-qt.AppImage --appimage-extract >/dev/null && mv squashfs-root linuxdeploy-plugin-qt.dir \
 && ln -s /opt/linuxdeploy/linuxdeploy.dir/AppRun /usr/local/bin/linuxdeploy \
 && ln -s /opt/linuxdeploy/linuxdeploy-plugin-qt.dir/AppRun /usr/local/bin/linuxdeploy-plugin-qt
WORKDIR /work
