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

    // Send, at the thumb. Narrow: the whole width at the foot, under everything. Wide (a phone on its side): in the right column, under the choices,
    // at the usual size of a button, and the picture column takes the rest of the height (#205)
    Item {
        id: sendBar
        objectName: "sendBar"
        x: screen.wide ? screen.width / 2 : 0
        width: screen.wide ? screen.width / 2 : screen.width
        anchors.bottom: parent.bottom
        height: screen.wide ? 8 + Theme.tap + 16 : 8 + 56 + 8
        WideButton {
            x: Theme.margin; y: 8
            width: parent.width - 2 * Theme.margin
            height: screen.wide ? Theme.tap : 56
            text: TxController.hasPicture ? qsTr("Send") : qsTr("Choose a picture first")
            icon: TxController.hasPicture ? Icons.send : ""
            enabledLook: TxController.hasPicture
            onClicked: TxController.send()
        }
    }

    // narrow: everything one under the other, scrolling; wide: the picture at the left, the choices at the right (with Send under them)
    Item {
        id: body
        anchors.top: strip.bottom; anchors.topMargin: 6
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: screen.wide ? parent.bottom : sendBar.top

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
                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.bottomMargin: 16   // its foot 16 dp above the screen's, as the Send button's
                width: parent.width * 0.5
                contentWidth: width
                contentHeight: wideBlock.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                TxPictureBlock { id: wideBlock; width: parent.width; maxCanvasHeight: Math.max(150, body.height - 16 - wideBlock.belowHeight) }
            }
            Flickable {
                anchors.right: parent.right; anchors.top: parent.top
                height: parent.height - sendBar.height   // the choices scroll above the Send button (not an anchor: the bar is not a sibling)
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
