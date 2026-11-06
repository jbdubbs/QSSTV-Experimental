import QtQuick
import QtQuick.Layouts

// The bottom navigation bar of a narrow window: Receive, Transmit, Gallery. bottomInset: room for the system's gesture bar.
Rectangle {
    id: nav
    property real bottomInset: 0
    color: Theme.surfaceContainer
    implicitHeight: Theme.navBarHeight + bottomInset
    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 12
        anchors.bottomMargin: 16 + nav.bottomInset
        spacing: 0
        Repeater {
            model: [{l: qsTr("Receive"), i: Icons.mic}, {l: qsTr("Transmit"), i: Icons.send}, {l: qsTr("Gallery"), i: Icons.image}]
            delegate: NavItem {
                required property int index
                required property var modelData
                Layout.fillWidth: true
                Layout.fillHeight: true
                label: modelData.l
                icon: modelData.i
                selected: AppModel.screen === index
                onClicked: AppModel.screen = index
            }
        }
    }
}
