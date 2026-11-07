import QtQuick
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
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: bar.bottom; anchors.topMargin: 8
        width: Math.min(parent.width - 48, 420)
        spacing: 16
        Rectangle {
            width: Math.min(parent.width, 240); height: width * 0.75
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
            anchors.horizontalCenter: parent.horizontalCenter
            width: 180; height: 180
            value: TxController.progress
            Column {
                anchors.centerIn: parent
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: TxController.remainingText !== "" ? TxController.remainingText : qsTr("Sending"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(34) }
                Text { anchors.horizontalCenter: parent.horizontalCenter; visible: TxController.remainingText !== ""; text: qsTr("left of %1").arg(TxController.totalText); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
            }
        }
        Flow {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            Chip { visible: TxController.toolName === ""; text: AppModel.digital ? qsTr("Digital · %1").arg(TxController.drmSummary) : TxController.modeName; tone: "selected" }
            Chip { text: AppModel.routeText + (AppModel.mediaVolumeAvailable ? "  " + AppModel.mediaVolume + " %" : ""); icon: Icons.volume }
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Keep the phone near the radio's microphone. The screen stays on.")
            color: Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(14)
        }
    }
    WideButton {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24 + overlay.bottomInset
        width: Math.min(parent.width - 48, 420)
        danger: true
        text: qsTr("Stop sending")
        icon: Icons.stop
        onClicked: TxController.stop()
    }
}
