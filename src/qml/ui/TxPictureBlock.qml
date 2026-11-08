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
        // Free framing is placed with the fingers (#177): pinch to zoom, drag to move. The picture is drawn again at every step from the whole picture
        // (a digital one smaller, and encoded once when the fingers lift), so what is seen is what is sent, and the parts that were out of the frame
        // come in when the picture is moved.
        Item {
            id: mover
            objectName: "pictureMover"
            anchors.fill: parent
            visible: TxController.hasPicture
            readonly property bool placing: TxController.framingFree && !TxController.sending
            property bool gesturing: false
            property point start   // where the pinch began (the middle of the two fingers)
            property var pending: null   // the last step, until it is drawn
            onPlacingChanged: if (!placing && gesturing) finish()
            function begin() {
                if (gesturing) return
                gesturing = true
                pending = null
                TxController.beginFraming()
            }
            function finish() {
                if (!gesturing) return
                flush()   // the last step is drawn before the picture is made final
                gesturing = false
                TxController.endFraming()
            }
            // the step from the start of the gesture: a zoom about the point where the fingers were, then a move; in parts of the picture's width and height
            function queue() {
                if (!gesturing) return
                const pw = surface.paintedWidth, ph = surface.paintedHeight
                if (pw <= 0 || ph <= 0) return
                const ox = (width - pw) / 2, oy = (height - ph) / 2
                if (pinch.active)
                    pending = [pinch.activeScale, (start.x - ox) / pw, (start.y - oy) / ph,
                               (pinch.centroid.position.x - start.x) / pw, (pinch.centroid.position.y - start.y) / ph]
                else if (drag.active)
                    pending = [1, 0.5, 0.5, drag.translation.x / pw, drag.translation.y / ph]
                else return
                Qt.callLater(flush)   // one drawing for the steps that come together
            }
            function flush() {
                if (!pending) return
                const a = pending
                pending = null
                TxController.moveFraming(a[0], a[1], a[2], a[3], a[4])
            }
            PinchHandler {
                id: pinch
                target: null
                enabled: mover.placing
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                onActiveChanged: {
                    if (active) { mover.start = centroid.position; mover.begin() }
                    else mover.finish()
                }
                onActiveScaleChanged: mover.queue()
                onCentroidChanged: mover.queue()
            }
            DragHandler {
                id: drag
                target: null
                enabled: mover.placing
                minimumPointCount: 1
                maximumPointCount: 1
                grabPermissions: PointerHandler.CanTakeOverFromAnything
                onActiveChanged: active ? mover.begin() : mover.finish()
                onTranslationChanged: mover.queue()
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
