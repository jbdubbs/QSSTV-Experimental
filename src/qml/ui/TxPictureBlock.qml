import QtQuick
import QSSTV

// The picture that will be sent, and where a picture comes from
Column {
    id: block
    spacing: 8
    Rectangle {
        id: canvas
        x: Theme.margin
        width: parent.width - 2 * Theme.margin
        height: Math.min(width * 0.75, Math.max(150, block.maxCanvasHeight))
        radius: Theme.radius
        color: Theme.surfaceHigh
        clip: true
        ImageSurface {
            anchors.fill: parent
            source: 2
            revision: TxController.pictureRevision
            visible: TxController.hasPicture
        }
        Column {
            visible: !TxController.hasPicture
            anchors.centerIn: parent
            width: parent.width - 56
            spacing: 4
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Choose a picture to send")
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(16)
                font.weight: Font.Medium
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Pick one from your photos, take a new one, or open a file.")
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(14)
            }
        }
        Rectangle {
            visible: TxController.hasPicture
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
            radius: 10; color: "#8c000000"
            width: sizeText.implicitWidth + 16; height: sizeText.implicitHeight + 4
            Text { id: sizeText; anchors.centerIn: parent; text: TxController.pictureSize; color: "white"; font.pixelSize: Theme.sp(11) }
        }
        Accessible.role: Accessible.Graphic
        Accessible.name: TxController.hasPicture ? qsTr("Picture to send, %1").arg(TxController.pictureSize) : qsTr("No picture chosen")
    }
    // the highest the picture may be in a screen that is short (set by the screen)
    property real maxCanvasHeight: 400
    Flow {
        x: Theme.margin
        width: parent.width - 2 * Theme.margin
        spacing: 8
        Chip { text: qsTr("Photos"); icon: Icons.image; onClicked: TxController.pickPhoto() }
        Chip { text: qsTr("Camera"); icon: Icons.camera; onClicked: TxController.camera() }
        Chip { text: qsTr("Files"); icon: Icons.folder; onClicked: TxController.pickFile() }
    }
}
