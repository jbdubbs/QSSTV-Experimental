import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QSSTV

// The settings: a short overview with the current values, a page for each group, nothing to confirm (every change is kept at once).
// What is not here yet is reached through the classic Configuration dialog.
Rectangle {
    id: screen
    color: Theme.surface
    visible: AppModel.settingsOpen
    z: 45
    property real topInset: 0
    property real bottomInset: 0
    property int page: 0          // 0 overview, 1 operator, 2 pictures, 3 position, 4 advanced
    signal showAbout()
    MouseArea { anchors.fill: parent }

    function close() { AppModel.settingsOpen = false }
    // Back: one level up; true when the settings were showing
    function goBack() {
        if (!AppModel.settingsOpen) return false
        if (formatDialog.open) formatDialog.open = false
        else if (coordDialog.open) coordDialog.open = false
        else if (page !== 0) page = 0
        else close()
        return true
    }
    onVisibleChanged: { formatDialog.open = false; coordDialog.open = false; if (visible) page = 0 }
    function openFormat() { formatDialog.open = true }   // the screenshots

    StackLayout {
        anchors.fill: parent
        currentIndex: screen.page

        // ---- overview ----
        SettingsPage {
            title: qsTr("Settings"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.close()
            PreferenceRow { icon: Icons.person; title: qsTr("Operator"); summary: SettingsController.operatorSummary; chevron: true; onClicked: screen.page = 1 }
            PreferenceRow { icon: Icons.volume; title: qsTr("Audio and radio"); summary: SettingsController.audioSummary; chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.pin; title: qsTr("Position"); summary: SettingsController.positionSummary; chevron: true; onClicked: screen.page = 3 }
            PreferenceRow { icon: Icons.image; title: qsTr("Pictures"); summary: SettingsController.picturesSummary; chevron: true; onClicked: screen.page = 2 }
            PreferenceRow { icon: Icons.tune; title: qsTr("Advanced"); summary: qsTr("Modes, DRM, CW, waterfall text, frequencies"); chevron: true; onClicked: screen.page = 4 }
            Item { width: 1; height: 8 }
            PreferenceRow { icon: Icons.help; title: qsTr("Help and User's Guide"); onClicked: AppModel.openHelp() }
            PreferenceRow { icon: Icons.info; title: qsTr("About, licenses and source code"); summary: qsTr("Version %1").arg(AppModel.versionText); onClicked: { screen.close(); screen.showAbout() } }
        }

        // ---- operator ----
        SettingsPage {
            title: qsTr("Operator"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 0
            Text {
                width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 8
                text: qsTr("Your call sign goes on the pictures you send. You can leave it empty.")
                color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
            }
            TextRow { label: qsTr("Call sign"); hint: qsTr("Call sign"); capitalization: Qt.ImhUppercaseOnly | Qt.ImhNoAutoUppercase
                      text: SettingsController.callsign; onCommitted: (t) => SettingsController.callsign = t }
            TextRow { label: qsTr("First name"); hint: qsTr("First name"); text: SettingsController.firstName; onCommitted: (t) => SettingsController.firstName = t }
            TextRow { label: qsTr("Last name"); hint: qsTr("Last name"); text: SettingsController.lastName; onCommitted: (t) => SettingsController.lastName = t }
            TextRow { label: qsTr("City"); hint: qsTr("City"); text: SettingsController.qth; onCommitted: (t) => SettingsController.qth = t }
        }

        // ---- pictures ----
        SettingsPage {
            title: qsTr("Pictures"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 0
            PrefGroup { text: qsTr("Received pictures") }
            PreferenceRow {
                icon: Icons.image; title: qsTr("File format"); summary: SettingsController.imageFormat.toUpperCase(); chevron: true
                onClicked: formatDialog.open = true
            }
            PrefGroup { text: qsTr("Sent pictures") }
            PreferenceRow {
                icon: Icons.send; title: qsTr("Keep a copy of what I send"); summary: qsTr("In the Sent tab of the Gallery")
                checkable: true; checked: SettingsController.saveSent
                onToggled: (on) => SettingsController.saveSent = on
            }
        }

        // ---- position ----
        SettingsPage {
            title: qsTr("Position"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 0
            PreferenceRow {
                icon: Icons.pin; title: qsTr("Use the phone's position"); summary: qsTr("For the position on the picture. The system asks once.")
                checkable: true; checked: SettingsController.gpsOn
                onToggled: (on) => SettingsController.gpsOn = on
            }
            PreferenceRow {
                icon: Icons.pin; title: qsTr("Look up city and state"); summary: SettingsController.placeLookup ? qsTr("The position is sent to OpenStreetMap") : qsTr("Off: the position stays on the phone")
                checkable: true; checked: SettingsController.placeLookup; enabledRow: SettingsController.gpsOn
                onToggled: (on) => SettingsController.placeLookup = on
            }
            PreferenceRow {
                icon: Icons.tune; title: qsTr("Coordinate format"); summary: SettingsController.coordFormat === 1 ? qsTr("Degrees and minutes (APRS)") : qsTr("Decimal degrees")
                chevron: true; enabledRow: SettingsController.gpsOn
                onClicked: coordDialog.open = true
            }
        }

        // ---- advanced: still the classic pages ----
        SettingsPage {
            title: qsTr("Advanced"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 0
            Text {
                width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 8; bottomPadding: 8
                text: qsTr("These open the classic settings, which have every option. They will move here one by one.")
                color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
            }
            PreferenceRow { icon: Icons.tune; title: qsTr("Receive"); summary: qsTr("Sensitivity, save rules, slant, sync"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.send; title: qsTr("Digital (DRM) profiles"); summary: qsTr("Saved settings for digital pictures"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.mic; title: qsTr("CW and waterfall identification"); summary: qsTr("Text, speed, tone"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.pin; title: qsTr("Frequencies"); summary: qsTr("SSTV and DRM calling frequencies"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.volume; title: qsTr("Calibration"); summary: qsTr("Sound card clock: NTP or WWV"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.image; title: qsTr("Templates"); summary: qsTr("The overlays used on your pictures"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.info; title: qsTr("Classic settings"); summary: qsTr("Every option on one list"); chevron: true; onClicked: SettingsController.openClassic() }
        }
    }

    ChoiceDialog {
        id: formatDialog
        title: qsTr("Picture file format")
        options: SettingsController.imageFormats.map(f => ({text: f.toUpperCase(), hint: f === "png" ? qsTr("No loss, larger files") : f === "jpg" || f === "jpeg" ? qsTr("Smaller files") : ""}))
        current: SettingsController.imageFormats.indexOf(SettingsController.imageFormat)
        onPicked: (i) => SettingsController.imageFormat = SettingsController.imageFormats[i]
    }
    ChoiceDialog {
        id: coordDialog
        title: qsTr("Coordinate format")
        options: [{text: qsTr("Decimal degrees"), hint: "47.60620 -122.33210"}, {text: qsTr("Degrees and minutes (APRS)"), hint: "4736.37N 12219.93W"}]
        current: SettingsController.coordFormat
        onPicked: (i) => SettingsController.coordFormat = i
    }
}
