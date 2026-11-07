import QtQuick

// The circle of a radio button: a ring, with a dot in it when chosen. Only the picture: the row around it takes the taps.
Item {
    id: mark
    property bool checked: false
    implicitWidth: 24
    implicitHeight: 24
    Rectangle {
        anchors.centerIn: parent
        width: 20; height: 20; radius: 10
        color: "transparent"
        border.width: 2
        border.color: mark.checked ? Theme.primary : Theme.outline
        Rectangle { anchors.centerIn: parent; width: 10; height: 10; radius: 5; color: Theme.primary; visible: mark.checked }
    }
}
