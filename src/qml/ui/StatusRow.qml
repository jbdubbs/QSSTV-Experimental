import QtQuick
import QSSTV

// What the receiver is doing, as chips: listening / receiving, the mode that was detected, the line, the signal.
// The first chip is also the switch (#181): "Listening" stops the receiver, "Stopped" starts it. While a picture comes in it is not offered
Flow {
    spacing: 8
    signal openModes()
    Chip {
        objectName: "stoppedChip"
        visible: !RxController.listening
        icon: Icons.play; text: qsTr("Stopped"); tone: "warn"
        onClicked: RxController.toggle()
        Accessible.name: qsTr("Stopped, tap to start")
    }
    Chip {
        objectName: "listeningChip"
        visible: RxController.listening && !RxController.receiving
        icon: Icons.stop; text: qsTr("Listening"); tone: "good"
        onClicked: RxController.toggle()
        Accessible.name: qsTr("Listening, tap to stop")
    }
    Chip { visible: RxController.receiving && RxController.modeText === ""; text: AppModel.digital ? qsTr("Receiving digital") : qsTr("Receiving"); tone: "selected" }
    Chip { visible: RxController.modeText !== ""; text: RxController.modeText; tone: "selected" }
    Chip {
        visible: RxController.receiving && RxController.lines > 0
        text: qsTr("Line %1 of %2").arg(RxController.line).arg(RxController.lines)
    }
    Chip {
        visible: RxController.signalText !== ""
        text: RxController.signalText
        tone: RxController.sync >= 0.6 ? "good" : RxController.sync >= 0.3 ? "plain" : "warn"
    }
    // DRM has no mode to choose; the analog chip opens the list of modes, and shows a fixed mode (not Auto) highlighted
    Chip { visible: RxController.listening && !RxController.receiving && AppModel.digital; text: qsTr("Mode: DRM") }
    Chip {
        visible: RxController.listening && !RxController.receiving && !AppModel.digital
        icon: Icons.expand; text: qsTr("Mode: %1").arg(RxController.modeName)
        tone: RxController.mode >= 0 ? "selected" : "plain"
        onClicked: openModes()
    }
    Chip {
        visible: Frequencies.radioReady && Frequencies.chipText !== ""
        icon: Icons.radio; text: Frequencies.chipText
        onClicked: Frequencies.pickerOpen = true
    }
}
