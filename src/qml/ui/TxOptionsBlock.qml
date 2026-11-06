import QtQuick
import QtQuick.Controls.Material
import QSSTV

// The template toggle, the mode, what it takes, and how loud it will be
Column {
    id: block
    signal openDetails()
    signal openModes()
    spacing: 4

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
            TapHandler { onTapped: block.openDetails() }
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
        TapHandler { onTapped: block.openModes() }
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
