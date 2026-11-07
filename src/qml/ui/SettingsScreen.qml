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
    property int page: 0          // 0 overview, 1 operator, 2 pictures, 3 position, 4 advanced, 5 audio and radio, 6 the radio list, 7 receive, 8 identification
    onPageChanged: {
        if (page === 5) SettingsController.rescanAdapters()   // an adapter may have been plugged in
        if (page !== 6) { search.text = ""; search.focus = false; Qt.inputMethod.hide() }   // the radio search is over
    }
    signal showAbout()
    MouseArea { anchors.fill: parent }

    function close() { AppModel.settingsOpen = false }
    // Back: one level up; true when the settings were showing
    function goBack() {
        if (!AppModel.settingsOpen) return false
        if (formatDialog.open) formatDialog.open = false
        else if (coordDialog.open) coordDialog.open = false
        else if (micDialog.open) micDialog.open = false
        else if (speakerDialog.open) speakerDialog.open = false
        else if (presetDialog.open) presetDialog.open = false
        else if (adapterDialog.open) adapterDialog.open = false
        else if (pttDialog.open) pttDialog.open = false
        else if (baudDialog.open) baudDialog.open = false
        else if (sensitivityDialog.open) sensitivityDialog.open = false
        else if (page === 6) page = 5
        else if (page === 7 || page === 8) page = 4
        else if (page !== 0) page = 0
        else close()
        return true
    }
    onVisibleChanged: { formatDialog.open = false; coordDialog.open = false; micDialog.open = false; speakerDialog.open = false; presetDialog.open = false; adapterDialog.open = false; pttDialog.open = false; baudDialog.open = false; sensitivityDialog.open = false; if (visible) page = 0 }
    function openFormat() { formatDialog.open = true }   // the screenshots
    function openMic() { presetDialog.open = true }

    StackLayout {
        anchors.fill: parent
        currentIndex: screen.page

        // ---- overview ----
        SettingsPage {
            title: qsTr("Settings"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.close()
            PreferenceRow { icon: Icons.person; title: qsTr("Operator"); summary: SettingsController.operatorSummary; chevron: true; onClicked: screen.page = 1 }
            PreferenceRow { icon: Icons.volume; title: qsTr("Audio and radio"); summary: SettingsController.audioSummary; chevron: true; onClicked: screen.page = 5 }
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
            InlineBanner {
                visible: SettingsController.gpsState === 3
                x: 16; width: parent.width - 32
                text: SettingsController.gpsReason
                actionText: SettingsController.gpsReason.indexOf("may not") >= 0 ? qsTr("Allow") : ""
                onAction: AppModel.openAppSettings()
            }
            PreferenceRow {
                visible: SettingsController.gpsState === 2
                icon: Icons.pin; title: qsTr("Position now"); summary: SettingsController.positionText
            }
            PreferenceRow {
                visible: SettingsController.gpsState === 1
                icon: Icons.pin; title: qsTr("Position now"); summary: qsTr("Waiting for the phone to find where it is")
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
                text: qsTr("Some of these open the classic settings, which have every option. They will move here one by one.")
                color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
            }
            PreferenceRow { icon: Icons.tune; title: qsTr("Receive"); summary: qsTr("Sensitivity, when a picture is kept"); chevron: true; onClicked: screen.page = 7 }
            PreferenceRow { icon: Icons.send; title: qsTr("Digital (DRM) profiles"); summary: qsTr("Saved settings for digital pictures"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.mic; title: qsTr("CW and waterfall identification"); summary: qsTr("Text, speed, tone"); chevron: true; onClicked: screen.page = 8 }
            PreferenceRow { icon: Icons.pin; title: qsTr("Frequencies"); summary: qsTr("SSTV and DRM calling frequencies"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.volume; title: qsTr("Calibration"); summary: qsTr("Sound card clock: NTP or WWV"); chevron: true; onClicked: SettingsController.openClassic() }
            PreferenceRow { icon: Icons.image; title: qsTr("Templates"); summary: qsTr("Change them or make new ones in the Gallery"); chevron: true; onClicked: { AppModel.settingsOpen = false; GalleryModel.tab = 2; AppModel.screen = 2 } }
            PreferenceRow { icon: Icons.info; title: qsTr("Classic settings"); summary: qsTr("Every option on one list"); chevron: true; onClicked: SettingsController.openClassic() }
        }

        // ---- audio and radio ----
        SettingsPage {
            title: qsTr("Audio and radio"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 0
            PrefGroup { text: qsTr("Sound in") }
            PreferenceRow {
                icon: Icons.mic; title: qsTr("Microphone"); chevron: true
                summary: SettingsController.inputDevice === "default" ? qsTr("System default") : SettingsController.inputDevice
                onClicked: micDialog.open = true
            }
            PreferenceRow {
                icon: Icons.tune; title: qsTr("Microphone processing"); chevron: true
                summary: SettingsController.micPreset === 1 ? qsTr("Unprocessed") : SettingsController.micPreset === 2 ? qsTr("Voice recognition")
                       : SettingsController.micPreset === 3 ? qsTr("Standard") : qsTr("Automatic")
                onClicked: presetDialog.open = true
            }
            SliderRow {
                title: qsTr("Receive gain"); summary: qsTr("Raise it when the radio is quiet")
                from: 0; to: 30; stepSize: 1; value: SettingsController.rxGainDb
                valueText: "+" + SettingsController.rxGainDb + " dB"
                onCommitted: (v) => SettingsController.rxGainDb = v
            }
            LevelMeter {
                x: 24; width: parent.width - 48
                level: RxController.level; zoneStart: RxController.zoneStart; zoneEnd: RxController.zoneEnd
                word: RxController.levelWord; hint: RxController.levelHint; active: RxController.listening
            }
            PrefGroup { text: qsTr("Sound out") }
            PreferenceRow {
                icon: Icons.volume; title: qsTr("Speaker"); chevron: true
                summary: SettingsController.outputDevice === "default" ? qsTr("System default") : SettingsController.outputDevice
                onClicked: speakerDialog.open = true
            }
            SliderRow {
                visible: AppModel.mediaVolumeAvailable
                title: qsTr("Media volume"); summary: qsTr("Sets how loud a picture is sent")
                from: 0; to: 100; stepSize: 1; value: AppModel.mediaVolume
                valueText: AppModel.mediaVolume + " %"
                onCommitted: (v) => AppModel.mediaVolume = v
            }
            Item {
                width: parent.width; height: 64
                Button {
                    x: 24; anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Test sound"); highlighted: true; font.pixelSize: Theme.sp(14)
                    enabled: !TxController.sending
                    Material.accent: Theme.primary
                    onClicked: TxController.sendTone(1000, 3)
                    Accessible.name: qsTr("Play a test tone")
                }
            }
            Text {
                visible: SettingsController.routeDetail !== ""
                width: parent.width; leftPadding: 24; rightPadding: 24; bottomPadding: 8
                text: qsTr("In use: %1").arg(SettingsController.routeDetail)
                color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap
            }
            PrefGroup { text: qsTr("Radio control") }
            PreferenceRow {
                icon: Icons.usb; title: qsTr("USB interface"); chevron: true
                summary: SettingsController.adapters.length === 0 ? qsTr("None found. Plug in the adapter.") : (SettingsController.adapterText !== "" ? SettingsController.adapterText : qsTr("Not chosen"))
                enabledRow: SettingsController.adapters.length > 0
                onClicked: adapterDialog.open = true
            }
            PreferenceRow {
                icon: Icons.settings; title: qsTr("Radio"); chevron: true
                summary: SettingsController.radioModelText !== "" ? SettingsController.radioModelText : qsTr("No radio control")
                onClicked: screen.page = 6
            }
            PreferenceRow {
                icon: Icons.send; title: qsTr("Push to talk"); chevron: true
                summary: SettingsController.pttMode === 1 ? qsTr("RTS line") : SettingsController.pttMode === 2 ? qsTr("DTR line")
                       : SettingsController.pttMode === 3 ? qsTr("CAT command") : qsTr("None (VOX or by hand)")
                onClicked: pttDialog.open = true
            }
            PreferenceRow {
                icon: Icons.tune; title: qsTr("Speed"); chevron: true
                summary: SettingsController.radioModel === "" ? qsTr("Choose a radio first") : qsTr("%1 baud").arg(SettingsController.baudRate)
                enabledRow: SettingsController.radioModel !== ""
                onClicked: baudDialog.open = true
            }
            Item {
                width: parent.width; height: 64
                Button {
                    x: 24; anchors.verticalCenter: parent.verticalCenter
                    text: SettingsController.pttTesting ? qsTr("Keying...") : qsTr("Test push to talk")
                    font.pixelSize: Theme.sp(14)
                    enabled: SettingsController.pttMode !== 0 && !SettingsController.pttTesting && !TxController.sending
                    Material.accent: Theme.primary
                    onClicked: SettingsController.testPtt()
                    Accessible.name: qsTr("Key the transmitter for a moment")
                }
            }
            Text {
                width: parent.width; leftPadding: 24; rightPadding: 24; bottomPadding: 12
                text: qsTr("Keys the transmitter for a second. Turn the radio's power down or disconnect the antenna first.")
                color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap
            }
        }

        // ---- the radio list: hundreds of models, so a search first ----
        Item {
            id: radioPage
            property string filter: ""
            readonly property var models: {
                const all = SettingsController.radioModels
                const f = filter.trim().toLowerCase()
                return f === "" ? all : all.filter(m => m.toLowerCase().indexOf(f) >= 0)
            }
            Item {
                id: radioBar
                y: screen.topInset; width: parent.width; height: Theme.appBarHeight
                Item {
                    x: 4; width: Theme.tap; height: parent.height
                    Icon { anchors.centerIn: parent; path: Icons.back; color: Theme.surfaceOn }
                    TapHandler { margin: Theme.tapMargin(parent); onTapped: screen.page = 5 }
                    Accessible.onPressAction: { screen.page = 5 }
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Back")
                }
                Text { x: 64; height: parent.height; verticalAlignment: Text.AlignVCenter; text: qsTr("Radio"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(22); Accessible.role: Accessible.Heading }
            }
            TextField {
                id: search
                anchors.top: radioBar.bottom; anchors.topMargin: 4
                x: 16; width: parent.width - 32
                placeholderText: qsTr("Search: Kenwood, TS-480, IC-7300 ...")
                font.pixelSize: Theme.sp(16)
                color: Theme.surfaceOn; placeholderTextColor: Theme.outline
                Material.accent: Theme.primary
                inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: radioPage.filter = text
                Accessible.name: qsTr("Search the radios")
            }
            ListView {
                anchors.top: search.bottom; anchors.topMargin: 4
                anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, 640)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator {}
                header: PreferenceRow {
                    title: qsTr("No radio control"); summary: qsTr("Use VOX, or the key line of the adapter")
                    onClicked: { SettingsController.radioModel = ""; screen.page = 5 }
                }
                model: radioPage.models
                delegate: PreferenceRow {
                    required property string modelData
                    title: modelData.trim()
                    onClicked: { SettingsController.radioModel = modelData; screen.page = 5 }
                }
                footer: Item { width: 1; height: screen.bottomInset + 24 }
            }
        }

        // ---- receive ----
        SettingsPage {
            title: qsTr("Receive"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 4
            PreferenceRow {
                icon: Icons.mic; title: qsTr("Sensitivity"); chevron: true
                summary: RxController.sensitivity === 0 ? qsTr("Low") : RxController.sensitivity === 2 ? qsTr("High") : RxController.sensitivity === 3 ? qsTr("DX (weak signals)") : qsTr("Normal")
                onClicked: sensitivityDialog.open = true
            }
            SliderRow {
                title: qsTr("Keep a picture when this much came in"); summary: qsTr("A picture that stops early is still kept when it reached this share")
                from: 0; to: 100; stepSize: 5; value: RxController.minCompletion
                valueText: RxController.minCompletion + " %"
                onCommitted: (v) => RxController.minCompletion = v
            }
            PreferenceRow {
                icon: Icons.image; title: qsTr("Save pictures automatically"); summary: qsTr("When one is complete")
                checkable: true; checked: RxController.autoSave
                onToggled: (on) => RxController.autoSave = on
            }
            PreferenceRow {
                icon: Icons.tune; title: qsTr("Correct slant"); summary: qsTr("Straighten pictures from a radio whose clock is off")
                checkable: true; checked: RxController.autoSlant
                onToggled: (on) => RxController.autoSlant = on
            }
        }

        // ---- identification ----
        SettingsPage {
            title: qsTr("CW and waterfall"); topInset: screen.topInset; bottomInset: screen.bottomInset
            onBack: screen.page = 4
            PrefGroup { text: qsTr("CW") }
            TextRow { label: qsTr("Text sent as CW"); hint: qsTr("Text sent as CW"); text: SettingsController.cwText; onCommitted: (t) => SettingsController.cwText = t }
            SliderRow {
                title: qsTr("Tone"); from: 300; to: 1500; stepSize: 10; value: SettingsController.cwTone
                valueText: SettingsController.cwTone + " Hz"
                onCommitted: (v) => SettingsController.cwTone = v
            }
            SliderRow {
                title: qsTr("Speed"); from: 5; to: 40; stepSize: 1; value: SettingsController.cwSpeed
                valueText: SettingsController.cwSpeed + " WPM"
                onCommitted: (v) => SettingsController.cwSpeed = v
            }
            PrefGroup { text: qsTr("Waterfall text") }
            SliderRow {
                title: qsTr("Letter size"); from: 8; to: 32; stepSize: 1; value: SettingsController.wfFontSize
                valueText: SettingsController.wfFontSize + " pt"
                onCommitted: (v) => SettingsController.wfFontSize = v
            }
            PreferenceRow {
                icon: Icons.mic; title: qsTr("Bold letters"); checkable: true; checked: SettingsController.wfBold
                onToggled: (on) => SettingsController.wfBold = on
            }
            PrefGroup { text: qsTr("Markers in the waterfall") }
            Repeater {
                model: [
                    {key: "startPic", label: qsTr("Before a picture")}, {key: "endPic", label: qsTr("After a picture")},
                    {key: "fix", label: qsTr("Repair request: fix")}, {key: "bsr", label: qsTr("Repair request: BSR")},
                    {key: "startBin", label: qsTr("Before a file")}, {key: "endBin", label: qsTr("After a file")},
                    {key: "startRepeater", label: qsTr("Before a repeater message")}, {key: "endRepeater", label: qsTr("After a repeater message")}]
                delegate: TextRow {
                    required property var modelData
                    label: modelData.label; hint: modelData.label
                    text: { SettingsController.wfFontSize; return SettingsController.waterfallText(modelData.key) }
                    onCommitted: (t) => SettingsController.setWaterfallText(modelData.key, t)
                }
            }
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
    ChoiceDialog {
        id: presetDialog
        title: qsTr("Microphone processing")
        options: [
            {text: qsTr("Automatic"), hint: qsTr("Unprocessed if the phone has it")},
            {text: qsTr("Unprocessed"), hint: qsTr("Best for pictures")},
            {text: qsTr("Voice recognition"), hint: qsTr("Some noise reduction")},
            {text: qsTr("Standard"), hint: qsTr("The phone may level and filter the sound")}]
        current: SettingsController.micPreset
        onPicked: (i) => SettingsController.micPreset = i
    }
    ChoiceDialog {
        id: adapterDialog
        title: qsTr("USB interface")
        options: SettingsController.adapters.map(a => ({text: a.text}))
        current: { const a = SettingsController.adapters; for (let i = 0; i < a.length; i++) if (a[i].name === SettingsController.adapter) return i; return -1 }
        onPicked: (i) => SettingsController.adapter = SettingsController.adapters[i].name
    }
    ChoiceDialog {
        id: pttDialog
        title: qsTr("Push to talk")
        options: [
            {text: qsTr("None"), hint: qsTr("The radio uses VOX, or you key it by hand")},
            {text: qsTr("RTS line"), hint: qsTr("Digirig and most radio cables")},
            {text: qsTr("DTR line"), hint: qsTr("Some cables use this one instead")},
            {text: qsTr("CAT command"), hint: qsTr("The radio keys itself on a command")}]
        current: SettingsController.pttMode
        onPicked: (i) => SettingsController.pttMode = i
    }
    ChoiceDialog {
        id: baudDialog
        readonly property var rates: [1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
        title: qsTr("Speed")
        options: rates.map(r => ({text: qsTr("%1 baud").arg(r)}))
        current: rates.indexOf(SettingsController.baudRate)
        onPicked: (i) => SettingsController.baudRate = rates[i]
    }
    ChoiceDialog {
        id: sensitivityDialog
        title: qsTr("Sensitivity")
        options: [
            {text: qsTr("Low"), hint: qsTr("Fewer false starts, needs a clean signal")},
            {text: qsTr("Normal"), hint: qsTr("The usual choice")},
            {text: qsTr("High"), hint: qsTr("Starts on weaker signals")},
            {text: qsTr("DX (weak signals)"), hint: qsTr("Weakest signals, more false starts")}]
        current: RxController.sensitivity
        onPicked: (i) => RxController.sensitivity = i
    }
    ChoiceDialog {
        id: coordDialog
        title: qsTr("Coordinate format")
        options: [{text: qsTr("Decimal degrees"), hint: "47.60620 -122.33210"}, {text: qsTr("Degrees and minutes (APRS)"), hint: "4736.37N 12219.93W"}]
        current: SettingsController.coordFormat
        onPicked: (i) => SettingsController.coordFormat = i
    }
}
