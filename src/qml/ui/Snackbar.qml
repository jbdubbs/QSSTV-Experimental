import QtQuick

// A short message at the bottom with an optional action; it goes away by itself
Rectangle {
    id: snack
    property string text
    property string actionText
    property string kind          // what the action is for (the one who shows it decides)
    property var payload          // and what it is about
    signal action()
    function show(message, actionLabel, what, data) {
        text = message
        actionText = actionLabel || ""
        kind = what || ""
        payload = data
        shown = true
        hider.restart()
    }
    property bool shown: false
    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150 } }
    z: 60
    height: Math.max(48, label.implicitHeight + 28)
    radius: 6
    color: Theme.dark ? "#e3e2e6" : "#2f3133"
    Row {
        anchors.fill: parent
        anchors.leftMargin: 16; anchors.rightMargin: 8
        Text {
            id: label
            width: parent.width - (snack.actionText !== "" ? actionButton.width : 0) - 8
            anchors.verticalCenter: parent.verticalCenter
            text: snack.text
            wrapMode: Text.WordWrap
            color: Theme.dark ? "#2f3133" : "#f1f0f4"
            font.pixelSize: Theme.sp(14)
        }
        Item {
            id: actionButton
            visible: snack.actionText !== ""
            width: actionLabel.implicitWidth + 24; height: parent.height
            Text {
                id: actionLabel
                anchors.centerIn: parent
                text: snack.actionText
                color: Theme.dark ? "#00677d" : "#8fdcf5"
                font.pixelSize: Theme.sp(14); font.weight: Font.Medium
            }
            TapHandler { onTapped: { snack.action(); snack.shown = false } }
            Accessible.role: Accessible.Button
            Accessible.name: snack.actionText
        }
    }
    Timer { id: hider; interval: 5000; onTriggered: snack.shown = false }
    Accessible.role: Accessible.AlertMessage
    Accessible.name: text
}
