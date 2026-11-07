import QtQuick
import QSSTV

// The template that goes on the picture: choose another, change this one, or make a new one
BottomSheet {
    id: sheet
    title: qsTr("Template")
    signal newTemplate()
    property var items: []
    onOpenChanged: if (open) items = TxController.templates()

    Text {
        visible: sheet.items.length === 0
        width: parent.width
        text: qsTr("There are no templates yet.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
        leftPadding: 24; rightPadding: 24; bottomPadding: 12
    }
    ListView {
        id: list
        width: parent.width
        height: Math.min(contentHeight, 4 * 72)
        interactive: contentHeight > height
        clip: true
        model: sheet.items
        delegate: Item {
            required property var modelData
            width: list.width; height: 72
            Rectangle { anchors.fill: parent; color: Theme.primaryContainer; visible: parent.modelData.current === true }
            Rectangle {
                id: thumb
                x: 24; anchors.verticalCenter: parent.verticalCenter
                width: 72; height: 56; radius: 8; color: Theme.surfaceHigh; clip: true
                Image { anchors.fill: parent; source: parent.parent.modelData.thumb; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false }
            }
            Text {
                anchors.left: thumb.right; anchors.leftMargin: 16
                anchors.right: tick.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.name; elide: Text.ElideRight
                color: Theme.surfaceOn; font.pixelSize: Theme.sp(16)
            }
            Icon { id: tick; visible: modelData.current === true; path: Icons.check; color: Theme.primary
                   anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter }
            TapHandler { onTapped: { TxController.chooseTemplate(parent.modelData.path); sheet.open = false } }
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.name
            Accessible.checked: modelData.current === true
            Accessible.onPressAction: { TxController.chooseTemplate(modelData.path); sheet.open = false }
        }
    }
    PreferenceRow {
        visible: TxController.templateFile !== ""
        icon: Icons.edit
        title: qsTr("Edit this template")
        summary: TxController.templateName
        onClicked: { sheet.open = false; TemplateEditor.openFile(TxController.templateFile) }
    }
    PreferenceRow {
        icon: Icons.add
        title: qsTr("New template")
        onClicked: { sheet.open = false; sheet.newTemplate() }
    }
    Item { width: 1; height: 12 }
}
