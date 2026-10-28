#!/bin/sh
# Builds the template editor (src/editor) into standalone test binaries, without the rest of the application:
#   build.sh <out-dir> <main.cpp>...     -> <out-dir>/<name> for every given main file (test.cpp -> test)
# Needs Qt6 Core/Gui/Widgets/Svg/Test headers and moc/rcc (Qt6 libexec; or MOC=/RCC=).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC="$HERE/../../src"
QT=${QT_INCLUDE:-/usr/include/qt6}
MOC=${MOC:-$(command -v moc-qt6 || echo /usr/lib64/qt6/libexec/moc)}
RCC=${RCC:-$(command -v rcc-qt6 || echo /usr/lib64/qt6/libexec/rcc)}
OUT=$1; shift
mkdir -p "$OUT"
INC="-I$SRC/template -I$SRC/editor -I$SRC/utils -I$OUT -isystem $QT -isystem $QT/QtCore -isystem $QT/QtGui -isystem $QT/QtWidgets -isystem $QT/QtSvg -isystem $QT/QtTest"
CXX="g++ -O1 -fPIC -std=gnu++17 -Wall -Wextra -Wno-unused-parameter -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_SVG_LIB -DQT_TESTLIB_LIB $INC"

for h in editordocument editorcanvas propertiespanel layerspanel macropalette canvassizedialog sizepreviewpanel templateeditor gradientedit; do
  "$MOC" "$SRC/editor/$h.h" -o "$OUT/moc_$h.cpp"
done
"$MOC" "$SRC/utils/externaleditor.h" -o "$OUT/moc_externaleditor.cpp"
"$MOC" "$SRC/editor/gradientedit.cpp" -o "$OUT/gradientedit.moc"     # GradientStopBar is defined in the .cpp
"$RCC" --name editor "$SRC/editor/editor.qrc" -o "$OUT/qrc_editor.cpp"

# objects are rebuilt only when a source is newer
SOURCES="template/templatedoc template/templatelegacy template/mmsstvtemplate template/mmsstvimport template/templaterender template/templatevars utils/macroexpansion utils/externaleditor
editor/editordocument editor/editorcommands editor/editorcanvas editor/editoricons editor/gradientedit editor/propertiespanel
editor/layerspanel editor/macropalette editor/canvassizedialog editor/sizepreviewpanel editor/templatepresets editor/templateeditor"
OBJS=""
for s in $SOURCES; do
  o="$OUT/$(basename $s).o"
  OBJS="$OBJS $o"
  if [ ! -f "$o" ] || [ "$SRC/$s.cpp" -nt "$o" ] || [ -n "$(find "$SRC/editor" "$SRC/template" -name '*.h' -newer "$o")" ]; then
    $CXX -c "$SRC/$s.cpp" -o "$o" &
  fi
done
for h in editordocument editorcanvas propertiespanel layerspanel macropalette canvassizedialog sizepreviewpanel templateeditor gradientedit externaleditor; do
  o="$OUT/moc_$h.o"
  OBJS="$OBJS $o"
  if [ ! -f "$o" ] || [ "$OUT/moc_$h.cpp" -nt "$o" ]; then $CXX -c "$OUT/moc_$h.cpp" -o "$o" & fi
done
$CXX -c "$OUT/qrc_editor.cpp" -o "$OUT/qrc_editor.o" &
wait
for m in "$@"; do
  name=$(basename "$m" .cpp)
  $CXX "$m" $OBJS "$OUT/qrc_editor.o" -o "$OUT/$name" -lQt6Test -lQt6Svg -lQt6Widgets -lQt6Gui -lQt6Core
done
