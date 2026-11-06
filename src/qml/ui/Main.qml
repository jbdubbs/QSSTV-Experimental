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
    // the window draws behind the system bars (edge to edge); the screens keep clear of them themselves, with SafeArea
    topPadding: 0
    leftPadding: 0
    rightPadding: 0
    bottomPadding: 0

    readonly property bool wide: width >= 600
    property alias aboutOpen: about.open

    Item {
        id: root
        anchors.fill: parent
        readonly property var inset: SafeArea.margins

        // the menu of each screen: what is special to it, then the program's
        readonly property var menuEntries: {
            var l = []
            if (AppModel.screen === 0) l.push({id: "decode", text: qsTr("Decode a recording...")})
            if (AppModel.screen === 1) l.push({id: "tools", text: qsTr("Transmit tools...")})
            l.push({id: "settings", text: qsTr("Settings")}, {id: "help", text: qsTr("Help")}, {id: "about", text: qsTr("About")})
            return l
        }
        function menu(id) {
            if (id === "decode") RxController.decodeRecording()
            else if (id === "tools") transmit.toolsOpen = true
            else if (id === "settings") AppModel.openSettings()
            else if (id === "help") AppModel.openHelp()
            else if (id === "about") about.open = true
        }

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
                entries: root.menuEntries
                onEntryTriggered: (id) => root.menu(id)
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
        CameraScreen {
            anchors.fill: parent
            visible: TxController.cameraOpen   // the camera itself only runs while this shows
            topInset: root.inset.top
            bottomInset: root.inset.bottom
        }
        Snackbar {
            id: snack
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.margin; anchors.rightMargin: Theme.margin
            anchors.bottomMargin: (win.wide ? 16 : Theme.navBarHeight + 16) + root.inset.bottom
            onAction: AppModel.openAppSettings()
        }
        Connections {
            target: TxController
            function onCameraBlocked() { snack.show(qsTr("The camera is blocked for this app."), qsTr("Settings")) }
        }
        AboutSheet { id: about }
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
            if (TxController.cameraOpen) TxController.cameraOpen = false
            else if (TxController.sending) TxController.stop()
            else if (receive.optionsOpen) receive.optionsOpen = false
            else if (transmit.modesOpen) transmit.modesOpen = false
            else if (transmit.digitalOpen) transmit.digitalOpen = false
            else if (transmit.toolsOpen) transmit.toolsOpen = false
            else if (about.open) about.open = false
            else if (transmit.detailsOpen) transmit.detailsOpen = false
            else if (AppModel.screen !== 0) AppModel.screen = 0
            else AppModel.moveToBackground()
        } else {
            close.accepted = false
            AppModel.quit()
        }
    }
}
