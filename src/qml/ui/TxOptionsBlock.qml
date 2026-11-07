import QtQuick
import QtQuick.Controls.Material
import QSSTV

// The template toggle, the mode, what it takes, and how loud it will be
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

    // put my details on the picture: the text opens the details, the switch turns the template on and off
    Item {
        width: parent.width
        height: Math.max(56, textCol.implicitHeight + 16)
        Column {
            id: textCol
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.right: templateSwitch.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("Put my details on the picture"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); width: parent.width; wrapMode: Text.WordWrap }
            Text {
                text: TxController.detailsSummary !== "" ? TxController.detailsSummary : qsTr("Tap to add your call sign, who it is for and a report")
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(13)
                width: parent.width
                elide: Text.ElideRight
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openDetails() }
            Accessible.onPressAction: { block.openDetails() }
        }
        Switch {
            id: templateSwitch
            anchors.right: parent.right; anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            checked: TxController.useTemplate
            onToggled: TxController.useTemplate = checked
            Accessible.name: qsTr("Put my details on the picture")
        }
    }

    // which template: the sheet lists them, changes this one or begins a new one
    Item {
        width: parent.width
        height: Math.max(56, tplCol.implicitHeight + 16)
        Column {
            id: tplCol
            anchors.left: parent.left; anchors.leftMargin: 24
            anchors.right: tplChevron.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            Text { text: qsTr("Template"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); width: parent.width; wrapMode: Text.WordWrap }
            Text {
                text: TxController.templateName !== "" ? TxController.templateName : qsTr("None yet. Tap to make one")
                color: Theme.surfaceVariantOn
                font.pixelSize: Theme.sp(13)
                width: parent.width
                elide: Text.ElideRight
            }
        }
        Icon { id: tplChevron; path: Icons.chevron; color: Theme.outline; anchors.right: parent.right; anchors.rightMargin: 20; anchors.verticalCenter: parent.verticalCenter }
        TapHandler { margin: Theme.tapMargin(parent); onTapped: block.openTemplates() }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Template") + ", " + (TxController.templateName !== "" ? TxController.templateName : qsTr("none yet"))
        Accessible.onPressAction: { block.openTemplates() }
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
        Flow {
            x: Theme.margin
            width: parent.width - 2 * Theme.margin
            spacing: 8
            Repeater {
                model: TxController.favorites
                delegate: Chip {
                    required property var modelData
                    text: modelData.name
                    icon: selected ? Icons.check : ""
                    readonly property bool selected: TxController.mode === modelData.mode
                    tone: selected ? "selected" : "plain"
                    onClicked: TxController.mode = modelData.mode
                }
            }
            // a mode chosen under "All modes" that is not a favourite
            Chip {
                id: other
                visible: !TxController.favorites.some(f => f.mode === TxController.mode)
                text: TxController.modeName
                icon: Icons.check
                tone: "selected"
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
        Flow {
            x: Theme.margin
            width: parent.width - 2 * Theme.margin
            spacing: 8
            Repeater {
                model: TxController.drmProfiles
                delegate: Chip {
                    required property int index
                    required property string modelData
                    readonly property bool selected: TxController.drmProfile === index
                    text: modelData
                    icon: selected ? Icons.check : ""
                    tone: selected ? "selected" : "plain"
                    onClicked: TxController.drmProfile = index
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
                Slider {
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
            Item {
                width: parent.width; height: routeLabel.implicitHeight
                Row {
                    spacing: 6
                    Icon { path: Icons.volume; size: 18; color: Theme.surfaceVariantOn; anchors.verticalCenter: parent.verticalCenter }
                    Text { id: routeLabel; text: AppModel.routeText; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); anchors.verticalCenter: parent.verticalCenter }
                }
                Text {
                    anchors.right: parent.right
                    visible: AppModel.mediaVolumeAvailable
                    text: qsTr("Media volume %1 %").arg(AppModel.mediaVolume)
                    color: Theme.surfaceOn; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                }
            }
            Slider {
                visible: AppModel.mediaVolumeAvailable
                width: parent.width
                from: 0; to: 100; stepSize: 1
                value: AppModel.mediaVolume
                onMoved: AppModel.mediaVolume = Math.round(value)
                Accessible.name: qsTr("Media volume")
            }
            Text {
                visible: AppModel.mediaVolumeAvailable && AppModel.mediaVolume === 0
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
        Timer { interval: 700; running: AppModel.mediaVolumeAvailable && block.visible; repeat: true; onTriggered: AppModel.refreshVolume() }
    }
    Item { width: 1; height: 8 }
}
