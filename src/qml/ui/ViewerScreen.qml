import QtQuick
import QSSTV

// A picture of the gallery on the whole screen: swipe to the next, pinch or double-tap to zoom, and what can be done with it
Rectangle {
    id: viewer
    color: "black"
    z: 50
    visible: GalleryModel.viewerIndex >= 0
    property real topInset: 0
    property real bottomInset: 0
    property bool chrome: true
    property alias infoOpen: infoSheet.open
    // the revision is used (not just read): the compiler drops a statement whose value is not used, and the list would never be asked again
    property var pictures: GalleryModel.revision < 0 ? [] : GalleryModel.items()
    readonly property var current: GalleryModel.viewerIndex >= 0 && GalleryModel.viewerIndex < pictures.length ? pictures[GalleryModel.viewerIndex] : null
    MouseArea { anchors.fill: parent }

    ListView {
        id: pager
        anchors.fill: parent
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        boundsBehavior: Flickable.StopAtBounds
        interactive: !(currentItem && currentItem.zoomed)
        model: viewer.visible ? viewer.pictures : []
        cacheBuffer: width
        delegate: ZoomImage {
            required property var modelData
            width: pager.width; height: pager.height
            source: modelData.full
            onTapped: viewer.chrome = !viewer.chrome
        }
        // the finger moved the pager to another picture: that is the one now shown (a reset of the model must not count)
        onMovementEnded: if (currentIndex >= 0 && currentIndex !== GalleryModel.viewerIndex) GalleryModel.viewerIndex = currentIndex
    }
    // the pager follows the model: it starts at the picture that was tapped, and keeps its place when the list changes
    function syncPager() {
        const i = GalleryModel.viewerIndex
        if (i < 0 || i >= pager.count) return
        pager.currentIndex = i
        pager.positionViewAtIndex(i, ListView.SnapPosition)
    }
    Connections {
        target: GalleryModel
        function onViewerChanged() { viewer.syncPager() }
        function onItemsChanged() { Qt.callLater(viewer.syncPager) }
    }
    onVisibleChanged: if (visible) { chrome = true; Qt.callLater(syncPager) }

    // the bar on top: back, the name and when
    Rectangle {
        id: top
        visible: viewer.chrome
        width: parent.width; height: Theme.appBarHeight + viewer.topInset
        gradient: Gradient { GradientStop { position: 0; color: "#b3000000" } GradientStop { position: 1; color: "#00000000" } }
        Item {
            x: 4; y: viewer.topInset; width: Theme.tap; height: Theme.appBarHeight
            Icon { anchors.centerIn: parent; path: Icons.back; color: Theme.onPicture }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: GalleryModel.closeViewer() }
            Accessible.onPressAction: { GalleryModel.closeViewer() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back to the gallery")
        }
        Column {
            x: 64; y: viewer.topInset + 6
            width: parent.width - 80
            Text { width: parent.width; text: viewer.current ? (viewer.current.badge !== "" ? viewer.current.badge + " · " : "") + viewer.current.name : ""; color: Theme.onPicture; font.pixelSize: Theme.sp(16); font.weight: Font.Medium; elide: Text.ElideRight }
            Text { width: parent.width; text: viewer.current ? viewer.current.when : ""; color: Theme.onPictureVariant; font.pixelSize: Theme.sp(12) }
        }
    }

    // the actions at the thumb
    Rectangle {
        id: bottom
        visible: viewer.chrome
        anchors.bottom: parent.bottom
        width: parent.width; height: 72 + viewer.bottomInset
        gradient: Gradient { GradientStop { position: 0; color: "#00000000" } GradientStop { position: 1; color: "#b3000000" } }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 8
            spacing: 8
            Repeater {
                model: [
                    {id: "share", text: qsTr("Share"), icon: Icons.share, show: viewer.current && !viewer.current.isTemplate},
                    {id: "send", text: viewer.current && viewer.current.isTemplate ? qsTr("Use template") : qsTr("Send by radio"), icon: Icons.send, show: true},
                    {id: "edit", text: qsTr("Edit"), icon: Icons.edit, show: viewer.current && viewer.current.isTemplate},
                    {id: "info", text: qsTr("Info"), icon: Icons.info, show: true},
                    {id: "delete", text: qsTr("Delete"), icon: Icons.remove, show: true}
                ]
                delegate: Item {
                    required property var modelData
                    visible: modelData.show
                    width: visible ? 88 : 0; height: 56
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Icon { anchors.horizontalCenter: parent.horizontalCenter; path: modelData.icon; color: Theme.onPicture }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.text; color: Theme.onPicture; font.pixelSize: Theme.sp(12) }
                    }
                    TapHandler { margin: Theme.tapMargin(parent);
                        onTapped: {
                            var i = GalleryModel.viewerIndex
                            if (modelData.id === "share") GalleryModel.shareIndex(i)
                            else if (modelData.id === "send") GalleryModel.sendIndex(i)
                            else if (modelData.id === "info") infoSheet.open = true
                            else if (modelData.id === "edit") TemplateEditor.openFile(viewer.current.path)
                            else GalleryModel.deleteIndex(i)
                        }
                    }
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.text
                    Accessible.onPressAction: {
                        var i = GalleryModel.viewerIndex
                        if (modelData.id === "share") GalleryModel.shareIndex(i)
                        else if (modelData.id === "send") GalleryModel.sendIndex(i)
                        else if (modelData.id === "info") infoSheet.open = true
                        else if (modelData.id === "edit") TemplateEditor.openFile(viewer.current.path)
                        else GalleryModel.deleteIndex(i)
                    }
                }
            }
        }
    }

    BottomSheet {
        id: infoSheet
        title: qsTr("Details")
        Column {
            width: parent.width
            Repeater {
                model: infoSheet.open ? GalleryModel.info(GalleryModel.viewerIndex) : []
                delegate: Item {
                    required property var modelData
                    width: infoSheet.width; height: Math.max(48, col.implicitHeight + 16)
                    Column {
                        id: col
                        x: 24; width: parent.width - 48
                        anchors.verticalCenter: parent.verticalCenter
                        Text { text: modelData.label; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                        Text { width: parent.width; text: modelData.value; color: Theme.surfaceOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WrapAnywhere }
                    }
                }
            }
            Item { width: 1; height: 12 }
        }
    }
}
