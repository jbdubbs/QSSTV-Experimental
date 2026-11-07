import QtQuick
import QSSTV

// "All modes": every analog mode, grouped by how long it takes, with its size and time
BottomSheet {
    id: sheet
    title: qsTr("All modes")
    ListView {
        id: list
        width: parent.width
        height: Math.min(contentHeight, sheet.height * 0.6)
        clip: true
        model: sheet.open ? TxController.allModes() : []
        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Text {
            required property string section
            text: section
            color: Theme.primary
            font.pixelSize: Theme.sp(13)
            font.weight: Font.Medium
            leftPadding: 24; topPadding: 12; bottomPadding: 4
        }
        delegate: Item {
            required property var modelData
            width: list.width
            height: 56
            readonly property bool selected: modelData.mode === TxController.mode
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
            TapHandler { margin: Theme.tapMargin(parent); onTapped: { TxController.mode = modelData.mode; sheet.open = false } }
            Accessible.onPressAction: { { TxController.mode = modelData.mode; sheet.open = false } }
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.name + ", " + modelData.size + ", " + modelData.time
            Accessible.checked: selected
        }
    }
    Item { width: 1; height: 12 }
}
