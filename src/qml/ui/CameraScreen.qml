import QtQuick
import QtMultimedia
import QSSTV

// The camera: the picture in the viewfinder, a button to take it, a button to turn to the other camera
Rectangle {
    id: screen
    color: "black"
    z: 45
    property real topInset: 0
    property real bottomInset: 0
    signal closed()

    MediaDevices { id: devices }
    property int deviceIndex: 0
    CaptureSession {
        id: session
        camera: Camera {
            id: camera
            active: screen.visible
            cameraDevice: devices.videoInputs.length > 0 ? devices.videoInputs[Math.min(screen.deviceIndex, devices.videoInputs.length - 1)] : devices.defaultVideoInput
        }
        imageCapture: ImageCapture {
            id: capture
            onImageSaved: (id, path) => TxController.usePicture(path)
        }
        videoOutput: view
    }
    VideoOutput {
        id: view
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    Item {
        id: bar
        y: screen.topInset
        width: parent.width; height: Theme.appBarHeight
        Item {
            x: 4; width: Theme.tap; height: Theme.tap; anchors.verticalCenter: parent.verticalCenter
            Icon { anchors.centerIn: parent; path: Icons.close; color: "white" }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: TxController.cameraOpen = false }
            Accessible.onPressAction: { TxController.cameraOpen = false }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Close the camera")
        }
        Item {
            visible: devices.videoInputs.length > 1
            anchors.right: parent.right; anchors.rightMargin: 4
            width: Theme.tap; height: Theme.tap; anchors.verticalCenter: parent.verticalCenter
            Icon { anchors.centerIn: parent; path: Icons.flip; color: "white" }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: screen.deviceIndex = (screen.deviceIndex + 1) % devices.videoInputs.length }
            Accessible.onPressAction: { screen.deviceIndex = (screen.deviceIndex + 1) % devices.videoInputs.length }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Use the other camera")
        }
    }
    // the shutter
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 32 + screen.bottomInset
        width: 76; height: 76; radius: 38
        color: "transparent"; border.width: 4; border.color: "white"
        Rectangle { anchors.centerIn: parent; width: 60; height: 60; radius: 30; color: shutter.pressed ? "#cccccc" : "white" }
        TapHandler { margin: Theme.tapMargin(parent); id: shutter; enabled: capture.readyForCapture; onTapped: capture.captureToFile(TxController.tempPicturePath()) }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Take the picture")
        Accessible.onPressAction: if (capture.readyForCapture) capture.captureToFile(TxController.tempPicturePath())
    }
}
