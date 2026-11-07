import QtQuick

// A problem that stays until it is solved, said in words, with the one thing that helps: a row of the screen, not a pop-up
Rectangle {
    id: banner
    property string text
    property string actionText
    property bool warning: true
    property string icon: Icons.warning
    signal action()
    implicitHeight: Math.max(56, content.implicitHeight + 20)
    radius: 12
    color: warning ? Theme.warnContainer : Theme.surfaceContainer
    Row {
        id: content
        anchors.fill: parent; anchors.margins: 10; anchors.leftMargin: 14
        spacing: 12
        Icon { anchors.verticalCenter: parent.verticalCenter; path: banner.icon; color: banner.warning ? Theme.warn : Theme.surfaceVariantOn }
        Text {
            width: parent.width - 24 - 12 - (actionLabel.visible ? actionLabel.width + 12 : 0)
            anchors.verticalCenter: parent.verticalCenter
            text: banner.text; color: Theme.surfaceOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
        }
        Text {
            id: actionLabel
            visible: banner.actionText !== ""
            anchors.verticalCenter: parent.verticalCenter
            text: banner.actionText; color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
            padding: 8
            TapHandler { margin: Theme.tapMargin(parent); onTapped: banner.action() }
            Accessible.onPressAction: { banner.action() }
            Accessible.role: Accessible.Button
            Accessible.name: banner.actionText
        }
    }
    Accessible.role: warning ? Accessible.AlertMessage : Accessible.StaticText
    Accessible.name: text
}
