import QtQuick

// One destination of the navigation bar or rail: an icon in a pill, and its name
Item {
    id: item
    property string label
    property string icon
    property bool selected
    signal clicked()
    implicitHeight: col.implicitHeight
    implicitWidth: col.implicitWidth
    property real topPad: 0
    Column {
        id: col
        y: item.topPad
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4
        Rectangle {
            width: 64; height: 32; radius: 16
            anchors.horizontalCenter: parent.horizontalCenter
            color: item.selected ? Theme.primaryContainer : "transparent"
            Icon { anchors.centerIn: parent; path: item.icon; color: item.selected ? Theme.primaryContainerOn : Theme.surfaceVariantOn }
        }
        Text {
            text: item.label
            anchors.horizontalCenter: parent.horizontalCenter
            color: item.selected ? Theme.surfaceOn : Theme.surfaceVariantOn
            font.pixelSize: Theme.sp(12)
            font.weight: item.selected ? Font.Bold : Font.Medium
        }
    }
    TapHandler { onTapped: item.clicked() }
    Accessible.role: Accessible.PageTab
    Accessible.name: label
    Accessible.selected: selected
}
