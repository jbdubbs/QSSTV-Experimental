import QtQuick
import QSSTV

// Settings > Advanced > Calibration: the clocks in use, and the three ways to measure them
SettingsPage {
    id: page
    title: qsTr("Calibration")
    signal choose(int method)
    onVisibleChanged: if (!visible) resetDialog.open = false
    readonly property bool dialogOpen: resetDialog.open
    function closeDialog() { resetDialog.open = false }
    function openReset() { resetDialog.open = true }   // the screenshots

    Text {
        width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 10
        text: qsTr("Every sound device counts time a little fast or slow. On a long picture that shows as lines that slant. Measure it against a real clock once, and the program corrects for it.")
        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
    }
    Rectangle {
        x: 16; width: parent.width - 32; height: vals.implicitHeight + 28
        radius: 16; color: Theme.surfaceContainer
        Column {
            id: vals
            x: 16; y: 14; width: parent.width - 32; spacing: 8
            Row {
                width: parent.width
                Column {
                    width: parent.width / 2
                    Text { text: qsTr("Receive clock"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
                    Text { text: Calibration.rxClockText; color: Theme.surfaceOn; font.pixelSize: Theme.sp(20); font.weight: Font.Medium }
                    Text { text: Calibration.rxNote; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
                }
                Column {
                    width: parent.width / 2
                    Text { text: qsTr("Send clock"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
                    Text { text: Calibration.txClockText; color: Theme.surfaceOn; font.pixelSize: Theme.sp(20); font.weight: Font.Medium }
                    Text { text: Calibration.txNote; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
                }
            }
            Rectangle { width: parent.width; height: 1; color: Theme.outlineVariant }
            Row {
                width: parent.width; spacing: 8
                Icon { path: Icons.phone; size: 20; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
                Text { width: parent.width - 28; text: Calibration.infoText; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap }
            }
        }
    }
    PrefGroup { text: qsTr("Measure it with") }
    PreferenceRow {
        icon: Icons.cloud; title: qsTr("Internet time"); summary: qsTr("No radio needed. Takes 5 to 30 minutes."); chevron: true
        onClicked: page.choose(1)
    }
    PreferenceRow {
        icon: Icons.antenna; title: qsTr("Radio time signal (WWV)"); summary: qsTr("Tune a radio to WWV or WWVH. About 5 minutes."); chevron: true
        onClicked: page.choose(2)
    }
    PreferenceRow {
        icon: Icons.radio; title: qsTr("Another SSTV station"); summary: qsTr("Compare with a station that is calibrated."); chevron: true
        onClicked: page.choose(3)
    }
    PreferenceRow {
        icon: Icons.reset; title: qsTr("Back to 48,000 Hz"); summary: qsTr("Forget the measured clock")
        enabledRow: Calibration.calibrated
        onClicked: resetDialog.open = true
    }
    Item { width: 1; height: 8 }
    AskDialog {
        id: resetDialog
        title: qsTr("Back to 48,000 Hz?")
        text: qsTr("The measured clock is forgotten. Pictures from a device that is a little off will slant again on long modes.")
        buttons: [{text: qsTr("Cancel")}, {text: qsTr("Back to 48,000 Hz"), primary: true}]
        onChosen: (i) => { if (i === 1) Calibration.reset() }
    }
}
