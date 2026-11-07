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
        // the bars' icons are dark on a light screen and light on a dark one; the viewer, the camera and the sending page are black
        readonly property bool darkBarIcons: !Theme.dark && (TemplateEditor.open || (GalleryModel.viewerIndex < 0 && !TxController.cameraOpen))   // the editor is a light page, also over the black viewer
        onDarkBarIconsChanged: AppModel.systemBarIcons(darkBarIcons)
        Component.onCompleted: AppModel.systemBarIcons(darkBarIcons)
        // a screen of its own is in front of the others (settings, the viewer, the camera, sending ...): what is behind it is hidden, so that the
        // screen reader does not read it (Accessible.ignored does not reach the children) and nothing is drawn for nothing
        readonly property bool covered: TemplateEditor.open || AppModel.settingsOpen || AppModel.firstRun || GalleryModel.viewerIndex >= 0 || TxController.cameraOpen || TxController.sending

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
            visible: !root.covered
            anchors.top: parent.top
            anchors.topMargin: root.inset.top
            anchors.left: win.wide ? rail.right : parent.left
            anchors.right: parent.right
            anchors.rightMargin: root.inset.right
            anchors.bottom: win.wide ? parent.bottom : nav.top
            SelectionBar {
                visible: AppModel.screen === 2 && GalleryModel.selecting
                width: parent.width
                height: visible ? Theme.appBarHeight : 0
            }
            AppBar {
                id: bar
                visible: !(AppModel.screen === 2 && GalleryModel.selecting)
                height: visible ? Theme.appBarHeight : 0
                width: parent.width
                title: root.title
                routeText: AppModel.routeText
                entries: root.menuEntries
                onEntryTriggered: (id) => root.menu(id)
                onRouteClicked: { AppModel.settingsOpen = true; Qt.callLater(function() { settings.page = 5 }) }   // the route chip: where the sound goes, and how to change it
                actions: [
                    Item {
                        visible: AppModel.screen === 0
                        width: Theme.tap; height: Theme.tap
                        Icon { anchors.centerIn: parent; path: Icons.tune }
                        TapHandler { margin: Theme.tapMargin(parent); onTapped: receive.optionsOpen = true }
                        Accessible.onPressAction: { receive.optionsOpen = true }
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Receive options")
                    }
                ]
            }
            StackLayout {
                width: parent.width
                height: parent.height - Theme.appBarHeight
                currentIndex: AppModel.screen
                ReceiveScreen { id: receive; objectName: "receiveScreen" }
                TransmitScreen { id: transmit; objectName: "transmitScreen" }
                GalleryScreen { id: gallery }
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
            z: 60   // above the picture viewer too
            anchors.left: parent.left; anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: Theme.margin; anchors.rightMargin: Theme.margin
            anchors.bottomMargin: (GalleryModel.viewerIndex >= 0 ? 72 + 8 : win.wide ? 16 : Theme.navBarHeight + 16) + root.inset.bottom
            onAction: {
                if (kind === "settings") AppModel.openAppSettings()
                else if (kind === "undo") GalleryModel.undoDelete()
                else if (kind === "view") GalleryModel.openFile(payload)
            }
        }
        Connections {
            target: TxController
            function onCameraBlocked() { snack.show(qsTr("The camera is blocked for this app."), qsTr("Settings"), "settings") }
        }
        Connections {
            target: TxController
            function onSendingChanged() { AppModel.haptic(TxController.sending ? 1 : 0) }   // it starts, it is over
        }
        Connections {
            target: GalleryModel
            function onDeleted(count) { snack.show(count === 1 ? qsTr("Picture deleted") : qsTr("%1 pictures deleted").arg(count), qsTr("Undo"), "undo") }
        }
        Connections {
            target: RxController
            function onPictureSaved(file) { snack.show(qsTr("Saved to Gallery"), qsTr("View"), "view", file); AppModel.haptic(1) }
        }
        ViewerScreen {
            id: viewer
            objectName: "viewerScreen"
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
        }
        TemplateEditorScreen {
            id: tplEditor
            objectName: "templateEditorScreen"
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
            leftInset: root.inset.left
            rightInset: root.inset.right
            onNotice: (text) => snack.show(text, "")
        }
        SettingsScreen {
            id: settings
            objectName: "settingsScreen"
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
            onShowAbout: about.open = true
        }
        FirstRunScreen {
            id: firstRun
            objectName: "firstRunScreen"
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
            onSetUpRadio: { AppModel.settingsOpen = true; Qt.callLater(function() { settings.page = 5 }) }
        }
        ProblemDialog { id: problemDialog }
        Connections {
            target: AppModel
            function onProblemShown(title, text) { problemDialog.show(title, text) }
            function onNoticeShown(text) { snack.show(text, "") }
        }
        AboutSheet { id: about }
        SendingOverlay {
            anchors.fill: parent
            topInset: root.inset.top
            bottomInset: root.inset.bottom
        }
        NavBar {
            id: nav
            visible: !win.wide && !root.covered
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            bottomInset: root.inset.bottom
        }
        NavRail {
            id: rail
            visible: win.wide && !root.covered
            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
            leftInset: root.inset.left
            topPadding: root.inset.top
        }
    }

    onClosing: (close) => {
        if (Qt.platform.os === "android") {
            close.accepted = false
            if (problemDialog.open) problemDialog.open = false
            else if (tplEditor.goBack()) {}
            else if (firstRun.goBack()) {}
            else if (settings.goBack()) {}
            else if (viewer.infoOpen) viewer.infoOpen = false
            else if (GalleryModel.viewerIndex >= 0) GalleryModel.closeViewer()
            else if (AppModel.screen === 2 && GalleryModel.selecting) GalleryModel.clearSelection()
            else if (TxController.cameraOpen) TxController.cameraOpen = false
            else if (TxController.sending) TxController.stop()
            else if (receive.optionsOpen) receive.optionsOpen = false
            else if (transmit.modesOpen) transmit.modesOpen = false
            else if (transmit.digitalOpen) transmit.digitalOpen = false
            else if (transmit.toolsOpen) transmit.toolsOpen = false
            else if (transmit.templatesOpen) transmit.templatesOpen = false
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
