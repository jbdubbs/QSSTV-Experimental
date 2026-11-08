import QtQuick
import QSSTV

// "All modes": every analog mode, grouped by how long it takes, with its size and time. On Transmit it chooses the mode to send
// (the default); on Receive (withAuto) it starts with "Auto" and chooses the mode the receiver looks for (-1 = Auto)
BottomSheet {
    id: sheet
    title: qsTr("All modes")
    property int current: TxController.mode    // the mode shown as chosen
    property bool withAuto: false               // a first row "Auto" (mode -1)
    signal picked(int mode)
    ListView {
        id: list
        width: parent.width
        height: Math.min(contentHeight, sheet.height * 0.6)
        clip: true
        model: sheet.open ? (sheet.withAuto ? [{mode: -1, name: qsTr("Auto"), size: qsTr("Detects the mode from the signal"), time: "", group: ""}].concat(TxController.allModes())
                                            : TxController.allModes()) : []
        // the chosen mode in view: the list is long and the chosen one is often below the fold
        onCountChanged: {
            if (count === 0) return
            const i = model.findIndex(e => e.mode === sheet.current)
            if (i > 0) positionViewAtIndex(i, ListView.Center)
        }
        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section
            text: section
            height: section === "" ? 0 : implicitHeight   // the "Auto" row has no heading
            color: Theme.primary
            font.pixelSize: Theme.sp(13)
            font.weight: Font.Medium
            leftPadding: 24; topPadding: 12; bottomPadding: 4
        }
        delegate: Item {
            required property var modelData
            width: list.width
            height: 56
            readonly property bool selected: modelData.mode === sheet.current
            Rectangle {
                id: radio
                x: 24; anchors.verticalCenter: parent.verticalCenter
                width: 20; height: 20; radius: 10
                color: "transparent"
                border.width: 2
                border.color: parent.selected ? Theme.primary : Theme.outline
                Rectangle { visible: parent.parent.selected; anchors.centerIn: parent; width: 10; height: 10; radius: 5; color: Theme.primary }
            }
            Column {
                anchors.left: radio.right; anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                Text { text: modelData.name; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16) }
                Text { text: modelData.size; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
            }
            Text {
                anchors.right: parent.right; anchors.rightMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.time
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(14)
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: { sheet.picked(modelData.mode); sheet.open = false } }
            Accessible.onPressAction: { sheet.picked(modelData.mode); sheet.open = false }
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.name + ", " + modelData.size + (modelData.time !== "" ? ", " + modelData.time : "")
            Accessible.checked: selected
        }
    }
    Item { width: 1; height: 12 }
}
