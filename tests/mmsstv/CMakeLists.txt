cmake_minimum_required(VERSION 3.16)
project(mmsstv_import_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Svg)
set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(TEMPLATE_SOURCES
  ${SRC}/template/templatedoc.cpp ${SRC}/template/templatelegacy.cpp
  ${SRC}/template/templaterender.cpp ${SRC}/template/templatevars.cpp
  ${SRC}/template/mmsstvtemplate.cpp ${SRC}/template/mmsstvimport.cpp
  ${SRC}/utils/macroexpansion.cpp ${SRC}/utils/externaleditor.cpp)
set(EDITOR_SOURCES)
foreach(name editordocument editorcommands editorcanvas editoricons gradientedit propertiespanel
    layerspanel macropalette canvassizedialog sizepreviewpanel templatepresets templateeditor)
  list(APPEND EDITOR_SOURCES ${SRC}/editor/${name}.cpp)
endforeach()
add_library(mmsstv_editor STATIC ${TEMPLATE_SOURCES} ${EDITOR_SOURCES})
target_include_directories(mmsstv_editor PUBLIC ${SRC}/template ${SRC}/editor ${SRC}/utils)
target_link_libraries(mmsstv_editor PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg)
add_executable(mmsstv_import_test test.cpp ${SRC}/editor/editor.qrc)
target_link_libraries(mmsstv_import_test PRIVATE mmsstv_editor)
enable_testing()
add_test(NAME mmsstv_import COMMAND mmsstv_import_test)
set_tests_properties(mmsstv_import PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
