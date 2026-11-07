import QtQuick
import QSSTV

// The waterfall of the sound, as a slim strip
Rectangle {
    id: strip
    radius: Theme.radius
    color: Theme.waterfallBackground
    clip: true
    ImageSurface {
        anchors.fill: parent
        source: 1
        fill: true
        tint: Theme.night   // the waterfall is red in Night, whatever is chosen for the pictures
        revision: RxController.waterfallRevision
    }
    // the tones of the mode: sync 1200 Hz, black 1500 Hz, white 2300 Hz; digital: the edges and the pilot of the DRM signal
    Repeater {
        model: AppModel.digital ? [725, 1475, 1850] : [1200, 1500, 2300]
        delegate: Rectangle {
            required property int modelData
            x: (modelData - RxController.fftLow) / (RxController.fftHigh - RxController.fftLow) * strip.width - 1
            width: 2; height: 6
            color: "#ff5555"
        }
    }
    Rectangle {
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.margins: 6
        radius: 10; color: "#73000000"
        width: cap.implicitWidth + 16; height: cap.implicitHeight + 4
        Text {
            id: cap
            anchors.centerIn: parent
            text: AppModel.digital ? qsTr("725 to 1850 Hz") : qsTr("1200 to 2300 Hz")
            color: Theme.night ? Theme.onPicture : "#cfe7f1"
            font.pixelSize: Theme.sp(12)
        }
    }
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("Waterfall of the received sound")
}
