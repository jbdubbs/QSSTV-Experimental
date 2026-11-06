import QtQuick

// The extended floating action button: the main action of a screen (Listen / Stop)
Rectangle {
    id: fab
    property string text
    property string icon
    property bool emphasized: true     // filled with the primary colour; false: the tonal container
    property bool danger: false        // Stop: the error container
    signal clicked()
    implicitHeight: 56
    implicitWidth: row.implicitWidth + 36
    radius: 16
    color: danger ? Theme.errorContainer : emphasized ? Theme.primary : Theme.primaryContainer
    readonly property color content: danger ? Theme.error : emphasized ? Theme.primaryOn : Theme.primaryContainerOn
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12
        Icon { path: fab.icon; color: fab.content; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: fab.text
            color: fab.content
            font.pixelSize: Theme.sp(15)
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    TapHandler { onTapped: fab.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: text
}
