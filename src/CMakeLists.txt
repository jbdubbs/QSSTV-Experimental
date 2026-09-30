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

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Xml)

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
  find_path(HAMLIB_INCLUDE_DIR NAMES hamlib/rig.h HINTS "${HAMLIB_ROOT}/include" NO_DEFAULT_PATH)
  find_library(HAMLIB_LIBRARY NAMES hamlib HINTS "${HAMLIB_ROOT}/lib" NO_DEFAULT_PATH)
  if(NOT HAMLIB_INCLUDE_DIR OR NOT HAMLIB_LIBRARY)
    message(FATAL_ERROR "HAMLIB_ROOT=${HAMLIB_ROOT} was set but hamlib/rig.h or libhamlib was not found under it")
  endif()
  add_library(hamlib::hamlib UNKNOWN IMPORTED)
  set_target_properties(hamlib::hamlib PROPERTIES
    IMPORTED_LOCATION "${HAMLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${HAMLIB_INCLUDE_DIR}")
  get_filename_component(QSSTV_HAMLIB_LIBDIR "${HAMLIB_LIBRARY}" DIRECTORY)
else()
  pkg_check_modules(HAMLIB REQUIRED IMPORTED_TARGET hamlib)
  add_library(hamlib::hamlib ALIAS PkgConfig::HAMLIB)
  pkg_get_variable(QSSTV_HAMLIB_LIBDIR hamlib libdir)
endif()

# --- mmsstv-core (sibling checkout; see qsstv.pro's original comment) ---------------
set(MMSSTV_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../mmsstv-core" CACHE PATH "Path to the mmsstv-core checkout")
add_subdirectory("${MMSSTV_CORE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/mmsstv-core")

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
    sound/soundpulse.cpp
    widgets/spectrumwidget.cpp
    widgets/vumeter.cpp
    widgets/fftdisplay.cpp
    widgets/markerwidget.cpp
    dsp/downsamplefilter.cpp
    utils/arraydumper.cpp
    sound/calibration.cpp
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
    sstv/mmsstv_sstv_tx.cpp
    sstv/mmsstv_sstv_rx.cpp
    sstv/engineselection.cpp
    sstv/videofilterselection.cpp
    sstv/chromadeconvolution.cpp
    sstv/chromacompanding.cpp
    sstv/chromaedgeboost.cpp
    sstv/chromagridphase.cpp
    sstv/chromapseudoluma.cpp
    sstv/chromatriangledecimation.cpp
    sstv/syncprocessor.cpp
    utils/qurlinfo.cpp
    utils/reedsolomoncoder.cpp
    utils/rs.cpp
    xmlrpc/ipcmessage.cpp
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
)

# Linux-only today (excluded on macOS via the original qmake !macx: gate). Windows gets
# its own Qt Multimedia based backend in Phase 4/5 rather than joining this bucket.
if(NOT APPLE)
  list(APPEND QSSTV_SOURCES
    sound/soundalsa.cpp
    videocapt/cameradialog.cpp
    videocapt/imagesettings.cpp
    videocapt/v4l2control.cpp
    videocapt/videocapture.cpp
  )
endif()

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
)

if(NOT APPLE)
  list(APPEND QSSTV_FORMS
    videocapt/cameradialog.ui
    videocapt/imagesettings.ui
  )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  list(APPEND QSSTV_FORMS
    scope/scopeoffset.ui
    scope/plotform.ui
  )
endif()

add_executable(qsstv ${QSSTV_SOURCES} ${QSSTV_FORMS} qsstv.qrc)

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
    Qt6::Core Qt6::Widgets Qt6::Network Qt6::Xml
    PkgConfig::FFTW3
    PkgConfig::OPENJP2
    hamlib::hamlib
    mmsstv_core
    pulse
    pulse-simple
)

if(NOT APPLE)
  target_link_libraries(qsstv PRIVATE asound v4l2 v4lconvert rt)
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

# --- Install (Linux desktop integration; full packaging is Phase 7) -----------------
include(GNUInstallDirs)
install(TARGETS qsstv RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(UNIX AND NOT APPLE)
  install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/../qsstv.desktop" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")
  install(FILES icons/qsstv.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps")
endif()
