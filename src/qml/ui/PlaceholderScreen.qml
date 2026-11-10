import QtQuick

// A screen that is not built yet
Item {
    property string title
    Column {
        anchors.centerIn: parent
        spacing: 8
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(20) }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: qsTr("Coming in the next stage of the new interface."); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(14) }
    }
}
