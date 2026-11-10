import QtQuick
import QtQuick.Controls.Material

// A question with buttons (and a text field when asked): Keep the changes? Name the template.
Item {
    id: dlg
    property bool open: false
    property string title
    property string text
    property bool input: false
    property string inputText
    property var buttons: []          // [{text, primary}] the last is the main one
    signal chosen(int index)
    parent: Overlay.overlay
    anchors.fill: parent
    visible: open
    z: 30
    onOpenChanged: {
        if (open && input) { field.text = inputText; Qt.callLater(function() { field.forceActiveFocus() }) }
        else if (!open) { field.focus = false; Qt.inputMethod.hide() }   // the keyboard goes with the dialog
    }

    Rectangle { anchors.fill: parent; color: Theme.scrim; MouseArea { anchors.fill: parent } }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.min(parent.height * 0.22, parent.height - height - 24)
        width: Math.min(parent.width - 56, 360)
        height: col.implicitHeight
        radius: 28
        color: Theme.surfaceContainer
        Column {
            id: col
            width: parent.width
            topPadding: 24
            Text { text: dlg.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(22); leftPadding: 24; rightPadding: 24; bottomPadding: 12; width: parent.width; wrapMode: Text.WordWrap; Accessible.role: Accessible.Heading }
            Text { visible: dlg.text !== ""; text: dlg.text; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); leftPadding: 24; rightPadding: 24; bottomPadding: 12; width: parent.width; wrapMode: Text.WordWrap }
            Field {
                id: field
                visible: dlg.input
                x: 24; width: parent.width - 48
                font.pixelSize: Theme.sp(16)
                color: Theme.surfaceOn
                Material.accent: Theme.primary
                onTextChanged: dlg.inputText = text
                Accessible.name: dlg.title
            }
            Item { width: 1; height: dlg.input ? 12 : 0 }
            Flow {
                x: 12; width: parent.width - 24; layoutDirection: Qt.RightToLeft; spacing: 4
                bottomPadding: 12
                Repeater {
                    model: dlg.buttons.length
                    delegate: Item {
                        required property int index
                        readonly property var b: dlg.buttons[dlg.buttons.length - 1 - index]
                        width: lab.implicitWidth + 32; height: 48
                        Rectangle { anchors.fill: parent; anchors.margins: 4; radius: 20; color: b.primary ? Theme.primary : "transparent" }
                        Text { id: lab; anchors.centerIn: parent; text: parent.b.text; color: parent.b.primary ? Theme.primaryOn : Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium }
                        TapHandler { onTapped: { dlg.open = false; dlg.chosen(dlg.buttons.length - 1 - parent.index) } }
                        Accessible.role: Accessible.Button
                        Accessible.name: b.text
                        Accessible.onPressAction: { dlg.open = false; dlg.chosen(dlg.buttons.length - 1 - index) }
                    }
                }
            }
        }
    }
    Accessible.role: Accessible.Dialog
    Accessible.name: title
}
