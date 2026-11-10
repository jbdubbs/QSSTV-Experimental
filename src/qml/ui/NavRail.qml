import QtQuick

// The navigation rail of a wide window (tablet, phone on its side): the same three destinations at the left edge
Rectangle {
    id: rail
    property real leftInset: 0
    property real topPadding: 0
    color: Theme.surfaceContainer
    implicitWidth: Theme.railWidth + leftInset
    Column {
        x: rail.leftInset
        width: Theme.railWidth
        topPadding: 24 + rail.topPadding
        spacing: 14
        Repeater {
            model: [{l: qsTr("Receive"), i: Icons.mic}, {l: qsTr("Transmit"), i: Icons.send}, {l: qsTr("Gallery"), i: Icons.image}]
            delegate: NavItem {
                required property int index
                required property var modelData
                width: Theme.railWidth
                label: modelData.l
                icon: modelData.i
                selected: AppModel.screen === index
                onClicked: AppModel.screen = index
            }
        }
    }
}
