import QtQuick
import QtQuick.Layouts
import QSSTV

// Transmit: pick a picture, check the mode and the time, press Send
Item {
    id: screen
    property bool wide: width >= 600 && width > height * 1.1
    property alias detailsOpen: details.open
    property alias modesOpen: modes.open
    property alias digitalOpen: digital.open
    property alias toolsOpen: tools.open
    signal editProfiles()
    property alias templatesOpen: templates.open

    ModeStrip {
        id: strip
        x: Theme.margin; y: 2
        width: parent.width - 2 * Theme.margin
    }

    // Send, at the thumb, under everything
    Item {
        id: sendBar
        objectName: "sendBar"
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 56 + 16
        WideButton {
            x: Theme.margin; y: 8
            width: parent.width - 2 * Theme.margin
            text: TxController.hasPicture ? qsTr("Send") : qsTr("Choose a picture first")
            icon: TxController.hasPicture ? Icons.send : ""
            enabledLook: TxController.hasPicture
            onClicked: TxController.send()
        }
    }

    // narrow: everything one under the other, scrolling; wide: the picture at the left, the choices at the right
    Item {
        id: body
        anchors.top: strip.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: sendBar.top

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
                TxOptionsBlock { width: parent.width; onOpenDetails: details.open = true; onOpenTemplates: templates.open = true; onOpenModes: modes.open = true; onOpenDigital: digital.open = true }
            }
        }
        Item {
            visible: screen.wide
            anchors.fill: parent
            // the picture (with its framing) and the ways to get a picture: they scroll when the screen is too low for them (a phone on its side)
            Flickable {
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: parent.width * 0.5
                contentWidth: width
                contentHeight: wideBlock.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                TxPictureBlock { id: wideBlock; width: parent.width; maxCanvasHeight: Math.max(150, body.height - wideBlock.belowHeight) }
            }
            Flickable {
                anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
                width: parent.width * 0.5
                contentWidth: width
                contentHeight: wideOptions.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                TxOptionsBlock { id: wideOptions; width: parent.width; onOpenDetails: details.open = true; onOpenTemplates: templates.open = true; onOpenModes: modes.open = true; onOpenDigital: digital.open = true }
            }
        }
    }
    ModeSheet { id: modes; onPicked: (m) => TxController.mode = m }
    DrmSheet { id: digital; onEditProfiles: { digital.open = false; screen.editProfiles() } }
    ToolsSheet { id: tools }
    DetailsSheet { id: details }
    TemplateSheet { id: templates; onNewTemplate: starters.open = true }
    StartersSheet { id: starters }
}
