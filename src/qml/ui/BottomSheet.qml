import QtQuick
import QtQuick.Controls

// A modal bottom sheet: the content slides up over a scrim. open it with open = true; Back and a tap on the scrim close it.
Item {
    id: sheet
    property bool open: false
    property string title
    default property alias content: body.data
    parent: Overlay.overlay          // over the whole window, the navigation bar too
    anchors.fill: parent
    visible: open || panel.y < height
    z: 10

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        opacity: sheet.open ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.ms(180) } }
        MouseArea { anchors.fill: parent; onClicked: sheet.open = false }
    }
    Rectangle {
        id: panel
        width: parent.width
        height: Math.min(parent.height * 0.85, column.implicitHeight + 24)
        y: sheet.open ? parent.height - height : parent.height
        Behavior on y { NumberAnimation { duration: Theme.ms(220); easing.type: Easing.OutCubic } }
        color: Theme.surfaceContainer
        radius: 28
        Rectangle { width: parent.width; height: parent.radius; y: parent.height - height; color: parent.color }   // square at the bottom
        MouseArea { anchors.fill: parent }   // taps inside are not taps on the scrim
        Column {
            id: column
            width: parent.width
            topPadding: 8
            Rectangle { width: 32; height: 4; radius: 2; color: Theme.outline; anchors.horizontalCenter: parent.horizontalCenter }
            Text {
                visible: sheet.title !== ""
                text: sheet.title
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(18)
                leftPadding: 24; topPadding: 14; bottomPadding: 8
            }
            Column { id: body; width: parent.width }
        }
    }
    Accessible.role: Accessible.Dialog
    Accessible.name: title
}
