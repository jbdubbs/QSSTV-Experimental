import QtQuick
import QtQuick.Layouts
import QSSTV

// While a picture is on the air: the time left, the way the sound goes, and a big Stop
Rectangle {
    id: overlay
    color: Theme.surface
    visible: TxController.sending
    z: 40
    onVisibleChanged: AppModel.keepScreenOn(visible)
    MouseArea { anchors.fill: parent }   // nothing underneath can be pressed

    property real topInset: 0
    property real bottomInset: 0

    // how tall the picture may be: what is between the title bar and the Stop button, less the rest of the column. On a short screen, or with the two chips
    // one under the other (#175), the picture gives way, so that the text under it does not run behind the button
    readonly property real pictureRoom: stopButton.y - 8 - (bar.y + bar.height + 8) - (ring.height + chips.height + hint.height + 3 * body.spacing)

    Item {
        id: bar
        y: overlay.topInset
        width: parent.width; height: Theme.appBarHeight
        Item {
            x: 4; width: Theme.tap; height: Theme.tap; anchors.verticalCenter: parent.verticalCenter
            Icon { anchors.centerIn: parent; path: Icons.close }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: TxController.stop() }
            Accessible.onPressAction: { TxController.stop() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Stop sending")
        }
        Text {
            x: 64; anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Sending"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(20)
            Accessible.role: Accessible.Heading
        }
    }
    Column {
        id: body
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: bar.bottom; anchors.topMargin: 8
        width: Math.min(parent.width - 48, 420)
        spacing: 16
        Rectangle {
            height: Math.max(96, Math.min(Math.min(parent.width, 240) * 0.75, overlay.pictureRoom)); width: height / 0.75
            anchors.horizontalCenter: parent.horizontalCenter
            radius: Theme.radius; color: Theme.surfaceHigh; clip: true
            ImageSurface { anchors.fill: parent; source: 2; tint: Theme.tint; revision: TxController.pictureRevision; visible: TxController.toolName === "" }
            Column {
                visible: TxController.toolName !== ""
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 8
                Icon { anchors.horizontalCenter: parent.horizontalCenter; path: Icons.volume; size: 40; color: Theme.primary }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: TxController.toolName; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16) }
            }
        }
        ProgressRing {
            id: ring
            anchors.horizontalCenter: parent.horizontalCenter
            width: 180; height: 180
            value: TxController.progress
            Column {
                anchors.centerIn: parent
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: TxController.remainingText !== "" ? TxController.remainingText : qsTr("Sending"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(34) }
                Text { anchors.horizontalCenter: parent.horizontalCenter; visible: TxController.remainingText !== ""; text: qsTr("left of %1").arg(TxController.totalText); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
            }
        }
        // the mode and the way the sound goes: side by side when both fit the column, else one under the other, each centered; one that is
        // wider than the column alone (a long device name) is cut short (#175)
        GridLayout {
            id: chips
            anchors.horizontalCenter: parent.horizontalCenter
            columnSpacing: 8; rowSpacing: 8
            columns: (modeChip.visible ? modeChip.implicitWidth + columnSpacing : 0) + routeChip.implicitWidth <= body.width ? 2 : 1
            Chip {
                id: modeChip
                objectName: "sendModeChip"
                Layout.alignment: Qt.AlignHCenter
                maxWidth: body.width
                visible: TxController.toolName === ""
                text: AppModel.digital ? qsTr("Digital · %1").arg(TxController.drmSummary) : TxController.modeName
                tone: "selected"
            }
            Chip {
                id: routeChip
                objectName: "sendRouteChip"
                Layout.alignment: Qt.AlignHCenter
                maxWidth: body.width
                text: AppModel.routeText + (AppModel.mediaVolumeAvailable ? "  " + AppModel.mediaVolume + " %" : "")
                icon: Icons.volume
            }
        }
        Text {
            id: hint
            objectName: "sendHint"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Keep the phone near the radio's microphone. The screen stays on.")
            color: Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(14)
        }
    }
    WideButton {
        id: stopButton
        objectName: "sendStop"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24 + overlay.bottomInset
        width: Math.min(parent.width - 48, 420)
        danger: true
        text: qsTr("Stop sending")
        icon: Icons.stop
        onClicked: TxController.stop()
    }
}
