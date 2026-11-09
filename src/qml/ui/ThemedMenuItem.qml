import QtQuick
import QtQuick.Controls.Material

// One entry of a menu, in the colors of the look. markColumn keeps room before the words for a check, and marked draws it (a menu that lists
// the choices of one setting: the chosen one is checked); the menu of the top bar has neither.
MenuItem {
    id: item
    property bool marked: false
    property bool markColumn: false
    implicitHeight: 48
    contentItem: Item {
        readonly property real lead: item.markColumn ? 28 : 0
        implicitWidth: lead + label.implicitWidth
        implicitHeight: label.implicitHeight
        Icon {
            visible: item.markColumn && item.marked
            path: Icons.check; color: Theme.primary; size: 18
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: label
            x: parent.lead
            width: parent.width - parent.lead
            height: parent.height
            text: item.text
            color: item.enabled ? Theme.surfaceOn : Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(16)
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
    background: Rectangle { color: item.highlighted || item.down ? Theme.surfaceHigh : "transparent" }
}
