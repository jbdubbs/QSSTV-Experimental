import QtQuick

// A thin progress bar
Rectangle {
    property real value: 0
    implicitHeight: 4
    radius: 2
    color: Theme.outlineVariant
    Rectangle { width: parent.width * Math.max(0, Math.min(1, parent.value)); height: parent.height; radius: 2; color: Theme.primary }
}
