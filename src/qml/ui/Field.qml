import QtQuick
import QtQuick.Templates as T

// A text field in the colors of the look: an outlined box, the outline thicker and in the main color when the field has the focus
T.TextField {
    id: control
    implicitWidth: 200
    implicitHeight: 56
    leftPadding: 16; rightPadding: 16; topPadding: 8; bottomPadding: 8
    verticalAlignment: TextInput.AlignVCenter
    font.pixelSize: Theme.sp(16)
    color: Theme.surfaceOn
    selectionColor: Theme.primary
    selectedTextColor: Theme.primaryOn
    placeholderTextColor: Theme.surfaceVariantOn
    opacity: enabled ? 1 : 0.5
    background: Rectangle {
        radius: 4
        color: "transparent"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.primary : Theme.outline
    }
    Text {
        x: control.leftPadding; y: control.topPadding
        width: control.width - control.leftPadding - control.rightPadding; height: control.height - control.topPadding - control.bottomPadding
        verticalAlignment: Text.AlignVCenter
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        elide: Text.ElideRight
        visible: !control.length && !control.preeditText
    }
}
