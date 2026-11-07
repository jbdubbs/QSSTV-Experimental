import QtQuick
import QSSTV

// Radio time signal: the one second ticks of WWV or WWVH
CalibrationPage {
    title: qsTr("Radio time signal")
    steps: [qsTr("Tune a radio to WWV (2.5, 5, 10, 15 or 20 MHz), or to WWVH."),
            qsTr("Hold the phone near its speaker, or use the audio cable or interface."),
            qsTr("Press Start. The ticks each second must be heard.")]
    plotKind: 1
    SegmentedTabs {
        x: 16; width: parent.width - 32
        model: [qsTr("WWV 1000 Hz"), qsTr("WWVH 1200 Hz")]
        currentIndex: Calibration.station
        onPicked: (i) => Calibration.station = i
    }
    Item { width: 1; height: 8 }
    LevelMeter {
        x: 16; width: parent.width - 32
        level: Calibration.level; zoneStart: RxController.zoneStart; zoneEnd: RxController.zoneEnd
        active: Calibration.running
        word: Calibration.level < 0.25 ? qsTr("Too quiet") : Calibration.level < RxController.zoneStart ? qsTr("Low")
            : Calibration.level > 0.95 ? qsTr("Too loud") : Calibration.level > RxController.zoneEnd ? qsTr("Loud") : qsTr("Good")
        hint: Calibration.level < 0.25 ? qsTr("Raise the radio volume or move the phone closer.") : Calibration.level > 0.95 ? qsTr("Lower the radio volume or move the phone away.") : ""
    }
}
