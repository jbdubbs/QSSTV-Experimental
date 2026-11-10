import QtQuick
import QSSTV

// The canvas of the Receive screen: the picture as it comes in, or what to do while there is none
Rectangle {
    id: canvas
    radius: Theme.radius
    color: Theme.surfaceHigh
    clip: true
    property bool listening: RxController.listening
    ImageSurface {
        anchors.fill: parent
        source: 0
        tint: Theme.tint
        revision: RxController.pictureRevision
        visible: RxController.hasPicture
    }
    Column {
        visible: !RxController.hasPicture
        anchors.centerIn: parent
        width: parent.width - 56
        spacing: 4
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: canvas.listening ? qsTr("Listening") : qsTr("Not listening")
            color: Theme.surfaceOn
            font.pixelSize: Theme.sp(16)
            font.weight: Font.Medium
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: canvas.listening ? qsTr("Hold the phone near the radio's speaker. A picture starts by itself when a signal arrives.")
                                   : qsTr("Tap Stopped, below the picture, to start the receiver.")
            color: Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(14)
        }
    }
    Accessible.role: Accessible.Graphic
    Accessible.name: RxController.hasPicture ? qsTr("Received picture") : qsTr("No picture yet")
}
