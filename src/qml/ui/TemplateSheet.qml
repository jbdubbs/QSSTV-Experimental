import QtQuick
import QtQuick.Effects
import QSSTV

// The template that goes on the picture: choose another, change this one, or make a new one
BottomSheet {
    id: sheet
    title: qsTr("Template")
    signal newTemplate()
    property var items: []
    onOpenChanged: if (open) items = TxController.templates()

    // no template: the picture goes as it is
    Item {
        id: noneRow
        width: parent.width; height: 72
        readonly property bool chosen: !TxController.templateOn
        Rectangle { anchors.fill: parent; color: Theme.primaryContainer; visible: noneRow.chosen }
        Rectangle {
            id: noneTile
            x: 24; anchors.verticalCenter: parent.verticalCenter
            width: 72; height: 56; radius: 8; color: Theme.surfaceHigh
            Icon { path: Icons.close; color: Theme.surfaceVariantOn; anchors.centerIn: parent }
        }
        Text {
            anchors.left: noneTile.right; anchors.leftMargin: 16
            anchors.right: noneTick.left; anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("None"); elide: Text.ElideRight
            color: Theme.surfaceOn; font.pixelSize: Theme.sp(16)
        }
        Icon { id: noneTick; visible: noneRow.chosen; path: Icons.check; color: Theme.primary
               anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter }
        TapHandler { onTapped: { TxController.useTemplate = false; sheet.open = false } }
        Accessible.role: Accessible.RadioButton
        Accessible.name: qsTr("None")
        Accessible.checked: noneRow.chosen
        Accessible.onPressAction: { TxController.useTemplate = false; sheet.open = false }
    }
    Text {
        visible: sheet.items.length === 0
        width: parent.width
        text: qsTr("There are no templates yet.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
        leftPadding: 24; rightPadding: 24; bottomPadding: 12; topPadding: 12
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
            readonly property bool chosen: modelData.current === true && TxController.templateOn   // with None chosen no template is ticked
            Rectangle { anchors.fill: parent; color: Theme.primaryContainer; visible: parent.chosen }
            Rectangle {
                id: thumb
                x: 24; anchors.verticalCenter: parent.verticalCenter
                width: 72; height: 56; radius: 8; color: Theme.surfaceHigh; clip: true
                Image { anchors.fill: parent; source: parent.parent.modelData.thumb; fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false
                       layer.enabled: Theme.tint; layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "#e8341f" } }
            }
            Text {
                anchors.left: thumb.right; anchors.leftMargin: 16
                anchors.right: tick.left; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.name; elide: Text.ElideRight
                color: Theme.surfaceOn; font.pixelSize: Theme.sp(16)
            }
            Icon { id: tick; visible: parent.chosen; path: Icons.check; color: Theme.primary
                   anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter }
            TapHandler { onTapped: { TxController.chooseTemplate(parent.modelData.path); sheet.open = false } }
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.name
            Accessible.checked: chosen
            Accessible.onPressAction: { TxController.chooseTemplate(modelData.path); sheet.open = false }
        }
    }
    PreferenceRow {
        visible: TxController.templateFile !== "" && TxController.templateOn   // with None there is no template to change
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
