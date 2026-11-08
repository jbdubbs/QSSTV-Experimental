import QtQuick
import QSSTV

// How the picture fits the frame (#177): Stretch, Crop, Fit or Free. Free, and a single digital picture (always Free), is placed with the
// fingers on the picture above: pinch to zoom, drag to move.
Column {
    id: block
    objectName: "framingBlock"
    spacing: 4
    visible: TxController.framingChoice || TxController.framingFree
    readonly property var names: [qsTr("Stretch"), qsTr("Crop"), qsTr("Fit"), qsTr("Free")]

    Item {
        width: parent.width; height: 40
        Text {
            x: 24; anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Framing"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
        }
        Item {
            id: resetButton
            objectName: "framingReset"
            visible: TxController.framingMoved
            enabled: !TxController.sending
            opacity: enabled ? 1 : 0.38
            anchors.right: parent.right; anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: resetRow.width; height: resetRow.height
            Row {
                id: resetRow
                spacing: 4
                Icon { path: Icons.reset; color: Theme.primary; size: 18; anchors.verticalCenter: parent.verticalCenter }
                Text { text: qsTr("Reset"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium; anchors.verticalCenter: parent.verticalCenter }
            }
            TapHandler { margin: Theme.tapMargin(parent); onTapped: TxController.resetFraming() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Reset zoom and position")
            Accessible.onPressAction: { TxController.resetFraming() }
        }
    }
    // one row; it scrolls sideways when the chips are wider than the screen (as the mode chips do)
    ChipScroller {
        id: choices
        objectName: "framingChips"
        width: parent.width
        visible: TxController.framingChoice
        height: visible ? Theme.tap : 0
        enabled: !TxController.sending
        opacity: enabled ? 1 : 0.38
        Repeater {
            model: block.names
            delegate: Chip {
                id: choice
                required property int index
                required property string modelData
                anchors.verticalCenter: parent.verticalCenter
                readonly property bool selected: TxController.framing === index
                text: modelData
                icon: selected ? Icons.check : ""
                tone: selected ? "selected" : "plain"
                onClicked: TxController.framing = index
                onSelectedChanged: if (selected) Qt.callLater(choices.reveal, choice)
                Component.onCompleted: if (selected) Qt.callLater(choices.reveal, choice)
                Accessible.name: qsTr("Framing") + ", " + modelData + (selected ? ", " + qsTr("selected") : "")
            }
        }
    }
    Text {
        objectName: "framingHint"
        x: 24
        width: parent.width - 48
        visible: TxController.framingFree
        wrapMode: Text.WordWrap
        text: qsTr("Pinch to zoom, drag to move.")
        color: Theme.surfaceVariantOn
        font.pixelSize: Theme.sp(14)
    }
}
