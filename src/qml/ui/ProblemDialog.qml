import QtQuick
import QtQuick.Controls

// Something went wrong and the person has to know: a title, what happened, one button
Item {
    id: dlg
    property bool open: false
    property string title
    property string text
    function show(t, m) { title = t; text = m; open = true }
    parent: Overlay.overlay
    anchors.fill: parent
    visible: open
    z: 30

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent }
    }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, 360)
        height: Math.min(parent.height - 120, col.implicitHeight)
        radius: 28
        color: Theme.surfaceContainer
        Column {
            id: col
            width: parent.width
            topPadding: 24
            Icon { x: 24; path: Icons.warning; color: Theme.warn; size: 28 }
            Text {
                text: dlg.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(22)
                leftPadding: 24; rightPadding: 24; topPadding: 12; bottomPadding: 8; width: parent.width; wrapMode: Text.WordWrap
                Accessible.role: Accessible.Heading
            }
            Text {
                text: dlg.text; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
                leftPadding: 24; rightPadding: 24; bottomPadding: 12; width: parent.width; wrapMode: Text.WordWrap
            }
            Item {
                width: parent.width; height: 56
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("OK"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                    padding: 8
                    TapHandler { onTapped: dlg.open = false }
                    Accessible.role: Accessible.Button
                }
            }
        }
    }
    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
}
