import QtQuick
import QtQuick.Controls.Material

// Something went wrong and the person has to know: a title, what happened, one button
Item {
    id: dlg
    property bool open: false
    property string title
    property string text
    function show(t, m) { title = t; text = m; open = true }
    parent: Overlay.overlay
    anchors.fill: parent
    visible: open
    z: 30

    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        MouseArea { anchors.fill: parent }
    }
    Rectangle {
        id: card
        objectName: "problemCard"
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, 360)
        // as tall as its content, but inside the window: a long message (a large text size, a short screen) scrolls between the title and the button (#184)
        height: Math.min(parent.height - 120, header.height + messageText.implicitHeight + footer.height)
        radius: 28
        color: Theme.surfaceContainer
        Column {
            id: header
            width: parent.width
            topPadding: 24
            Icon { x: 24; path: Icons.warning; color: Theme.warn; size: 28 }
            Text {
                text: dlg.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(22)
                leftPadding: 24; rightPadding: 24; topPadding: 12; bottomPadding: 8; width: parent.width; wrapMode: Text.WordWrap
                Accessible.role: Accessible.Heading
            }
        }
        Flickable {
            id: message
            objectName: "problemMessage"
            anchors.top: header.bottom; anchors.bottom: footer.top
            anchors.left: parent.left; anchors.right: parent.right
            clip: true
            contentWidth: width
            contentHeight: messageText.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollIndicator.vertical: ScrollIndicator {}
            Text {
                id: messageText
                text: dlg.text; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14)
                leftPadding: 24; rightPadding: 24; bottomPadding: 12; width: parent.width; wrapMode: Text.WordWrap
            }
        }
        Item {
            id: footer
            anchors.bottom: parent.bottom
            width: parent.width; height: 56
            Text {
                objectName: "problemOk"
                anchors.right: parent.right; anchors.rightMargin: 24; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("OK"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                padding: 8
                TapHandler { margin: Theme.tapMargin(parent); onTapped: dlg.open = false }
                Accessible.onPressAction: { dlg.open = false }
                Accessible.role: Accessible.Button
            }
        }
    }
    Accessible.role: Accessible.AlertMessage
    Accessible.name: title
}
