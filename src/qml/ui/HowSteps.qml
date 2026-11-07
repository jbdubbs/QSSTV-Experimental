import QtQuick

// The numbered steps of what to do, in words
Column {
    id: how
    property var steps: []
    width: parent ? parent.width : 360
    topPadding: 8; bottomPadding: 4
    Repeater {
        model: how.steps
        delegate: Item {
            required property int index
            required property string modelData
            width: how.width
            height: Math.max(40, label.implicitHeight + 12)
            Rectangle {
                x: 24; anchors.verticalCenter: parent.verticalCenter
                width: 24; height: 24; radius: 12
                color: Theme.primaryContainer
                Text { anchors.centerIn: parent; text: index + 1; color: Theme.primaryContainerOn; font.pixelSize: Theme.sp(13); font.weight: Font.Bold }
            }
            Text {
                id: label
                x: 64; width: parent.width - 64 - 24
                anchors.verticalCenter: parent.verticalCenter
                text: modelData; color: Theme.surfaceOn; font.pixelSize: Theme.sp(14); wrapMode: Text.WordWrap
            }
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Step %1: %2").arg(index + 1).arg(modelData)
        }
    }
}
