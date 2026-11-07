import QtQuick
import QSSTV

// Internet time: the sound device against three time servers
CalibrationPage {
    title: qsTr("Internet time")
    steps: [qsTr("Be online. The phone asks three time servers what time it is."),
            qsTr("Press Start and leave the app open. The screen stays on."),
            qsTr("After 2 minutes you can save. 30 minutes is more exact.")]
    plotKind: 0
    TextRow {
        visible: !Calibration.running
        label: qsTr("Time servers"); text: Calibration.servers; hint: qsTr("Time servers")
        onCommitted: (t) => Calibration.servers = t
    }
    InlineBanner {
        x: 16; width: parent.width - 32
        warning: false; icon: Icons.info
        text: qsTr("Receiving stops while you measure and starts again afterwards. The result is for the sound device in use now: %1.").arg(Calibration.deviceText)
    }
}
