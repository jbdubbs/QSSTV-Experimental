import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QSSTV

// Receive: the picture comes first, the signal is explained in words, one button listens or stops
Item {
    id: screen
    property bool wide: width >= 600 && width > height * 1.1   // a tablet held upright stacks the parts like a phone
    property alias optionsOpen: options.open
    property alias modesOpen: modes.open
    signal openRoute()

    // a picture that starts while the list is open: a new mode would restart the receiver and lose it
    Connections {
        target: RxController
        function onStateChanged() { if (RxController.receiving) modes.open = false }
    }

    ModeStrip {
        id: strip
        x: Theme.margin; y: 2
        width: parent.width - 2 * Theme.margin
    }

    InlineBanner {
        id: micBanner
        visible: AppModel.micDenied
        x: Theme.margin; width: parent.width - 2 * Theme.margin
        y: strip.y + strip.height + 6
        text: qsTr("The microphone is blocked, so nothing can be received.")
        actionText: qsTr("Allow")
        onAction: AppModel.openAppSettings()
    }

    // narrow: the picture, the status and the meters stacked; wide: the picture on the left, the meters and options on the right
    Item {
        id: body
        anchors.top: micBanner.visible ? micBanner.bottom : strip.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.bottomMargin: 80    // room for the button

        PictureCanvas {
            id: canvas
            objectName: "pictureCanvas"
            x: Theme.margin
            y: 0
            width: screen.wide ? body.width * 0.58 - Theme.margin : body.width - 2 * Theme.margin
            height: screen.wide ? body.height - 6 - status.height - 12
                                : Math.max(120, body.height - status.height - 6 - meterBlock.height - 8)
        }
        StatusRow {
            id: status
            x: Theme.margin
            y: canvas.y + canvas.height + 6
            width: canvas.width
            onOpenModes: modes.open = true
        }
        ProgressLine {
            visible: RxController.receiving && RxController.lines > 0
            value: RxController.progress
            x: Theme.margin
            y: status.y + status.height + 4
            width: canvas.width
        }
        Column {
            id: meterBlock
            x: screen.wide ? canvas.x + canvas.width + Theme.margin : Theme.margin
            y: screen.wide ? 0 : status.y + status.height + 14
            width: screen.wide ? body.width - x - Theme.margin : body.width - 2 * Theme.margin
            spacing: 8
            LevelMeter {
                width: parent.width
                level: RxController.level
                zoneStart: RxController.zoneStart
                zoneEnd: RxController.zoneEnd
                word: RxController.levelWord
                active: RxController.listening
            }
            SpectrumStrip { width: parent.width; height: screen.wide ? 110 : 56 }
        }
    }

    ExtendedFab {
        anchors.right: parent.right; anchors.rightMargin: Theme.margin
        anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.margin
        text: RxController.listening ? qsTr("Stop") : qsTr("Listen")
        icon: RxController.listening ? Icons.stop : Icons.mic
        danger: RxController.listening
        emphasized: !RxController.listening
        onClicked: RxController.toggle()
    }

    ModeSheet {
        id: modes
        title: qsTr("Receive mode")
        withAuto: true
        current: RxController.mode
        onPicked: (m) => RxController.mode = m
    }

    BottomSheet {
        id: options
        title: qsTr("Receive options")
        PreferenceRow {
            title: qsTr("Save pictures automatically")
            summary: qsTr("When a picture is complete")
            checkable: true
            checked: RxController.autoSave
            onToggled: (on) => RxController.autoSave = on
        }
        PreferenceRow {
            title: qsTr("Correct slant")
            summary: qsTr("Straightens pictures from a clock that is slightly off")
            checkable: true
            checked: RxController.autoSlant
            onToggled: (on) => RxController.autoSlant = on
        }
        Item { width: 1; height: 16 }
    }
}
