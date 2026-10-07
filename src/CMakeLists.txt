cmake_minimum_required(VERSION 3.16)
project(qsstv LANGUAGES CXX)

# --- Phase 0 of the cross-platform plan: qmake -> CMake, Linux/macOS parity with the
# existing qsstv.pro. Windows sources/backends land incrementally in later phases (see
# /home/jbdubbs/.claude/plans/crystalline-sleeping-thompson.md); this file does not yet
# attempt a Windows configuration.

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Xml SerialPort Multimedia)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFTW3 REQUIRED IMPORTED_TARGET fftw3 fftw3f)
pkg_check_modules(OPENJP2 REQUIRED IMPORTED_TARGET libopenjp2)

# --- Hamlib -------------------------------------------------------------------------
# On this dev box there is no system-wide hamlib package at all -- the project already
# builds against a from-source checkout (~/projects/hamlib-upstream-sstv/install), the
# same one Phase 1 of the plan vendors for Windows. HAMLIB_ROOT makes that explicit and
# reproducible instead of relying on PKG_CONFIG_PATH being set in the invoking shell;
# plain pkg-config discovery remains available for machines that do have a system
# hamlib.pc (e.g. a real distro package).
set(HAMLIB_ROOT "" CACHE PATH "Prefix of a from-source Hamlib install (skips pkg-config)")
if(HAMLIB_ROOT)
  # NO_CMAKE_FIND_ROOT_PATH: when cross-compiling (see cmake/ toolchain files),
  # CMAKE_FIND_ROOT_PATH_MODE_INCLUDE/LIBRARY are ONLY, which would otherwise restrict
  # this search to the mingw sysroot -- but HAMLIB_ROOT deliberately points outside it,
  # at this repo's own vendored build (see tools/build-hamlib-win64.sh).
  find_path(HAMLIB_INCLUDE_DIR NAMES hamlib/rig.h HINTS "${HAMLIB_ROOT}/include"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  find_library(HAMLIB_LIBRARY NAMES hamlib HINTS "${HAMLIB_ROOT}/lib"
    NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
  if(NOT HAMLIB_INCLUDE_DIR OR NOT HAMLIB_LIBRARY)
    message(FATAL_ERROR "HAMLIB_ROOT=${HAMLIB_ROOT} was set but hamlib/rig.h or libhamlib was not found under it")
  endif()
  add_library(hamlib::hamlib UNKNOWN IMPORTED)
  set_target_properties(hamlib::hamlib PROPERTIES
    IMPORTED_LOCATION "${HAMLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${HAMLIB_INCLUDE_DIR}")
  # This vendored build is static, and a static archive doesn't carry its own transitive
  # link dependencies the way a shared lib's SONAME chain does -- tools/build-hamlib-win64.sh
  # picked up mingw64-libusb1 automatically (for the USB-CAT rig backends, e.g. FiFi-SDR/
  # FUNcube), so anything linking hamlib::hamlib needs libusb-1.0 explicitly too.
  pkg_check_modules(LIBUSB1 QUIET IMPORTED_TARGET libusb-1.0)
  set(_hamlib_extra_libs "")
  if(LIBUSB1_FOUND)
    list(APPEND _hamlib_extra_libs PkgConfig::LIBUSB1)
  endif()
  if(WIN32)
    # hamlib's network.c uses GetAdaptersAddresses() (IP Helper API) for interface
    # detection -- another static-archive transitive dependency our own link needs too.
    list(APPEND _hamlib_extra_libs iphlpapi)
  endif()
  if(_hamlib_extra_libs)
    set_target_properties(hamlib::hamlib PROPERTIES INTERFACE_LINK_LIBRARIES "${_hamlib_extra_libs}")
  endif()
  get_filename_component(QSSTV_HAMLIB_LIBDIR "${HAMLIB_LIBRARY}" DIRECTORY)
else()
  pkg_check_modules(HAMLIB REQUIRED IMPORTED_TARGET hamlib)
  add_library(hamlib::hamlib ALIAS PkgConfig::HAMLIB)
  pkg_get_variable(QSSTV_HAMLIB_LIBDIR hamlib libdir)
endif()


# --- Qwt (debug-only oscilloscope) ---------------------------------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  find_path(QWT_INCLUDE_DIR NAMES qwt_plot.h PATH_SUFFIXES qwt qt6/qwt qt5/qwt)
  find_library(QWT_LIBRARY NAMES qwt-qt6 qwt)
  if(NOT QWT_INCLUDE_DIR OR NOT QWT_LIBRARY)
    message(FATAL_ERROR "Debug build needs Qwt (scope/* oscilloscope) but it was not found; "
                         "install it or configure with -DCMAKE_BUILD_TYPE=Release")
  endif()
  add_library(qwt::qwt UNKNOWN IMPORTED)
  set_target_properties(qwt::qwt PROPERTIES
    IMPORTED_LOCATION "${QWT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${QWT_INCLUDE_DIR}")
endif()

# --- Doxygen (optional; never blocks the build) -------------------------------------
find_package(Doxygen QUIET)

# --- Sources --------------------------------------------------------------------------
set(QSSTV_SOURCES
    main.cpp
    mainwindow.cpp
    config/baseconfig.cpp
    config/soundconfig.cpp
    utils/dirdialog.cpp
    utils/logging.cpp
    utils/loggingparams.cpp
    appglobal.cpp
    utils/supportfunctions.cpp
    sound/wavio.cpp
    sound/wavreader.cpp
    config/directoriesconfig.cpp
    config/configdialog.cpp
    sound/soundbase.cpp
    sound/soundqtmultimedia.cpp
    widgets/spectrumwidget.cpp
    widgets/vumeter.cpp
    widgets/fftdisplay.cpp
    widgets/markerwidget.cpp
    dsp/downsamplefilter.cpp
    utils/arraydumper.cpp
    sound/calibration.cpp
    sound/calibrationmethod.cpp
    sound/calibrationwwv.cpp
    sound/wwvtickdetector.cpp
    sound/wwvtickfit.cpp
    sound/calibrationntp.cpp
    sound/calibrationsstv.cpp
    sound/slantfit.cpp
    sound/sntpclient.cpp
    sound/linefit.cpp
    dsp/synthes.cpp
    dsp/filterparam.cpp
    sound/waterfalltext.cpp
    utils/macroexpansion.cpp
    widgets/imageviewer.cpp
    editor/editor.cpp
    editor/editorscene.cpp
    editor/editorview.cpp
    editor/gradientdialog.cpp
    editor/graphicitems.cpp
    widgets/textdisplay.cpp
    dispatch/dispatcher.cpp
    dispatch/filedecoder.cpp
    drmrx/bits2bytes.cpp
    drmrx/channeldecode.cpp
    drmrx/crc16_bytewise.cpp
    drmrx/crc8_c.cpp
    drmrx/deinterleaver.cpp
    drmrx/demodulator.cpp
    drmrx/drm.cpp
    drmrx/drmconstellationframe.cpp
    drmrx/drmpsdframe.cpp
    drmrx/drmrx.cpp
    drmrx/drmstatusframe.cpp
    drmrx/filter1.cpp
    drmrx/filter1c.cpp
    drmrx/fixform.cpp
    drmrx/getfoffsint.cpp
    drmrx/getmode.cpp
    drmrx/getofdm.cpp
    drmrx/getofdmsync.cpp
    drmrx/getsymbolidx.cpp
    drmrx/lubksb.cpp
    drmrx/ludcmp.cpp
    drmrx/mkfacmap.cpp
    drmrx/mkmscmap.cpp
    drmrx/msdhardfac.cpp
    drmrx/msdhardmsc.cpp
    drmrx/newfft.cpp
    drmrx/nrutil.cpp
    drmrx/psdcmean.cpp
    drmrx/psdmean.cpp
    drmrx/sourcedecoder.cpp
    drmrx/viterbi_decode.cpp
    drmtx/common/datadecoding/DABMOT.cpp
    drmtx/common/datadecoding/DataDecoder.cpp
    drmtx/common/datadecoding/MOTSlideShow.cpp
    drmtx/common/FAC/FAC.cpp
    drmtx/common/interleaver/BlockInterleaver.cpp
    drmtx/common/interleaver/SymbolInterleaver.cpp
    drmtx/common/matlib/MatlibSigProToolbox.cpp
    drmtx/common/matlib/MatlibStdToolbox.cpp
    drmtx/common/mlc/BitInterleaver.cpp
    drmtx/common/mlc/ChannelCode.cpp
    drmtx/common/mlc/ConvEncoder.cpp
    drmtx/common/mlc/EnergyDispersal.cpp
    drmtx/common/mlc/MLC.cpp
    drmtx/common/mlc/QAMMapping.cpp
    drmtx/common/ofdmcellmapping/CellMappingTable.cpp
    drmtx/common/ofdmcellmapping/OFDMCellMapping.cpp
    drmtx/common/SDC/SDCTransmit.cpp
    drmtx/common/sourcedecoders/AudioSourceDecoder.cpp
    drmtx/common/tables/TableFAC.cpp
    drmtx/common/util/CRC.cpp
    drmtx/common/util/Utilities.cpp
    drmtx/common/csoundout.cpp
    drmtx/common/DataIO.cpp
    drmtx/common/DRMSignalIO.cpp
    drmtx/common/CDrmTransmitter.cpp
    drmtx/common/OFDM.cpp
    drmtx/common/Parameter.cpp
    drmtx/bsrform.cpp
    drmtx/drmparams.cpp
    drmtx/drmtransmitter.cpp
    drmtx/drmtx.cpp
    logbook/logbook.cpp
    mainwidgets/gallerywidget.cpp
    mainwidgets/rxfunctions.cpp
    mainwidgets/rxwidget.cpp
    mainwidgets/txfunctions.cpp
    mainwidgets/txwidget.cpp
    rig/freqdisplay.cpp
    rig/rigcontrol.cpp
    sstv/modes/modeavt.cpp
    sstv/modes/modebase.cpp
    sstv/modes/modebw.cpp
    sstv/modes/modegbr.cpp
    sstv/modes/modegbr2.cpp
    sstv/modes/modejb60.cpp
    sstv/modes/modepd.cpp
    sstv/modes/modergb.cpp
    sstv/modes/moderobot1.cpp
    sstv/modes/moderobot2.cpp
    sstv/cw.cpp
    sstv/sstvparam.cpp
    sstv/sstvrx.cpp
    sstv/sstvtx.cpp
    sstv/chromacompanding.cpp
    sstv/chromaedgeboost.cpp
    sstv/chromagridphase.cpp
    sstv/chromapseudoluma.cpp
    sstv/chromatriangledecimation.cpp
    sstv/syncprocessor.cpp
    utils/qurlinfo.cpp
    utils/reedsolomoncoder.cpp
    utils/rs.cpp
    xmlrpc/maiaFault.cpp
    xmlrpc/maiaObject.cpp
    xmlrpc/maiaXmlRpcClient.cpp
    xmlrpc/maiaXmlRpcServer.cpp
    xmlrpc/maiaXmlRpcServerConnection.cpp
    xmlrpc/xmlinterface.cpp
    config/operatorconfig.cpp
    widgets/extviewer.cpp
    config/guiconfig.cpp
    utils/qftp.cpp
    sound/resamplefilter.cpp
    widgets/imagematrix.cpp
    widgets/waterfallform.cpp
    config/rigconfig.cpp
    config/cwconfig.cpp
    config/drmprofileconfig.cpp
    config/ftpconfig.cpp
    config/hybridconfig.cpp
    config/repeaterconfig.cpp
    config/waterfallconfig.cpp
    utils/hybridcrypt.cpp
    sstv/visfskid.cpp
    dsp/filters.cpp
    dsp/filter.cpp
    widgets/drmsegmentsview.cpp
    widgets/xyzplaintextedit.cpp
    utils/hexconvertor.cpp
    config/frequencyselectwidget.cpp
    editor/canvassizeform.cpp
    utils/fftcalc.cpp
    utils/jp2io.cpp
    utils/color.cpp
    widgets/testpatternselection.cpp
    utils/filewatcher.cpp
    utils/ftpthread.cpp
    utils/ftpfunctions.cpp
    editor/basegraphicitem.cpp
    editor/templateviewer.cpp
    videocapt/cameradialog.cpp
    videocapt/imagesettings.cpp
)

# Video capture used to be Linux/macOS-only V4L2 (excluded on macOS via the original
# qmake !macx: gate); Phase 5 of the cross-platform plan replaced it with a Qt Multimedia
# camera backend (QCamera/QMediaCaptureSession/QVideoSink) that works on every OS, same
# as audio (Phase 4, sound/soundqtmultimedia.cpp) already did -- so both are unconditional
# now instead of gated source lists.

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND QSSTV_SOURCES
    scope/scopeoffset.cpp
    scope/scopeview.cpp
    scope/scopeplot.cpp
  )
endif()

set(QSSTV_FORMS
    mainwindow.ui
    config/guiconfig.ui
    config/soundconfig.ui
    utils/loggingform.ui
    config/directoriesconfig.ui
    config/configdialog.ui
    widgets/spectrumwidget.ui
    sound/calibration.ui
    editor/editorform.ui
    editor/gradientform.ui
    editor/textform.ui
    widgets/textdisplay.ui
    config/operatorconfig.ui
    drmrx/drmconstellationframe.ui
    drmrx/drmpsdframe.ui
    drmrx/drmstatusframe.ui
    widgets/extviewer.ui
    widgets/sweepform.ui
    widgets/waterfallform.ui
    config/rigconfig.ui
    config/cwconfig.ui
    config/drmprofileconfig.ui
    config/ftpconfig.ui
    config/hybridconfig.ui
    config/repeaterconfig.ui
    config/waterfallconfig.ui
    drmtx/bsrform.ui
    mainwidgets/gallerywidget.ui
    mainwidgets/rxwidget.ui
    mainwidgets/txwidget.ui
    widgets/freqform.ui
    rig/freqdisplay.ui
    widgets/drmsegmentsview.ui
    drmrx/fixform.ui
    config/frequencyselectwidget.ui
    editor/canvassizeform.ui
    widgets/testpatternselection.ui
    editor/templateviewer.ui
    videocapt/cameradialog.ui
    videocapt/imagesettings.ui
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND QSSTV_FORMS
    scope/scopeoffset.ui
    scope/plotform.ui
  )
endif()

add_executable(qsstv ${QSSTV_SOURCES} ${QSSTV_FORMS} qsstv.qrc)

if(WIN32)
  # GUI subsystem, not console -- double-clicking qsstv.exe shouldn't flash a console
  # window. main.cpp's attachParentConsoleIfAny() is what keeps --batch/--decode's
  # output reaching a terminal when launched from one despite this.
  set_target_properties(qsstv PROPERTIES WIN32_EXECUTABLE TRUE)
  # Windows Media Foundation (Qt's audio decoder there) has no Vorbis decoder, so decode
  # Ogg Vorbis with libvorbisfile (mingw64-libvorbis) instead; see sound/wavreader.cpp.
  pkg_check_modules(VORBISFILE vorbisfile)
  if(VORBISFILE_FOUND)
    target_compile_definitions(qsstv PRIVATE QSSTV_HAVE_VORBISFILE)
    target_include_directories(qsstv PRIVATE ${VORBISFILE_INCLUDE_DIRS})
    target_link_libraries(qsstv PRIVATE ${VORBISFILE_LIBRARIES})
  else()
    message(WARNING "vorbisfile not found (dnf install mingw64-libvorbis): Ogg Vorbis files will not decode on Windows")
  endif()
endif()

target_include_directories(qsstv PRIVATE
    config dispatch drmrx drmtx dsp editor logbook mainwidgets
    rig scope sound sstv utils widgets xmlrpc videocapt
)

# Unlike qmake's uic (which resolves all FORMS into one shared include path), CMake's
# AUTOUIC only looks in the including .cpp's own directory by default -- e.g.
# mainwidgets/txwidget.cpp includes "ui_freqform.h" for widgets/freqform.ui. Point it at
# every directory that holds a .ui file instead.
set_target_properties(qsstv PROPERTIES
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/config;${CMAKE_CURRENT_SOURCE_DIR}/dispatch;${CMAKE_CURRENT_SOURCE_DIR}/drmrx;${CMAKE_CURRENT_SOURCE_DIR}/drmtx;${CMAKE_CURRENT_SOURCE_DIR}/dsp;${CMAKE_CURRENT_SOURCE_DIR}/editor;${CMAKE_CURRENT_SOURCE_DIR}/logbook;${CMAKE_CURRENT_SOURCE_DIR}/mainwidgets;${CMAKE_CURRENT_SOURCE_DIR}/rig;${CMAKE_CURRENT_SOURCE_DIR}/scope;${CMAKE_CURRENT_SOURCE_DIR}/sound;${CMAKE_CURRENT_SOURCE_DIR}/sstv;${CMAKE_CURRENT_SOURCE_DIR}/utils;${CMAKE_CURRENT_SOURCE_DIR}/widgets;${CMAKE_CURRENT_SOURCE_DIR}/xmlrpc;${CMAKE_CURRENT_SOURCE_DIR}/videocapt"
)

target_compile_definitions(qsstv PRIVATE QT_DEPRECATED_WARNINGS)
target_compile_options(qsstv PRIVATE -Wno-implicit-fallthrough -Wno-psabi)

target_link_libraries(qsstv PRIVATE
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Xml Qt6::SerialPort Qt6::Multimedia
    PkgConfig::FFTW3
    PkgConfig::OPENJP2
    hamlib::hamlib
)

if(UNIX AND NOT APPLE)
  # soundbase.cpp's clock_gettime(), needed on older glibc where it lived in librt before
  # merging into libc (glibc 2.34+); Windows has no -lrt and doesn't need one, and this
  # was wrongly `if(NOT APPLE)` before Phase 7's Windows cross-build caught it (that
  # condition also matches Windows, which promptly failed to link).
  target_link_libraries(qsstv PRIVATE rt)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_link_libraries(qsstv PRIVATE qwt::qwt)
endif()

# Match the qmake build's rpath trick: find libhamlib.so at runtime without needing
# LD_LIBRARY_PATH, whether it came from HAMLIB_ROOT or system pkg-config.
if(QSSTV_HAMLIB_LIBDIR)
  set_target_properties(qsstv PROPERTIES
    BUILD_RPATH "${QSSTV_HAMLIB_LIBDIR}"
    INSTALL_RPATH "${QSSTV_HAMLIB_LIBDIR}")
endif()

# --- Install --------------------------------------------------------------------------
include(GNUInstallDirs)
if(WIN32)
  # The exe must sit next to its DLLs and plugin folders (platforms/, multimedia/, ...
  # below) -- Windows' DLL search checks the executable's own directory first, not a
  # bin/ subfolder one level up. CMAKE_INSTALL_BINDIR (bin/) is right for Linux/macOS,
  # where RPATH/install_name handle this instead, but would silently produce a package
  # that fails to launch with missing-DLL errors on Windows.
  install(TARGETS qsstv RUNTIME DESTINATION .)
else()
  install(TARGETS qsstv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(UNIX AND NOT APPLE)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../qsstv.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
  install(FILES icons/qsstv.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps")
endif()

if(WIN32)
  # No windeployqt available for this cross-compiled target (Fedora's mingw64-qt6-qttools
  # doesn't ship a working one, since it's a native-PE tool meant to run on the target
  # OS) -- list the runtime DLLs and Qt plugins this app actually needs explicitly
  # instead. This list is the full *recursive* dependency closure (qsstv.exe's own
  # imports, plus every bundled DLL's and plugin's own imports, walked to a fixed point)
  # -- checking only qsstv.exe's and the plugins' own direct imports, as an earlier pass
  # here did, misses things like Qt6Gui/Qt6Network's own transitive pulls (ICU,
  # fontconfig/freetype/harfbuzz for text shaping, OpenSSL for TLS, pcre2, zlib, ...),
  # which don't show up until something using them actually runs -- which is exactly
  # what happened: "icui18n77.dll was not found" on first real Windows execution.
  # Re-run tools/check-win-deps.py after any Qt/mingw package update to re-verify this
  # list instead of trusting it blindly again.
  list(GET CMAKE_FIND_ROOT_PATH 0 MINGW_SYSROOT)
  set(QSSTV_WIN_RUNTIME_DLLS
    libgcc_s_seh-1.dll
    libwinpthread-1.dll
    libstdc++-6.dll
    libfftw3-3.dll
    libfftw3f-3.dll
    libopenjp2.dll
    libusb-1.0.dll
    libjpeg-62.dll
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Multimedia.dll
    Qt6Network.dll
    Qt6SerialPort.dll
    Qt6Widgets.dll
    Qt6Xml.dll
    # Found only by walking the transitive closure (see tools/check-win-deps.py) --
    # Qt6Core's ICU/pcre2/zlib, Qt6Gui's font-shaping stack, Qt6Network's OpenSSL.
    iconv.dll
    icudata77.dll
    icui18n77.dll
    icuuc77.dll
    libbz2-1.dll
    libcrypto-3-x64.dll
    libexpat-1.dll
    libfontconfig-1.dll
    libfreetype-6.dll
    libglib-2.0-0.dll
    libharfbuzz-0.dll
    libintl-8.dll
    libpcre2-16-0.dll
    libpcre2-8-0.dll
    libpng16-16.dll
    zlib1.dll
    libwebp-7.dll
    libwebpdemux-2.dll
    libwebpmux-3.dll
    libsharpyuv-0.dll
  )
  if(VORBISFILE_FOUND)   # Ogg Vorbis decoding (mingw64-libvorbis, mingw64-libogg)
    list(APPEND QSSTV_WIN_RUNTIME_DLLS libvorbisfile-3.dll libvorbis-0.dll libogg-0.dll)
  endif()
  foreach(_dll ${QSSTV_WIN_RUNTIME_DLLS})
    install(FILES "${MINGW_SYSROOT}/bin/${_dll}" DESTINATION .)
  endforeach()

  # Qt loads plugins from <appdir>/<plugin-subdir>/ automatically (the same layout
  # windeployqt would produce), no qt.conf needed. qwindows.dll is required for any
  # window at all; the rest match what this app actually exercises: the camera backend
  # (Phase 5), JPEG save/load (snapshots, received/sent pictures), and a native-looking
  # widget style instead of Qt's generic fallback.
  install(FILES "${MINGW_SYSROOT}/lib/qt6/plugins/platforms/qwindows.dll" DESTINATION platforms)
  install(FILES "${MINGW_SYSROOT}/lib/qt6/plugins/multimedia/windowsmediaplugin.dll" DESTINATION multimedia)
  install(FILES "${MINGW_SYSROOT}/lib/qt6/plugins/imageformats/qjpeg.dll" DESTINATION imageformats)
  # WebP (mingw64-qt6-qtimageformats + mingw64-libwebp).
  install(FILES "${MINGW_SYSROOT}/lib/qt6/plugins/imageformats/qwebp.dll" DESTINATION imageformats)
  # AVIF/HEIC: KDE kimageformats plugins plus libheif/libavif (codecs linked in statically),
  # cross-built from source by tools/build-image-plugins-win64.sh into third_party/ (no mingw
  # packages exist). Optional: the package simply lacks those two formats if it was not run.
  set(QSSTV_IMGFMT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/imageformats-win64")
  if(EXISTS "${QSSTV_IMGFMT_DIR}/plugins/imageformats/kimg_heif.dll"
     AND EXISTS "${QSSTV_IMGFMT_DIR}/plugins/imageformats/kimg_avif.dll")
    install(FILES "${QSSTV_IMGFMT_DIR}/plugins/imageformats/kimg_heif.dll"
                  "${QSSTV_IMGFMT_DIR}/plugins/imageformats/kimg_avif.dll" DESTINATION imageformats)
    install(FILES "${QSSTV_IMGFMT_DIR}/bin/libheif.dll" "${QSSTV_IMGFMT_DIR}/bin/libavif.dll" DESTINATION .)
  else()
    message(STATUS "AVIF/HEIC plugins not found in ${QSSTV_IMGFMT_DIR} -- run tools/build-image-plugins-win64.sh to include them")
  endif()
  install(FILES "${MINGW_SYSROOT}/lib/qt6/plugins/styles/qmodernwindowsstyle.dll" DESTINATION styles)
endif()

# --- CPack ------------------------------------------------------------------------------
# One packaging mechanism for all three OSes instead of hand-rolled per-OS rules: ZIP
# everywhere (always available, no extra tool needed) plus NSIS on Windows -- a proper
# installer wizard instead of just a folder to unzip -- when makensis is present
# (Fedora: mingw64-nsis; not installed on this dev box as of this writing, so only
# verified with the ZIP generator so far).
# QSSTV-Experimental is built upon the original QSSTV 9.5.11 by Johan Maes, ON4QZ
# (https://www.qsl.net/o/on4qz).
set(CPACK_PACKAGE_NAME "QSSTV-Experimental")
set(CPACK_PACKAGE_VENDOR "QSSTV-Experimental")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SSTV and HamDRM transceiver")
set(CPACK_PACKAGE_VERSION "10.0-Pre5")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "QSSTV-Experimental")
set(CPACK_GENERATOR "ZIP")
if(WIN32)
  find_program(MAKENSIS_EXECUTABLE makensis)
  if(MAKENSIS_EXECUTABLE)
    list(APPEND CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
  else()
    message(STATUS "makensis not found (Fedora: mingw64-nsis) -- CPack will only produce a ZIP, not an NSIS installer")
  endif()
endif()
include(CPack)
