import QtQuick
import QtQuick.Controls.Material

// One entry of the menu of the top bar, in the colors of the look
MenuItem {
    id: item
    implicitHeight: 48
    contentItem: Text {
        text: item.text
        color: item.enabled ? Theme.surfaceOn : Theme.surfaceVariantOn
        font.pixelSize: Theme.sp(16)
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle { color: item.highlighted || item.down ? Theme.surfaceHigh : "transparent" }
}
