import QtQuick
import QtQuick.Controls.Material
import QSSTV

// A page that measures the sound clock: what to do, the reading, how far it is, the plot of the measurement, Start and Save at the bottom.
// The rows that are special to a method go in as children.
Item {
    id: page
    property string title
    property real topInset: 0
    property real bottomInset: 0
    property var steps: []
    property int plotKind: 0
    property bool showReading: true
    property bool showBar: true
    property bool showPlot: true
    property bool showApply: true
    property bool showStart: true
    property string startText: qsTr("Start")
    property string againText: qsTr("Start again")
    property string stopText: qsTr("Stop")
    property real plotHeight: 170
    default property alias extra: extraColumn.data
    readonly property bool wide: width >= 720 && width > height
    signal back()
    signal saved()

    function save() { if (Calibration.save()) page.saved() }

    SettingsPage {
        anchors.fill: parent
        title: page.title; topInset: page.topInset; bottomInset: page.bottomInset
        contentWidth: page.wide ? 960 : 640
        onBack: page.back()
        footerHeight: 72
        footer: Row {
            x: 16; y: 8; width: parent.width - 32; height: 56; spacing: 10
            // Start; or Stop while it measures; next to Save once there is something to save
            WideButton {
                visible: page.showStart
                width: Calibration.canSave ? (parent.width - 10) * 0.42 : parent.width
                text: Calibration.running ? page.stopText : Calibration.canSave ? page.againText : page.startText
                icon: Calibration.running ? Icons.stop : Icons.play
                danger: Calibration.running && !Calibration.canSave
                tonal: Calibration.canSave
                onClicked: Calibration.running ? Calibration.stop() : Calibration.start()
            }
            WideButton {
                visible: Calibration.canSave
                width: page.showStart ? (parent.width - 10) * 0.58 : parent.width
                text: qsTr("Save"); icon: Icons.check
                onClicked: page.save()
            }
        }

        Item {
            id: body
            width: parent.width
            height: page.wide && page.showPlot ? Math.max(left.height, 264) : left.height
            Column {
                id: left
                width: page.wide && page.showPlot ? body.width * 0.5 : body.width
                HowSteps { steps: page.steps }
                Column { id: extraColumn; width: parent.width }
                InlineBanner {
                    visible: Calibration.problem !== ""
                    x: 16; width: parent.width - 32
                    text: Calibration.problem
                    actionText: Calibration.problemAction === 1 ? qsTr("Settings") : ""
                    onAction: AppModel.openAppSettings()
                }
                Column {
                    visible: page.showReading
                    width: parent.width
                    Text {
                        width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 10
                        text: Calibration.statusText; visible: text !== ""
                        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
                        Accessible.role: Accessible.StaticText
                    }
                    Text {
                        width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 4
                        text: Calibration.rateText !== "" ? Calibration.rateText : "--"
                        color: Calibration.rateText !== "" ? Theme.surfaceOn : Theme.outline
                        font.pixelSize: Theme.sp(34)
                        Accessible.role: Accessible.StaticText
                        Accessible.name: Calibration.rateText !== "" ? Calibration.rateText : qsTr("no reading yet")
                    }
                    Text {
                        width: parent.width; leftPadding: 24; rightPadding: 24; bottomPadding: 4
                        visible: Calibration.ppmText !== ""
                        text: Calibration.ppmText + (Calibration.errorText !== "" ? " (" + Calibration.errorText + ")" : "")
                        color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap
                        Accessible.role: Accessible.StaticText
                    }
                }
                QualityBar {
                    visible: page.showBar
                    width: parent.width
                    value: Calibration.quality; mark: Calibration.qualityMark
                    leftText: Calibration.qualityLeft; rightText: Calibration.qualityRight
                }
                // narrow: the plot under the numbers
                Item {
                    visible: page.showPlot && !page.wide
                    width: parent.width; height: visible ? page.plotHeight + 16 : 0
                    Rectangle {
                        x: 16; y: 8; width: parent.width - 32; height: page.plotHeight; radius: 12; color: "#0a0a28"; clip: true
                        CalibrationPlot { anchors.fill: parent; kind: page.plotKind; revision: Calibration.plotRevision }
                    }
                }
                Text {
                    visible: text !== "" && page.showReading
                    width: parent.width; leftPadding: 24; rightPadding: 24; topPadding: 2; bottomPadding: 6
                    text: Calibration.detailText
                    color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12); wrapMode: Text.WordWrap
                }
                PreferenceRow {
                    visible: page.showApply
                    title: qsTr("Use it for sending too")
                    summary: qsTr("Most sound devices run input and output from one clock.")
                    checkable: true; checked: Calibration.applyTx
                    onToggled: (c) => Calibration.applyTx = c
                }
            }
            // wide: the plot beside them
            Rectangle {
                visible: page.showPlot && page.wide
                x: body.width * 0.5; y: 8; width: body.width * 0.5 - 16; height: 240
                radius: 12; color: "#0a0a28"; clip: true
                CalibrationPlot { anchors.fill: parent; kind: page.plotKind; revision: Calibration.plotRevision }
            }
        }
    }
}
