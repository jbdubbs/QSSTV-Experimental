import QtQuick
import QSSTV

// Gallery: received, sent, templates and stock pictures, newest first, as thumbnails under their date
Item {
    id: screen
    objectName: "galleryScreen"
    function harnessStarters() { starters.open = true }   // the screenshots of the test harness

    SegmentedTabs {
        id: tabs
        x: Theme.margin; y: 2
        width: parent.width - 2 * Theme.margin
        model: [qsTr("Received"), qsTr("Sent"), qsTr("Templates")]
        currentIndex: GalleryModel.tab
        onPicked: (i) => GalleryModel.tab = i
    }

    readonly property int gap: 4
    readonly property real cellSize: (list.width - (GalleryModel.columns + 1) * gap) / GalleryModel.columns
    Binding { target: GalleryModel; property: "columns"; value: Math.max(3, Math.floor(screen.width / 120)) }

    ListView {
        id: list
        anchors.top: tabs.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        clip: true
        model: GalleryModel
        boundsBehavior: Flickable.StopAtBounds
        bottomMargin: 16
        delegate: Item {
            id: row
            required property int kind
            required property string title
            required property var cells
            width: list.width
            height: kind === 0 ? 44 : screen.cellSize + screen.gap
            Text {
                visible: row.kind === 0
                x: Theme.margin; anchors.bottom: parent.bottom; anchors.bottomMargin: 6
                text: row.title
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                Accessible.role: Accessible.Heading
                Accessible.name: row.title
            }
            Row {
                visible: row.kind === 1
                x: screen.gap
                spacing: screen.gap
                Repeater {
                    model: row.kind === 1 ? row.cells : []
                    delegate: ThumbCell {
                        required property var modelData
                        info: modelData
                        width: screen.cellSize; height: screen.cellSize
                    }
                }
            }
        }
    }

    // nothing here yet
    Column {
        visible: GalleryModel.count === 0
        anchors.centerIn: parent
        width: parent.width - 64
        spacing: 6
        Icon { anchors.horizontalCenter: parent.horizontalCenter; path: Icons.image; size: 48; color: Theme.outline }
        Text {
            width: parent.width; horizontalAlignment: Text.AlignHCenter
            text: GalleryModel.tab === 0 ? qsTr("No received pictures yet") : GalleryModel.tab === 1 ? qsTr("Nothing sent yet") : qsTr("No templates or pictures")
            color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); font.weight: Font.Medium
        }
        Text {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: GalleryModel.tab === 0 ? qsTr("Pictures you receive are saved here.") : GalleryModel.tab === 1 ? qsTr("Pictures you send are kept here.")
                                         : qsTr("Templates put your details on a picture. Pictures here can be sent again.")
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
        }
    }

    ExtendedFab {
        visible: GalleryModel.tab === 2 && !GalleryModel.selecting
        anchors.right: parent.right; anchors.rightMargin: 16
        anchors.bottom: parent.bottom; anchors.bottomMargin: 16
        text: qsTr("New template"); icon: Icons.add
        onClicked: starters.open = true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("New template")
    }
    BottomSheet {
        id: starters
        title: qsTr("Start a new template")
        Repeater {
            model: TemplateEditor.starterNames()
            delegate: PreferenceRow {
                required property string modelData
                required property int index
                icon: index === 0 ? Icons.rectItem : index === 1 ? Icons.barItem : index === 2 ? Icons.textItem : Icons.replayItem
                title: modelData
                summary: index === 0 ? qsTr("Add everything yourself") : index === 1 ? qsTr("A bar along the bottom with your call sign")
                       : index === 2 ? qsTr("Your call sign in white letters with a dark edge") : qsTr("Room for the picture you reply to")
                onClicked: { starters.open = false; TemplateEditor.newTemplate(index) }
            }
        }
        Item { width: 1; height: 12 }
    }
}
