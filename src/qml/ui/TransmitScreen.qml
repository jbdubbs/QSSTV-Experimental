import QtQuick
import QtQuick.Layouts
import QSSTV

// Transmit: pick a picture, check the mode and the time, press Send
Item {
    id: screen
    property bool wide: width >= 600 && width > height * 1.1
    property alias detailsOpen: details.open
    property alias modesOpen: modes.open

    ModeStrip {
        id: strip
        x: Theme.margin; y: 2
        width: parent.width - 2 * Theme.margin
    }

    // Send, at the thumb, under everything
    Item {
        id: sendBar
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 56 + 16
        WideButton {
            x: Theme.margin; y: 8
            width: parent.width - 2 * Theme.margin
            text: TxController.hasPicture ? qsTr("Send") : qsTr("Choose a picture first")
            icon: TxController.hasPicture ? Icons.send : ""
            enabledLook: TxController.hasPicture && !AppModel.digital
            onClicked: TxController.send()
        }
    }

    // narrow: everything one under the other, scrolling; wide: the picture at the left, the choices at the right
    Item {
        id: body
        anchors.top: strip.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: sendBar.top
        visible: !AppModel.digital

        Flickable {
            visible: !screen.wide
            anchors.fill: parent
            contentWidth: width
            contentHeight: narrowColumn.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Column {
                id: narrowColumn
                width: parent.width
                spacing: 4
                TxPictureBlock { width: parent.width; maxCanvasHeight: body.height * 0.4 }
                TxOptionsBlock { width: parent.width; onOpenDetails: details.open = true; onOpenModes: modes.open = true }
            }
        }
        Item {
            visible: screen.wide
            anchors.fill: parent
            TxPictureBlock { width: parent.width * 0.5; maxCanvasHeight: body.height - 60 }
            Flickable {
                anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: parent.width * 0.5
                contentWidth: width
                contentHeight: wideOptions.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                TxOptionsBlock { id: wideOptions; width: parent.width; onOpenDetails: details.open = true; onOpenModes: modes.open = true }
            }
        }
    }
    Column {
        visible: AppModel.digital
        anchors.centerIn: parent
        width: parent.width - 64
        spacing: 6
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: qsTr("Digital pictures"); color: Theme.surfaceOn; font.pixelSize: Theme.sp(18) }
        Text {
            width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
            text: qsTr("Sending digital (DRM) pictures comes in the next step of the new interface. Receiving works.")
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
        }
    }

    ModeSheet { id: modes }
    DetailsSheet { id: details }
}
