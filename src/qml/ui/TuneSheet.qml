import QtQuick
import QSSTV

// The frequencies for the mode the program is in; a tap tunes the radio to one
BottomSheet {
    id: sheet
    title: qsTr("Tune the radio")
    onOpenChanged: Frequencies.pickerOpen = open
    Connections { target: Frequencies; function onPickerChanged() { sheet.open = Frequencies.pickerOpen } }
    signal editList()
    property var items: []
    onVisibleChanged: if (visible) items = Frequencies.pickerEntries()

    Text {
        visible: sheet.items.length === 0
        width: parent.width
        text: AppModel.digital ? qsTr("There are no frequencies for digital pictures yet.") : qsTr("There are no frequencies for analog pictures yet.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
        leftPadding: 24; rightPadding: 24; bottomPadding: 12; wrapMode: Text.WordWrap
    }
    ListView {
        id: list
        width: parent.width
        height: Math.min(contentHeight, 5 * 64)
        interactive: contentHeight > height
        clip: true
        model: sheet.items
        delegate: Item {
            required property var modelData
            width: list.width; height: 64
            Rectangle { anchors.fill: parent; color: Theme.primaryContainer; visible: parent.modelData.current === true }
            Row {
                anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24; spacing: 12
                Text { anchors.verticalCenter: parent.verticalCenter; width: 120; text: modelData.freq; color: Theme.surfaceOn; font.pixelSize: Theme.sp(20); font.weight: Font.Medium }
                Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 120 - 12 - 32; text: modelData.text; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap }
                Icon { anchors.verticalCenter: parent.verticalCenter; visible: modelData.current === true; path: Icons.check; color: Theme.primary }
            }
            TapHandler { onTapped: Frequencies.tune(parent.modelData.index) }
            Accessible.role: Accessible.RadioButton
            Accessible.name: qsTr("%1 MHz, %2").arg(modelData.freq).arg(modelData.text)
            Accessible.checked: modelData.current === true
            Accessible.onPressAction: Frequencies.tune(modelData.index)
        }
    }
    PreferenceRow {
        icon: Icons.edit
        title: qsTr("Edit the list")
        onClicked: { sheet.open = false; sheet.editList() }
    }
    Item { width: 1; height: 12 }
}
