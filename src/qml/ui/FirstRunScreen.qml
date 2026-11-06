import QtQuick
import QtQuick.Controls.Material
import QSSTV

// The first start: three short steps, each can be skipped. Who is on the air, where the sound comes from, and (only when a USB adapter is
// attached) the radio. Permissions are asked later, when a feature is first used.
Rectangle {
    id: first
    color: Theme.surface
    z: 55
    visible: AppModel.firstRun
    property real topInset: 0
    property real bottomInset: 0
    property int step: 0
    readonly property int lastStep: SettingsController.adapters.length > 0 ? 2 : 1
    signal setUpRadio()
    MouseArea { anchors.fill: parent }

    function goBack() {
        if (!AppModel.firstRun) return false
        if (micDialog.open) micDialog.open = false
        else if (speakerDialog.open) speakerDialog.open = false
        else if (step > 0) step--
        return true      // on the first step Back does nothing: Skip leaves the setup
    }
    function next() { if (step >= lastStep) AppModel.finishFirstRun(); else step++ }
    onVisibleChanged: if (visible) step = 0

    // the three dots
    Row {
        y: first.topInset + 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8
        Repeater {
            model: first.lastStep + 1
            Rectangle { required property int index; width: index === first.step ? 24 : 8; height: 8; radius: 4; color: index === first.step ? Theme.primary : Theme.outlineVariant }
        }
    }

    Flickable {
        id: body
        anchors.fill: parent
        anchors.topMargin: first.topInset + 56
        anchors.bottomMargin: first.bottomInset + 80
        contentHeight: content.implicitHeight + 24
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Column {
            id: content
            width: Math.min(parent.width, 560)
            anchors.horizontalCenter: parent.horizontalCenter

            // ---- 1: who ----
            Column {
                width: parent.width; visible: first.step === 0
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 16; text: qsTr("Who is on the air?"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(28); wrapMode: Text.WordWrap; Accessible.role: Accessible.Heading }
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 12; bottomPadding: 16
                       text: qsTr("Your call sign goes on the pictures you send. You can leave it empty and set it later.")
                       color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap }
                TextRow { label: qsTr("Call sign"); hint: qsTr("Call sign"); capitalization: Qt.ImhUppercaseOnly | Qt.ImhNoAutoUppercase
                          text: SettingsController.callsign; onCommitted: (t) => SettingsController.callsign = t }
                TextRow { label: qsTr("Name (optional)"); hint: qsTr("Name (optional)"); text: SettingsController.firstName; onCommitted: (t) => SettingsController.firstName = t }
            }

            // ---- 2: sound ----
            Column {
                width: parent.width; visible: first.step === 1
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 16; text: qsTr("Where does the sound come from?"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(28); wrapMode: Text.WordWrap; Accessible.role: Accessible.Heading }
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 12; bottomPadding: 8
                       text: qsTr("Start with the phone's own microphone and speaker: hold the phone near the radio. Change it here if you use a cable.")
                       color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap }
                PreferenceRow {
                    icon: Icons.mic; title: qsTr("Microphone"); chevron: true
                    summary: SettingsController.inputDevice === "default" ? qsTr("Phone microphone (system default)") : SettingsController.inputDevice
                    onClicked: micDialog.open = true
                }
                PreferenceRow {
                    icon: Icons.volume; title: qsTr("Speaker"); chevron: true
                    summary: SettingsController.outputDevice === "default" ? qsTr("Phone speaker (system default)") : SettingsController.outputDevice
                    onClicked: speakerDialog.open = true
                }
                LevelMeter {
                    x: 24; width: parent.width - 48
                    level: RxController.level; zoneStart: RxController.zoneStart; zoneEnd: RxController.zoneEnd
                    word: RxController.levelWord; hint: RxController.levelHint; active: RxController.listening
                }
            }

            // ---- 3: radio ----
            Column {
                width: parent.width; visible: first.step === 2
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 16; text: qsTr("Control your radio?"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(28); wrapMode: Text.WordWrap; Accessible.role: Accessible.Heading }
                Text { width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 12; bottomPadding: 16
                       text: qsTr("A USB adapter is connected. The program can switch the transmitter on and read the frequency. Skip it if you use VOX.")
                       color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap }
                Item {
                    width: parent.width; height: 64
                    Button {
                        x: 24; anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Set up the radio"); highlighted: true
                        Material.accent: Theme.primary
                        onClicked: { AppModel.finishFirstRun(); first.setUpRadio() }
                    }
                }
            }
        }
    }

    // Skip and Next at the bottom, where the thumb is
    Item {
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.bottomMargin: first.bottomInset
        height: 72
        Text {
            x: 24; anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Skip"); color: Theme.primary; font.pixelSize: Theme.sp(15); font.weight: Font.Medium; padding: 12
            TapHandler { onTapped: AppModel.finishFirstRun() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Skip the setup")
        }
        Button {
            anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter
            text: first.step >= first.lastStep ? qsTr("Done") : qsTr("Next"); highlighted: true
            Material.accent: Theme.primary
            onClicked: first.next()
        }
    }

    ChoiceDialog {
        id: micDialog
        title: qsTr("Microphone")
        options: SettingsController.inputDevices.map(d => ({text: d === "default" ? qsTr("System default") : d}))
        current: SettingsController.inputDevices.indexOf(SettingsController.inputDevice)
        onPicked: (i) => SettingsController.inputDevice = SettingsController.inputDevices[i]
    }
    ChoiceDialog {
        id: speakerDialog
        title: qsTr("Speaker")
        options: SettingsController.outputDevices.map(d => ({text: d === "default" ? qsTr("System default") : d}))
        current: SettingsController.outputDevices.indexOf(SettingsController.outputDevice)
        onPicked: (i) => SettingsController.outputDevice = SettingsController.outputDevices[i]
    }
}
