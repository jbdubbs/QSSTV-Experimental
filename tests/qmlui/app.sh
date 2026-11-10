#!/bin/bash
# Screenshots of the Qt Quick interface (src/qml) on the desktop, offscreen, with the software scene graph (tests/qmlui/shot.cpp).
#   tests/qmlui/app.sh [build-dir] [out-dir]     build-dir default build-qml (cmake -S src -B build-qml -DQSSTV_QML_UI=ON)
# Sizes: a phone upright, a small phone, a phone on its side, a tablet; light and dark. The pictures are kept for looking at.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=${1:-$HERE/../../build-qml}
OUT=${2:-}
cd "$B"
SCRATCH=$(mktemp -d -p "${TMPDIR:-/tmp}"); [ -n "$KEEP" ] || trap 'rm -rf "${SCRATCH:?}"' EXIT
[ -n "$OUT" ] || OUT=$(mktemp -d -t qsstv-qmlui-XXXXXX)
FLAGS=$(grep '^CXX_DEFINES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
INCS=$(grep '^CXX_INCLUDES' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
CXXF=$(grep '^CXX_FLAGS' CMakeFiles/qsstv.dir/flags.make | cut -d= -f2-)
eval /usr/bin/c++ $CXXF $FLAGS $INCS -DSRC_TEMPLATE_FIXTURE=\\\"$HERE/../template/fixtures/synthetic.templ\\\" -isystem /usr/include/qt6/QtTest -fPIC -c "$HERE/shot.cpp" -o "$SCRATCH/shot.o"
LINK=$(sed "s# CMakeFiles/qsstv.dir/main.cpp.o # $SCRATCH/shot.o #; s#-o qsstv-experimental#-o $SCRATCH/shot#; s#-Wl,--dependency-file=CMakeFiles/qsstv.dir/link.d##" CMakeFiles/qsstv.dir/link.txt)
eval $LINK /usr/lib64/libQt6Test.so
rc=0
run() {   # theme width height scenes...
  local theme=$1 w=$2 h=$3; shift 3
  local label=$theme${TINT:+-tint}${QSSTV_QML_FONTSCALE:+-x$QSSTV_QML_FONTSCALE}
  mkdir -p "$SCRATCH/home-$label-$w"
  env ${TINT:+QSSTV_QML_TINT=1} QSSTV_QML_THEME=$theme timeout 300 "$SCRATCH/shot" "$SCRATCH/home-$label-$w" "$OUT/$label" "$w" "$h" "$@" || rc=1
}
SCENES=${QMLUI_SCENES:-"idle receiving options digital stopped quiet level-steady tx tx-picture tx-modes tx-details tx-template tx-template-none tx-digital tx-digital-details tx-tools tx-sending tx-sending-tool tx-sending-sample tx-digital-sending tx-digital-sending-device tx-route-follows about gallery gallery-sent gallery-templates gallery-select gallery-viewer gallery-info gallery-delete saved gallery-empty settings settings-operator settings-pictures settings-format settings-position settings-location settings-location-failed settings-advanced settings-audio settings-mic settings-radios settings-receive settings-cw settings-appearance mic-denied firstrun-1 firstrun-2 tpl-editor tpl-text tpl-style tpl-place tpl-add tpl-layers tpl-more tpl-macros tpl-font tpl-font-missing tpl-fill tpl-fill-stops tpl-fill-shadow tpl-color tpl-oldtext tpl-keep tpl-name rx-defaults rx-mode rx-modes rx-mode-fixed rx-modes-fixed rx-listen tx-chips tpl-starters tpl-drag cal-overview cal-overview-done cal-reset cal-ntp cal-ntp-early cal-ntp-good cal-ntp-problem cal-wwv cal-wwv-measuring cal-sstv cal-sstv-got cal-sstv-send cal-sstv-sending drm-profiles-factory drm-profiles drm-profile freq-empty freq freq-edit freq-new freq-noradio rx-freq rx-freq-sheet tx-freq tx-mode tx-framing tx-framing-free tx-framing-menu tx-digital-framing tx-send tx-volume tx-volume-device tx-volume-room tx-volume-mismatch tx-sending-mismatch settings-audio-mismatch problem-ptt problem-ptt-short speaker-many tx-pointers"}
run light 411 800 $SCENES
run dark 411 800 $SCENES
# the looks of Settings > Appearance: Dim and Night (pictures red too in the second Night run: $OUT/night-tint; the thumbnails of the gallery and the viewer are
# red through a shader effect, which the software scene graph of this harness does not draw: they stay empty here and are red on a GPU)
LOOK="idle receiving options tx tx-picture tx-sending gallery gallery-viewer gallery-select settings settings-appearance settings-audio saved tpl-editor tpl-text cal-ntp-good freq"
run dim 411 800 $LOOK
run night 411 800 $LOOK
TINT=1 run night 411 800 idle receiving tx-picture tx-sending gallery settings-appearance
run light 360 640 idle receiving tx-picture tx-send tx-framing tx-framing-free tx-framing-menu tx-digital-framing tx-volume-device tx-volume-room tx-volume-mismatch tx-sending-mismatch settings-audio-mismatch problem-ptt problem-ptt-short speaker-many tx-chips tx-digital tpl-text tpl-font tpl-fill tpl-fill-stops tpl-fill-shadow tpl-color tpl-place tx-sending tx-sending-sample tx-digital-sending tx-digital-sending-device
run light 320 640 firstrun-1 idle receiving tx-picture tx-framing tx-framing-free tx-framing-menu tx-digital-framing tx-volume-device tx-volume-room tx-volume-mismatch tx-sending-mismatch settings-audio-mismatch problem-ptt problem-ptt-short speaker-many tx-chips tx-digital tpl-text tpl-font tpl-style tpl-fill tpl-fill-stops tpl-fill-shadow tpl-color level-steady rx-modes-fixed tx-sending tx-digital-sending tx-digital-sending-device
QSSTV_QML_FONTSCALE=1.3 run light 411 800 receiving level-steady rx-mode-fixed rx-modes tx-chips tx-framing tx-volume-device tx-volume-room tx-volume-mismatch tx-sending-mismatch problem-ptt problem-ptt-short speaker-many tx-digital tx-sending tx-digital-sending tx-digital-sending-device   # a large text size: the meter is taller and its words wrap
run light 850 411 firstrun-1 idle receiving rx-modes tx-picture tx-send tx-framing tx-digital-framing tx-volume-room problem-ptt speaker-many gallery gallery-viewer settings settings-audio firstrun-2 tpl-editor tpl-text tpl-font tpl-fill tpl-fill-stops tpl-color tpl-add cal-overview-done cal-ntp-good drm-profile freq freq-edit
run light 1280 800 receiving tx-picture tx-send   # a tablet on its side: the wide layout with room to spare
run light 800 1200 idle receiving tx-picture gallery gallery-viewer settings settings-audio firstrun-2 tpl-editor tpl-style tpl-layers cal-ntp-good cal-sstv-got drm-profiles freq rx-freq-sheet
echo "screenshots: $OUT"
exit $rc
