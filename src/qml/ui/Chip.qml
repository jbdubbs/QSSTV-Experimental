import QtQuick
import QtQuick.Layouts

// An assist/filter chip: text with an optional icon. tone: "plain", "selected", "good", "warn"
Rectangle {
    id: chip
    property string text
    property string icon
    property string tone: "plain"
    property real maxWidth: 0   // 0: as wide as its words; else no wider than this, and the words are cut with "..." to fit
    property string fontFamily  // the words in this font (a chip that names a font shows it); empty: the app's font
    property bool dropdown: false   // a small arrow after the words: the chip opens a list
    signal clicked()
    implicitHeight: 32
    // the room the words may take; worked out from the width asked for, not from the width the chip gets, so that nothing loops
    readonly property real textRoom: maxWidth > 0 ? Math.max(0, maxWidth - 24 - (iconIcon.visible ? iconIcon.width + row.spacing : 0) - (arrowIcon.visible ? arrowIcon.width + row.spacing : 0)) : label.implicitWidth
    implicitWidth: row.implicitWidth + 24
    radius: 8
    color: tone === "selected" ? Theme.primaryContainer : tone === "good" ? Theme.goodContainer : tone === "warn" ? Theme.warnContainer : "transparent"
    border.width: tone === "plain" ? 1 : 0
    border.color: Theme.outline
    readonly property color content: tone === "selected" ? Theme.primaryContainerOn : tone === "good" ? Theme.good
                                     : tone === "warn" ? Theme.warn : Theme.surfaceVariantOn
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Icon { id: iconIcon; visible: chip.icon !== ""; path: chip.icon; color: chip.content; size: 18; anchors.verticalCenter: parent.verticalCenter }
        Text {
            id: label
            text: chip.text
            color: chip.content
            font.pixelSize: Theme.sp(13)
            font.weight: Font.Medium
            font.family: chip.fontFamily !== "" ? chip.fontFamily : Qt.application.font.family
            elide: Text.ElideRight
            width: Math.min(implicitWidth, chip.textRoom)
            anchors.verticalCenter: parent.verticalCenter
        }
        Icon { id: arrowIcon; visible: chip.dropdown; path: Icons.expand; color: chip.content; size: 18; anchors.verticalCenter: parent.verticalCenter }
    }
    TapHandler { margin: Theme.tapMargin(parent); onTapped: { chip.clicked(); AppModel.haptic(0) } }
    Accessible.onPressAction: { chip.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: text
}
