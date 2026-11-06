import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QSSTV

ApplicationWindow {
    id: win
    visible: true
    width: 411
    height: 800
    title: "QSSTV-Experimental"
    color: Theme.surface
    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.accent: Theme.primary
    font.family: Theme.fontFamily

    readonly property bool wide: width >= 600

    Item {
        id: root
        anchors.fill: parent
        readonly property var inset: SafeArea.margins

        readonly property string title: AppModel.screen === 0 ? qsTr("Receive") : AppModel.screen === 1 ? qsTr("Transmit") : qsTr("Gallery")

        // the status bar's room, the top bar, the screen
        Column {
            id: column
            anchors.top: parent.top
            anchors.topMargin: root.inset.top
            anchors.left: win.wide ? rail.right : parent.left
            anchors.right: parent.right
            anchors.rightMargin: root.inset.right
            anchors.bottom: win.wide ? parent.bottom : nav.top
            AppBar {
                id: bar
                width: parent.width
                title: root.title
                routeText: AppModel.routeText
                actions: [
                    Item {
                        visible: AppModel.screen === 0
                        width: Theme.tap; height: Theme.tap
                        Icon { anchors.centerIn: parent; path: Icons.tune }
                        TapHandler { onTapped: receive.optionsOpen = true }
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Receive options")
                    }
                ]
            }
            StackLayout {
                width: parent.width
                height: parent.height - bar.height
                currentIndex: AppModel.screen
                ReceiveScreen { id: receive; objectName: "receiveScreen" }
                TransmitScreen { id: transmit; objectName: "transmitScreen" }
                PlaceholderScreen { title: qsTr("Gallery") }
            }
        }
        SendingOverlay {
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
        }
        NavBar {
            id: nav
            visible: !win.wide
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            bottomInset: root.inset.bottom
        }
        NavRail {
            id: rail
            visible: win.wide
            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
            leftInset: root.inset.left
            topPadding: root.inset.top
        }
    }

    onClosing: (close) => {
        if (Qt.platform.os === "android") {
            close.accepted = false
            if (TxController.sending) TxController.stop()
            else if (receive.optionsOpen) receive.optionsOpen = false
            else if (transmit.modesOpen) transmit.modesOpen = false
            else if (transmit.detailsOpen) transmit.detailsOpen = false
            else if (AppModel.screen !== 0) AppModel.screen = 0
            else AppModel.moveToBackground()
        } else {
            close.accepted = false
            AppModel.quit()
        }
    }
}
