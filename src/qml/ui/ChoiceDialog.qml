import QtQuick
import QtQuick.Controls.Material

// Android's single-choice dialog: a title and the options with radio buttons; a tap on an option applies it and closes the dialog
Item {
    id: dlg
    property bool open: false
    property string title
    property var options: []          // [{text, hint}] or plain strings
    property int current: -1
    signal picked(int index)
    parent: Overlay.overlay
    anchors.fill: parent
    visible: open
    z: 20

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent; onClicked: dlg.open = false }
    }
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, 360)
        height: Math.min(parent.height - 120, col.implicitHeight)
        radius: 28
        color: Theme.surfaceContainer
        MouseArea { anchors.fill: parent }
        Column {
            id: col
            width: parent.width
            topPadding: 24; bottomPadding: 8
            Text {
                text: dlg.title
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(22)
                leftPadding: 24; bottomPadding: 12
                width: parent.width
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.Heading
            }
            Repeater {
                model: dlg.options
                delegate: Item {
                    required property var modelData
                    required property int index
                    width: col.width; height: Math.max(56, labels.implicitHeight + 16)
                    RadioMark { x: 20; anchors.verticalCenter: parent.verticalCenter; checked: index === dlg.current }
                    Column {
                        id: labels
                        x: 64; width: parent.width - 80
                        anchors.verticalCenter: parent.verticalCenter
                        Text { width: parent.width; text: typeof modelData === "string" ? modelData : modelData.text; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); wrapMode: Text.WordWrap }
                        Text { width: parent.width; visible: typeof modelData !== "string" && !!modelData.hint; text: typeof modelData === "string" ? "" : (modelData.hint || ""); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap }
                    }
                    TapHandler { margin: Theme.tapMargin(parent); onTapped: { dlg.picked(index); dlg.open = false } }
                    Accessible.onPressAction: { { dlg.picked(index); dlg.open = false } }
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: typeof modelData === "string" ? modelData : modelData.text
                    Accessible.checked: index === dlg.current
                }
            }
            Item {
                width: parent.width; height: 52
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Cancel"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                    TapHandler { margin: Theme.tapMargin(parent); onTapped: dlg.open = false }
                    Accessible.onPressAction: { dlg.open = false }
                    Accessible.role: Accessible.Button
                }
            }
        }
    }
    Accessible.role: Accessible.Dialog
    Accessible.name: title
}
