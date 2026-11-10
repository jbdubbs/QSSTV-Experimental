import QtQuick

// The small heading above a group of settings rows
Text {
    width: parent ? parent.width : 360
    leftPadding: 24; topPadding: 20; bottomPadding: 6
    color: Theme.primary
    font.pixelSize: Theme.sp(14)
    font.weight: Font.Medium
    Accessible.role: Accessible.Heading
    Accessible.name: text
}
