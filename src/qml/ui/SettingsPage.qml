import QtQuick
import QtQuick.Controls.Material

// One page of the settings: a bar with Back and the title, and the rows, which scroll
Item {
    id: page
    property string title
    property real topInset: 0
    property real bottomInset: 0
    default property alias rows: column.data
    property real contentWidth: 640        // a tablet does not stretch the rows over the whole screen
    property alias footer: footerItem.data  // buttons that stay at the bottom, under the rows
    property real footerHeight: 0
    signal back()

    Item {
        id: bar
        y: page.topInset
        width: parent.width; height: Theme.appBarHeight
        Item {
            x: 4; width: Theme.tap; height: parent.height
            Icon { anchors.centerIn: parent; path: Icons.back; color: Theme.surfaceOn }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: page.back() }
            Accessible.onPressAction: { page.back() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Back")
        }
        Text {
            x: 64; width: parent.width - 80; height: parent.height
            verticalAlignment: Text.AlignVCenter
            text: page.title
            color: Theme.surfaceOn
            font.pixelSize: Theme.sp(22)
            elide: Text.ElideRight
            Accessible.role: Accessible.Heading
            Accessible.name: page.title
        }
    }
    Item {
        id: footerItem
        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, page.contentWidth)
        height: page.footerHeight > 0 ? page.footerHeight + page.bottomInset : 0
    }
    Flickable {
        anchors.top: bar.bottom; anchors.bottom: footerItem.top
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width, page.contentWidth)
        contentHeight: column.implicitHeight + (page.footerHeight > 0 ? 0 : page.bottomInset) + 24
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollIndicator.vertical: ScrollIndicator {}
        Column { id: column; width: parent.width }
    }
}
