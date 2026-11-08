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
        id: card
        objectName: "choiceCard"
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, 360)
        // as tall as its content, but inside the window: more options than fit scroll between the title and Cancel (#184)
        height: Math.min(parent.height - 120, header.height + options.implicitHeight + 8 + footer.height)
        radius: 28
        color: Theme.surfaceContainer
        MouseArea { anchors.fill: parent }
        Text {
            id: header
            text: dlg.title
            color: Theme.surfaceOn
            font.pixelSize: Theme.sp(22)
            topPadding: 24; leftPadding: 24; bottomPadding: 12
            width: parent.width
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }
        Flickable {
            id: list
            objectName: "choiceList"
            anchors.top: header.bottom; anchors.bottom: footer.top
            anchors.left: parent.left; anchors.right: parent.right
            clip: true
            contentWidth: width
            contentHeight: options.implicitHeight + 8
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}
            Column {
                id: options
                width: list.width
                Repeater {
                    model: dlg.options
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: options.width; height: Math.max(56, labels.implicitHeight + 16)
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
            }
        }
        Item {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width; height: 52
            Text {
                objectName: "choiceCancel"
                anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Cancel"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                TapHandler { margin: Theme.tapMargin(parent); onTapped: dlg.open = false }
                Accessible.onPressAction: { dlg.open = false }
                Accessible.role: Accessible.Button
            }
        }
    }
    Accessible.role: Accessible.Dialog
    Accessible.name: title
}
