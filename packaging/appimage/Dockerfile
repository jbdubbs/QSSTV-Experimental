# Builder image for the QSSTV-Experimental AppImage. Ubuntu 24.04 on purpose: the AppImage
# can't run on a system older than the glibc it was built against (24.04 = glibc 2.39), so
# building here, not on a rolling distro, is what sets the compatibility floor.
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential g++ gcc make cmake pkg-config git wget curl ca-certificates file rsync \
    autoconf automake libtool gettext texinfo libusb-1.0-0-dev libltdl-dev \
    python3-pip libxkbcommon-dev libxkbcommon-x11-dev libgl1-mesa-dev libglib2.0-dev libfontconfig1-dev libfreetype-dev libdbus-1-dev \
    libx11-xcb-dev libxrandr-dev libxcb-cursor-dev libxcb-glx0-dev libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-randr0-dev \
    libxcb-render-util0-dev libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-xinerama0-dev libxcb-xkb-dev libxcb-util-dev \
    libfftw3-dev libasound2-dev libpulse-dev libopenjp2-7-dev libv4l-dev libjpeg-dev \
    patchelf desktop-file-utils appstream \
    && rm -rf /var/lib/apt/lists/*
# Qt: the official Qt 6.11 binaries (via aqtinstall), NOT Ubuntu 24.04's Qt 6.4.2 -- 6.4's
# PulseAudio audio backend misbehaves (capture bytesAvailable() always 0, TX stalls). The
# glibc floor is set by this image's glibc, not by Qt, so the floor stays 2.39 (the Qt
# binaries themselves are built against an older glibc).
ARG QT_VERSION=6.11.2
RUN pip3 install --break-system-packages aqtinstall \
 && aqt install-qt linux desktop $QT_VERSION linux_gcc_64 -m qtmultimedia qtserialport -O /opt/Qt
ENV QT_ROOT=/opt/Qt/$QT_VERSION/gcc_64
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
