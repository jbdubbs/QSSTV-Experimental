import QtQuick
import QtQuick.Controls.Material
import QSSTV

// Another SSTV station: one side sends a picture with one straight line, the other measures how much it slants
CalibrationPage {
    id: page
    title: qsTr("Another SSTV station")
    readonly property bool sending: Calibration.sstvRole === 1
    steps: sending ? [] : [qsTr("Press Listen. The other station sends its picture: one straight white line."),
                           qsTr("If the line comes out slanted, the clocks differ. The slant gives the difference."),
                           qsTr("Save, then listen again: the line should now be straight.")]
    plotKind: 2
    plotHeight: 220
    showBar: false
    showReading: !sending
    showPlot: !sending
    showApply: !sending
    startText: sending ? qsTr("Send") : qsTr("Listen")
    againText: qsTr("Listen again")
    stopText: qsTr("Stop")
    onSaved: {}   // listening goes on: the next picture shows the line with the new clock

    Text {
        width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 8
        text: qsTr("For when there is neither a time signal nor the internet. The result is only as good as the other station: one of you must have been calibrated in one of the other ways.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
    }
    SegmentedTabs {
        x: 16; width: parent.width - 32
        model: [qsTr("Send the picture"), qsTr("Receive it")]
        currentIndex: Calibration.sstvRole === 1 ? 0 : 1
        onPicked: (i) => Calibration.sstvRole = i === 0 ? 1 : 0
    }
    // the sending side
    Column {
        visible: page.sending
        width: parent.width
        Text {
            width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 10; bottomPadding: 4
            text: Calibration.statusText; visible: text !== ""
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
        }
        PreferenceRow {
            title: qsTr("Mode"); summary: Calibration.sstvModes[Calibration.sstvModeIndex]; chevron: true
            enabledRow: !Calibration.running
            onClicked: modeDialog.open = true
        }
        SliderRow {
            title: qsTr("Line at"); summary: qsTr("Leave it in the middle: the line slants to either side.")
            enabled: !Calibration.running
            from: 10; to: 90; stepSize: 1; value: Calibration.sstvPosition
            valueText: Calibration.sstvPosition + " %"
            onCommitted: (v) => Calibration.sstvPosition = v
        }
        Item {
            visible: Calibration.sstvSending
            width: parent.width; height: 48
            ProgressLine { x: 24; y: 10; width: parent.width - 48; value: Calibration.sstvProgress }
            Text { x: 24; y: 20; text: qsTr("Sent: %1 %").arg(Math.round(Calibration.sstvProgress * 100)); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
        }
    }
    ChoiceDialog {
        id: modeDialog
        title: qsTr("Mode of the picture")
        options: Calibration.sstvModes
        current: Calibration.sstvModeIndex
        onPicked: (i) => Calibration.sstvModeIndex = i
    }
    readonly property bool dialogOpen: modeDialog.open
    function closeDialog() { modeDialog.open = false }
}
