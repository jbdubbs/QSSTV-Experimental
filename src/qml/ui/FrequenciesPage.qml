import QtQuick
import QtQuick.Controls.Material
import QSSTV

// Settings > Advanced > Frequencies: where the radio can be tuned from the Receive and Transmit screens
Item {
    id: page
    property real topInset: 0
    property real bottomInset: 0
    readonly property bool dialogOpen: sheet.open
    signal back()
    signal openRadio()
    onVisibleChanged: if (!visible) sheet.open = false
    function closeDialog() { sheet.open = false }
    function openEdit(i) { sheet.openEdit(i) }   // the screenshots
    function openNew() { sheet.openNew() }

    SettingsPage {
        anchors.fill: parent
        title: qsTr("Frequencies"); topInset: page.topInset; bottomInset: page.bottomInset
        onBack: page.back()
        Text {
            width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 8
            text: qsTr("The frequencies you use for pictures. On the Receive and Transmit screens you tune the radio to one with a tap.")
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
        }
        InlineBanner {
            visible: !Frequencies.radioReady
            x: 16; width: parent.width - 32
            text: qsTr("No radio control is set up. You can keep the list now; to tune the radio with it, choose a radio model first.")
            actionText: qsTr("Radio"); onAction: page.openRadio()
        }
        // nothing yet
        Column {
            visible: Frequencies.count === 0
            width: parent.width; spacing: 6; topPadding: 28
            Icon { anchors.horizontalCenter: parent.horizontalCenter; path: Icons.pin; size: 40; color: Theme.outline }
            Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("No frequencies yet"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); font.weight: Font.Medium }
            Text { width: parent.width - 64; x: 32; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap; text: qsTr("Add the ones you use, or start with the usual ones."); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14) }
            Item {
                width: parent.width; height: 64
                WideButton {
                    anchors.centerIn: parent; width: 240; height: 48
                    text: qsTr("Add the usual ones"); tonal: true
                    onClicked: Frequencies.addUsual()
                }
            }
        }
        PrefGroup { visible: Frequencies.sstvEntries.length > 0; text: qsTr("Analog SSTV") }
        Repeater { model: Frequencies.sstvEntries; delegate: freqRow }
        PrefGroup { visible: Frequencies.drmEntries.length > 0; text: qsTr("Digital (DRM)") }
        Repeater { model: Frequencies.drmEntries; delegate: freqRow }
        PrefGroup { text: qsTr("Advanced") }
        TextRow {
            label: qsTr("Command sent to the radio after tuning"); hint: qsTr("Command sent to the radio after tuning (optional)")
            text: Frequencies.command
            onCommitted: (t) => Frequencies.command = t
        }
        PreferenceRow {
            title: qsTr("The command is hex"); summary: qsTr("For example FEFE94E0..., not text.")
            checkable: true; checked: Frequencies.commandHex
            onToggled: (c) => Frequencies.commandHex = c
        }
        Item { width: 1; height: 88 }
    }
    Component {
        id: freqRow
        Item {
            required property var modelData
            width: parent ? parent.width : 360; height: 64
            Row {
                anchors.fill: parent; anchors.leftMargin: 24; anchors.rightMargin: 24; spacing: 12
                Text { anchors.verticalCenter: parent.verticalCenter; width: 120; text: modelData.freq; color: Theme.surfaceOn; font.pixelSize: Theme.sp(20); font.weight: Font.Medium }
                Text { anchors.verticalCenter: parent.verticalCenter; width: parent.width - 120 - 12 - 36; text: modelData.text; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap }
                Icon { anchors.verticalCenter: parent.verticalCenter; path: Icons.chevron }
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: sheet.openEdit(parent.modelData.index) }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("%1 MHz, %2").arg(modelData.freq).arg(modelData.text)
            Accessible.onPressAction: sheet.openEdit(modelData.index)
        }
    }
    ExtendedFab {
        anchors.right: parent.right; anchors.rightMargin: 16
        anchors.bottom: parent.bottom; anchors.bottomMargin: 16 + page.bottomInset
        text: qsTr("Add"); icon: Icons.add
        onClicked: sheet.openNew()
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Add a frequency")
    }
    FrequencySheet { id: sheet }
}
