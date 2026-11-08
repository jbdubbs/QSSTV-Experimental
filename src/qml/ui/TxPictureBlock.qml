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
        // The picture moves with the fingers while Free framing is placed (#177): pinch to zoom, drag to move. The picture is drawn
        // again once, when the fingers lift (the digital one is encoded again each time), so this is only the preview of it.
        Item {
            id: mover
            objectName: "pictureMover"
            x: 0; y: 0
            width: parent.width; height: parent.height
            visible: TxController.hasPicture
            readonly property bool placing: TxController.framingFree && !TxController.sending && !holding
            property bool holding: false   // the placement is made and the picture with it is not here yet
            readonly property bool touching: pinch.active || drag.active
            onTouchingChanged: if (!touching) commit()
            onPlacingChanged: if (!placing && !holding) clear()
            function clear() {
                scale = 1; x = 0; y = 0
                holding = false
                hold.stop()
            }
            function commit() {
                if (scale === 1 && x === 0 && y === 0) return
                const pw = surface.paintedWidth, ph = surface.paintedHeight
                holding = true   // set first: the picture of an analog mode is there before the call returns
                hold.restart()
                // scale about the middle of the frame, then moved: the movement is in parts of the picture's width and height
                if (!TxController.adjustFraming(scale, 0.5, 0.5, pw > 0 ? x / pw : 0, ph > 0 ? y / ph : 0)) clear()
            }
            Connections {
                target: TxController
                function onPictureRevisionChanged() { if (mover.holding) mover.clear() }
            }
            Timer { id: hold; interval: 6000; onTriggered: mover.clear() }   // never stays; a digital picture takes a moment to encode
            PinchHandler {
                id: pinch
                target: mover
                enabled: mover.placing
                minimumRotation: 0
                maximumRotation: 0
                minimumScale: 0.1
                maximumScale: 12
                grabPermissions: PointerHandler.CanTakeOverFromAnything
            }
            DragHandler {
                id: drag
                target: mover
                enabled: mover.placing
                minimumPointCount: 1
                maximumPointCount: 1
                grabPermissions: PointerHandler.CanTakeOverFromAnything
            }
            ImageSurface {
                id: surface
                anchors.fill: parent
                source: 2
                tint: Theme.tint
                revision: TxController.pictureRevision
                visible: TxController.hasPicture
            }
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
            visible: TxController.hasPicture && !AppModel.digital
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 8
            radius: 10; color: Theme.pictureShade
            width: sizeText.implicitWidth + 16; height: sizeText.implicitHeight + 4
            Text { id: sizeText; anchors.centerIn: parent; text: TxController.pictureSize; color: Theme.onPicture; font.pixelSize: Theme.sp(12) }
        }
        Accessible.role: Accessible.Graphic
        Accessible.name: TxController.hasPicture ? qsTr("Picture to send, %1").arg(TxController.pictureSize) : qsTr("No picture chosen")
    }
    // the highest the picture may be in a screen that is short (set by the screen)
    property real maxCanvasHeight: 400
    // what lies under the picture (for a screen that shows this block beside the options and keeps the picture from taking all the height)
    readonly property real belowHeight: (framing.visible ? framing.height + spacing : 0) + pickers.height + spacing
    FramingBlock { id: framing; width: parent.width }
    Flow {
        id: pickers
        x: Theme.margin
        width: parent.width - 2 * Theme.margin
        spacing: 8
        Chip { text: qsTr("Photos"); icon: Icons.image; onClicked: TxController.pickPhoto() }
        Chip { visible: TxController.cameraAvailable; text: qsTr("Camera"); icon: Icons.camera; onClicked: TxController.camera() }
        Chip { text: qsTr("Files"); icon: Icons.folder; onClicked: TxController.pickFile() }
    }
}
