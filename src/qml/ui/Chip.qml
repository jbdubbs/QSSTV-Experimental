import QtQuick
import QtQuick.Layouts

// An assist/filter chip: text with an optional icon. tone: "plain", "selected", "good", "warn"
Rectangle {
    id: chip
    property string text
    property string icon
    property string tone: "plain"
    signal clicked()
    implicitHeight: 32
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
        Icon { visible: chip.icon !== ""; path: chip.icon; color: chip.content; size: 18; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: chip.text
            color: chip.content
            font.pixelSize: Theme.sp(13)
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    TapHandler { onTapped: chip.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: text
}
