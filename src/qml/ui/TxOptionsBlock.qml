import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QSSTV

// The template and its details, the mode, what it takes, and how loud it will be
Column {
    id: block
    signal openDetails()
    signal openTemplates()
    signal openModes()
    signal openDigital()
    spacing: 4

    // the radio's frequency: a tap tunes it to one of the list
    Flow {
        visible: Frequencies.radioReady && Frequencies.chipText !== ""
        x: Theme.margin; width: parent.width - 2 * Theme.margin
        Chip { icon: Icons.radio; text: Frequencies.chipText; onClicked: Frequencies.pickerOpen = true }
    }

    // which template: the sheet lists them (None first), changes this one or begins a new one
    Item {
        id: tplRow
        objectName: "templateRow"
        // None when the picture goes without a template; "None yet" when there is not one to choose
        readonly property string current: TxController.templateName === "" ? qsTr("None yet. Tap to make one")
                                          : TxController.templateOn ? TxController.templateName : qsTr("None")
        width: parent.width
        height: Math.max(56, tplCol.implicitHeight + 16)
        Column {
            id: tplCol
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.right: tplChevron.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("Template"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); width: parent.width; wrapMode: Text.WordWrap }
            Text {
                text: tplRow.current
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(13)
                width: parent.width
                elide: Text.ElideRight
            }
        }
        Icon { id: tplChevron; path: Icons.chevron; color: Theme.outline; anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter }
        TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openTemplates() }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Template") + ", " + tplRow.current
        Accessible.onPressAction: { block.openTemplates() }
    }

    // the details a template shows (whom it is for, the operator, the report); only while a template is on the picture
    Item {
        objectName: "detailsRow"
        visible: TxController.templateOn
        width: parent.width
        height: visible ? Math.max(56, detailsCol.implicitHeight + 16) : 0
        Column {
            id: detailsCol
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.right: detailsChevron.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("Details"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); width: parent.width; wrapMode: Text.WordWrap }
            Text {
                text: TxController.detailsSummary !== "" ? TxController.detailsSummary : qsTr("Tap to add your call sign, who it is for and a report")
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(13)
                width: parent.width
                elide: Text.ElideRight
            }
        }
        Icon { id: detailsChevron; path: Icons.chevron; color: Theme.outline; anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter }
        TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openDetails() }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Details") + ", " + (TxController.detailsSummary !== "" ? TxController.detailsSummary : qsTr("not set"))
        Accessible.onPressAction: { block.openDetails() }
    }

    // analog: the mode, what it takes
    Column {
        width: parent.width
        spacing: 4
        visible: !AppModel.digital
        Item {
            width: parent.width; height: 40
            Text {
                x: 24; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Mode"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
            }
            Row {
                anchors.right: parent.right; anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { text: qsTr("All modes"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
                Icon { path: Icons.chevron; color: Theme.primary; size: 20; anchors.verticalCenter: parent.verticalCenter }
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openModes() }
            Accessible.onPressAction: { block.openModes() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("All modes")
        }
        // one row; it scrolls sideways when the buttons are wider than the screen (#174)
        ChipScroller {
            id: modeChips
            objectName: "modeChips"
            width: parent.width
            Repeater {
                model: TxController.favorites
                delegate: Chip {
                    id: favourite
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name
                    icon: selected ? Icons.check : ""
                    readonly property bool selected: TxController.mode === modelData.mode
                    tone: selected ? "selected" : "plain"
                    onClicked: TxController.mode = modelData.mode
                    onSelectedChanged: if (selected) Qt.callLater(modeChips.reveal, favourite)
                    Component.onCompleted: if (selected) Qt.callLater(modeChips.reveal, favourite)
                }
            }
            // a mode chosen under "All modes" that is not a favourite
            Chip {
                id: other
                objectName: "otherModeChip"
                anchors.verticalCenter: parent.verticalCenter
                visible: !TxController.favorites.some(f => f.mode === TxController.mode)
                text: TxController.modeName
                icon: Icons.check
                tone: "selected"
                onVisibleChanged: if (visible) Qt.callLater(modeChips.reveal, other)
                onTextChanged: if (visible) Qt.callLater(modeChips.reveal, other)
                Component.onCompleted: if (visible) Qt.callLater(modeChips.reveal, other)
            }
        }
        Text {
            x: 24
            width: parent.width - 48
            visible: TxController.hasPicture
            text: TxController.pictureSize + (TxController.timeText !== "" ? "  ·  " : "")
            color: Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(14)
            Text {
                anchors.left: parent.left
                anchors.leftMargin: parent.contentWidth
                text: TxController.timeText
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(14)
                font.weight: Font.Medium
            }
        }
    }

    // digital: the saved settings, the size of the picture, what it takes
    Column {
        width: parent.width
        spacing: 4
        visible: AppModel.digital
        Item {
            width: parent.width; height: 40
            Text {
                x: 24; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Digital settings"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
            }
            Row {
                anchors.right: parent.right; anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { text: qsTr("Details"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
                Icon { path: Icons.chevron; color: Theme.primary; size: 20; anchors.verticalCenter: parent.verticalCenter }
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openDigital() }
            Accessible.onPressAction: { block.openDigital() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Digital settings, details")
        }
        ChipScroller {
            id: profileChips
            objectName: "profileChips"
            width: parent.width
            Repeater {
                model: TxController.drmProfiles
                delegate: Chip {
                    id: profile
                    required property int index
                    required property string modelData
                    anchors.verticalCenter: parent.verticalCenter
                    readonly property bool selected: TxController.drmProfile === index
                    text: modelData
                    icon: selected ? Icons.check : ""
                    tone: selected ? "selected" : "plain"
                    onClicked: TxController.drmProfile = index
                    onSelectedChanged: if (selected) Qt.callLater(profileChips.reveal, profile)
                    Component.onCompleted: if (selected) Qt.callLater(profileChips.reveal, profile)
                }
            }
        }
        Item {
            width: parent.width
            height: sizeCol.implicitHeight
            Column {
                id: sizeCol
                x: 24
                width: parent.width - 48
                Item {
                    width: parent.width; height: sizeLabel.implicitHeight + 4
                    Text { id: sizeLabel; text: qsTr("Picture size"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16) }
                    Text {
                        anchors.right: parent.right
                        text: qsTr("%1 bytes").arg(sizeSlider.pressed ? Math.round(sizeSlider.value) : TxController.drmSize)
                        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
                    }
                }
                ThemedSlider {
                    id: sizeSlider
                    width: parent.width
                    from: TxController.drmSizeMin; to: Math.max(TxController.drmSizeMax, TxController.drmSizeMin + 1)
                    stepSize: 100
                    value: TxController.drmSize
                    onPressedChanged: if (!pressed) TxController.drmSize = Math.round(value)   // the picture is encoded again once, when the finger lets go
                    Accessible.name: qsTr("Picture size in bytes")
                }
            }
        }
        Text {
            x: 24
            width: parent.width - 48
            text: TxController.drmSummary + (TxController.timeText !== "" ? "  ·  " : "")
            color: Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(14)
            Text {
                anchors.left: parent.left
                anchors.leftMargin: parent.contentWidth
                text: TxController.timeText
                color: Theme.surfaceOn
                font.pixelSize: Theme.sp(14)
                font.weight: Font.Medium
            }
        }
    }
    Rectangle {
        x: Theme.margin
        width: parent.width - 2 * Theme.margin
        height: volumeCol.implicitHeight + 24
        radius: Theme.radius
        color: Theme.surfaceContainer
        Column {
            id: volumeCol
            x: 14; y: 12
            width: parent.width - 28
            spacing: 4
            // the name of the device takes the room the percentage leaves and is cut with "..." when it is long (a Bluetooth speaker, a USB interface): the
            // percentage stays whole (#203)
            RowLayout {
                width: parent.width
                spacing: 6
                Icon { path: Icons.volume; size: 18; color: Theme.surfaceVariantOn; Layout.alignment: Qt.AlignVCenter }
                Text {
                    id: routeLabel; objectName: "volumeRoute"
                    text: AppModel.routeText
                    color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13)
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    objectName: "volumeLevel"
                    visible: AppModel.mediaVolumeAvailable && !AppModel.volumeMismatch
                    text: qsTr("Media volume %1 %").arg(AppModel.mediaVolume)
                    color: Theme.surfaceOn; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                }
            }
            // the slider and the keys act on the device Android plays music on: when the sound goes to another one, the note says so instead
            VolumeNote { width: parent.width }
            ThemedSlider {
                objectName: "mediaVolumeSlider"
                visible: AppModel.mediaVolumeAvailable && !AppModel.volumeMismatch
                width: parent.width
                from: 0; to: 100; stepSize: 1
                value: AppModel.mediaVolume
                onMoved: AppModel.mediaVolume = Math.round(value)
                Accessible.name: qsTr("Media volume")
            }
            Text {
                visible: AppModel.mediaVolumeAvailable && !AppModel.volumeMismatch && AppModel.mediaVolume === 0
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("The volume is off: the picture would be sent in silence. Turn it up with the slider or the volume keys.")
                color: Theme.error
                font.pixelSize: Theme.sp(13)
                Accessible.role: Accessible.AlertMessage
            }
            Text {
                visible: !AppModel.mediaVolumeAvailable
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("The loudness is set by the volume of the sound card.")
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(13)
            }
        }
    }
    Item { width: 1; height: 8 }
}
