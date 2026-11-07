import QtQuick
import QSSTV

// What the receiver is doing, as chips: listening / receiving, the mode that was detected, the line, the signal
Flow {
    spacing: 8
    Chip { visible: !RxController.listening; text: qsTr("Stopped"); tone: "warn" }
    Chip { visible: RxController.listening && !RxController.receiving; icon: Icons.mic; text: qsTr("Listening"); tone: "good" }
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
    Chip { visible: RxController.listening && !RxController.receiving; text: AppModel.digital ? qsTr("Mode: DRM") : qsTr("Mode: Auto") }
    Chip {
        visible: Frequencies.radioReady && Frequencies.chipText !== ""
        icon: Icons.radio; text: Frequencies.chipText
        onClicked: Frequencies.pickerOpen = true
    }
}
