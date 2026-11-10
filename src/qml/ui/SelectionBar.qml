import QtQuick
import QSSTV

// The top bar while pictures are selected: how many, and what can be done with them
Rectangle {
    id: bar
    color: Theme.primaryContainer
    implicitHeight: Theme.appBarHeight
    readonly property int count: GalleryModel.selectedCount
    Item {
        x: 4; width: Theme.tap; height: Theme.tap; anchors.verticalCenter: parent.verticalCenter
        Icon { anchors.centerIn: parent; path: Icons.close; color: Theme.primaryContainerOn }
        TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.clearSelection() }
        Accessible.onPressAction: { GalleryModel.clearSelection() }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Stop selecting")
    }
    Text {
        x: 64; anchors.verticalCenter: parent.verticalCenter
        text: qsTr("%1 selected").arg(bar.count)
        color: Theme.primaryContainerOn
        font.pixelSize: Theme.sp(20)
        Accessible.role: Accessible.Heading
    }
    Row {
        anchors.right: parent.right; anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        Item {
            width: Theme.tap; height: Theme.tap
            Text { anchors.centerIn: parent; text: qsTr("All"); color: Theme.primaryContainerOn; font.pixelSize: Theme.sp(14); font.weight: Font.Medium }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.selectAll() }
            Accessible.onPressAction: { GalleryModel.selectAll() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Select all")
        }
        Item {
            width: Theme.tap; height: Theme.tap
            Icon { anchors.centerIn: parent; path: Icons.send; color: Theme.primaryContainerOn }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.sendSelected() }
            Accessible.onPressAction: { GalleryModel.sendSelected() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Send by radio")
        }
        Item {
            visible: bar.count === 1
            width: visible ? Theme.tap : 0; height: Theme.tap
            Icon { anchors.centerIn: parent; path: Icons.share; color: Theme.primaryContainerOn }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.shareSelected() }
            Accessible.onPressAction: { GalleryModel.shareSelected() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Share")
        }
        Item {
            width: Theme.tap; height: Theme.tap
            Icon { anchors.centerIn: parent; path: Icons.remove; color: Theme.primaryContainerOn }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.deleteSelected() }
            Accessible.onPressAction: { GalleryModel.deleteSelected() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Delete")
        }
    }
}
