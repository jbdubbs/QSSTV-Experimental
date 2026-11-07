import QtQuick
import QtQuick.Controls.Material

// A settings row with a slider: the title and the value on one line, the slider under it; the new value is kept when the finger lets go
Item {
    id: row
    property string title
    property string summary
    property string valueText
    property alias from: slider.from
    property alias to: slider.to
    property alias stepSize: slider.stepSize
    property real value: 0
    signal committed(real value)
    signal moved(real value)          //!< while the finger is still on the slider
    width: parent ? parent.width : 360
    implicitHeight: head.implicitHeight + slider.implicitHeight + (note.visible ? note.implicitHeight : 0) + 20
    Item {
        id: head
        x: 24; y: 8; width: parent.width - 48
        implicitHeight: Math.max(24, titleText.implicitHeight)
        Text { id: titleText; text: row.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); anchors.left: parent.left; anchors.right: valueLabel.left; anchors.rightMargin: 12; wrapMode: Text.WordWrap }
        Text { id: valueLabel; text: row.valueText; color: Theme.primary; font.pixelSize: Theme.sp(15); font.weight: Font.Medium; anchors.right: parent.right }
    }
    Text {
        id: note
        visible: row.summary !== ""
        x: 24; y: head.y + head.height + 2; width: parent.width - 48
        text: row.summary; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap
    }
    Slider {
        id: slider
        x: 12; width: parent.width - 24
        y: (note.visible ? note.y + note.height : head.y + head.height) + 2
        Material.accent: Theme.primary
        value: row.value
        onMoved: row.moved(value)
        onPressedChanged: if (!pressed) row.committed(value)
        Accessible.name: row.title
    }
    Binding { target: slider; property: "value"; value: row.value; when: !slider.pressed }
}
