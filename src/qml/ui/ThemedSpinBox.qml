import QtQuick
import QtQuick.Templates as T

// A number with − and + at the sides that can also be typed, in the colors of the look
T.SpinBox {
    id: control
    implicitWidth: 168
    implicitHeight: 48
    editable: true
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    contentItem: TextInput {
        text: control.displayText
        font.pixelSize: Theme.sp(16)
        color: Theme.surfaceOn
        selectionColor: Theme.primary
        selectedTextColor: Theme.primaryOn
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }
    up.indicator: Item {
        x: control.mirrored ? 0 : parent.width - width
        width: 48; height: parent.height
        Text { anchors.centerIn: parent; text: "+"; font.pixelSize: Theme.sp(22); color: control.up.pressed ? Theme.surfaceOn : Theme.primary; opacity: enabled ? 1 : 0.4 }
    }
    down.indicator: Item {
        x: control.mirrored ? parent.width - width : 0
        width: 48; height: parent.height
        Text { anchors.centerIn: parent; text: "−"; font.pixelSize: Theme.sp(22); color: control.down.pressed ? Theme.surfaceOn : Theme.primary; opacity: enabled ? 1 : 0.4 }
    }
    background: Rectangle {
        radius: 8
        color: "transparent"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.primary : Theme.outline
    }
}
