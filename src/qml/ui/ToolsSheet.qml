import QtQuick
import QtQuick.Controls.Material
import QSSTV

// Transmit tools: a test pattern as the picture, text or the call sign in the waterfall, CW, a test tone, a frequency sweep
BottomSheet {
    id: sheet
    title: page === "menu" ? qsTr("Transmit tools") : pageTitle
    property string page: "menu"
    readonly property string pageTitle: page === "pattern" ? qsTr("Test pattern") : page === "text" ? qsTr("Waterfall text")
                                         : page === "tone" ? qsTr("Test tone") : qsTr("Frequency sweep")
    onOpenChanged: if (open) { page = "menu"; var d = TxController.toolDefaults(); tone.value = d.tone; toneTime.value = d.duration; low.value = d.low; high.value = d.high; sweepTime.value = d.sweepDuration; wfText.text = "" }

    // back to the list of tools
    Item {
        visible: sheet.page !== "menu"
        width: parent.width; height: visible ? 40 : 0
        Row {
            x: 16; anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Icon { path: Icons.back; size: 20; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
            Text { text: qsTr("All tools"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
        }
        TapHandler { margin: Theme.tapMargin(parent); onTapped: sheet.page = "menu" }
        Accessible.onPressAction: { sheet.page = "menu" }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Back to all tools")
    }

    Column {
        width: parent.width
        visible: sheet.page === "menu"
        PreferenceRow { title: qsTr("Test pattern"); summary: qsTr("Colour bars or a test card as the picture"); chevron: true; onClicked: sheet.page = "pattern" }
        PreferenceRow { title: qsTr("Waterfall text"); summary: qsTr("Your own text, drawn in the waterfall"); chevron: true; onClicked: sheet.page = "text" }
        PreferenceRow { title: qsTr("Waterfall call sign"); summary: qsTr("Your call sign, drawn in the waterfall"); onClicked: { TxController.sendWaterfallId(); sheet.open = false } }
        PreferenceRow { title: qsTr("CW call sign"); summary: qsTr("Your call sign in morse code"); onClicked: { TxController.sendCwId(); sheet.open = false } }
        PreferenceRow { title: qsTr("Test tone"); summary: qsTr("A steady tone, to set the levels"); chevron: true; onClicked: sheet.page = "tone" }
        PreferenceRow { title: qsTr("Frequency sweep"); summary: qsTr("A tone that glides from low to high"); chevron: true; onClicked: sheet.page = "sweep" }
    }

    Column {
        width: parent.width
        visible: sheet.page === "pattern"
        PreferenceRow { title: qsTr("SMPTE colour bars"); onClicked: { TxController.testPattern(0); sheet.open = false } }
        PreferenceRow { title: qsTr("PM5544"); summary: qsTr("The classic TV test card"); onClicked: { TxController.testPattern(1); sheet.open = false } }
        PreferenceRow { title: qsTr("Indian head"); onClicked: { TxController.testPattern(2); sheet.open = false } }
    }

    Column {
        width: parent.width
        visible: sheet.page === "text"
        spacing: 12
        Field {
            id: wfText
            x: 24; width: parent.width - 48
            placeholderText: qsTr("Text to send")
            inputMethodHints: Qt.ImhNoPredictiveText
            Accessible.name: qsTr("Waterfall text")
        }
        WideButton {
            x: 24; width: parent.width - 48
            text: qsTr("Send"); icon: Icons.send
            enabledLook: wfText.text.length > 0
            onClicked: { TxController.sendWaterfallText(wfText.text); sheet.open = false }
        }
    }

    Column {
        width: parent.width
        visible: sheet.page === "tone"
        spacing: 12
        Row {
            x: 24; spacing: 16
            Column { Text { text: qsTr("Frequency (Hz)"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                ThemedSpinBox { id: tone; from: 100; to: 4000; stepSize: 50; editable: true; value: 1000; Accessible.name: qsTr("Frequency in hertz") } }
            Column { Text { text: qsTr("Seconds"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                ThemedSpinBox { id: toneTime; from: 1; to: 120; editable: true; value: 5; Accessible.name: qsTr("Duration in seconds") } }
        }
        WideButton {
            x: 24; width: parent.width - 48
            text: qsTr("Send"); icon: Icons.send
            onClicked: { TxController.sendTone(tone.value, toneTime.value); sheet.open = false }
        }
    }

    Column {
        width: parent.width
        visible: sheet.page === "sweep"
        spacing: 12
        Row {
            x: 24; spacing: 12
            Column { Text { text: qsTr("From (Hz)"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                ThemedSpinBox { id: low; from: 100; to: 3900; stepSize: 100; editable: true; value: 300; Accessible.name: qsTr("Lowest frequency") } }
            Column { Text { text: qsTr("To (Hz)"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                ThemedSpinBox { id: high; from: 200; to: 4000; stepSize: 100; editable: true; value: 3000; Accessible.name: qsTr("Highest frequency") } }
        }
        Row {
            x: 24
            Column { Text { text: qsTr("Seconds"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13) }
                ThemedSpinBox { id: sweepTime; from: 1; to: 120; editable: true; value: 10; Accessible.name: qsTr("Duration in seconds") } }
        }
        WideButton {
            x: 24; width: parent.width - 48
            text: qsTr("Send"); icon: Icons.send
            enabledLook: high.value > low.value
            onClicked: { TxController.sendSweep(low.value, high.value, sweepTime.value); sheet.open = false }
        }
    }
    Item { width: 1; height: 16 }
}
