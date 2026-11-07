import QtQuick

// The input level with its good zone, said in words, and what to do when it is not good
Rectangle {
    id: card
    property real level: 0
    property real zoneStart: 0.35
    property real zoneEnd: 0.85
    property string word
    property string hint
    property bool active: true
    color: Theme.surfaceContainer
    radius: Theme.radius
    implicitHeight: col.implicitHeight + 24
    Column {
        id: col
        x: 14; y: 12
        width: parent.width - 28
        spacing: 8
        Item {
            width: parent.width
            height: title.implicitHeight
            Text { id: title; text: qsTr("Input level"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
            Text {
                anchors.right: parent.right
                text: card.active ? card.word : qsTr("Not listening")
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(14)
                font.weight: Font.Medium
            }
        }
        Rectangle {
            id: track
            width: parent.width; height: 12; radius: 6
            color: Theme.outlineVariant
            clip: true
            Rectangle { x: card.zoneStart * parent.width; width: (card.zoneEnd - card.zoneStart) * parent.width; height: parent.height; color: Theme.goodContainer }
            Rectangle { width: Math.max(6, card.level * parent.width); height: parent.height; radius: 6; color: Theme.primary
                Behavior on width { NumberAnimation { duration: 90 } } }
            Rectangle { x: card.level * parent.width - 1; y: 0; width: 2; height: parent.height; color: Theme.surfaceOn; visible: card.active }
        }
        Item {
            width: parent.width; height: lab1.implicitHeight
            Text { id: lab1; text: qsTr("Too quiet"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
            Text { anchors.horizontalCenter: parent.horizontalCenter; x: (card.zoneStart + card.zoneEnd) / 2 * parent.width - width / 2; text: qsTr("Good"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
            Text { anchors.right: parent.right; text: qsTr("Too loud"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
        }
        Row {
            visible: card.hint !== ""
            spacing: 6
            width: parent.width
            Icon { path: Icons.warning; size: 18; color: Theme.warn; anchors.verticalCenter: parent.verticalCenter }
            Text {
                width: parent.width - 24
                text: card.hint
                color: Theme.warn
                font.pixelSize: Theme.sp(13)
                wrapMode: Text.WordWrap
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
    Accessible.role: Accessible.ProgressBar
    Accessible.name: qsTr("Input level: %1").arg(word)
}
