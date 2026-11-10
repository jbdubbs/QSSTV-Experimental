import QtQuick

// A row of 2 to 4 exclusive choices (Material's single-select segmented button): the current one is filled and checked
Rectangle {
    id: tabs
    property var model: []
    property int currentIndex: 0
    property bool check: true             // a check mark in the chosen one (not when the words need the room)
    signal picked(int index)
    implicitHeight: 40
    radius: height / 2
    color: "transparent"
    border.width: 1
    border.color: Theme.outline
    clip: true
    Row {
        anchors.fill: parent
        Repeater {
            model: tabs.model
            delegate: Rectangle {
                required property int index
                required property string modelData
                readonly property bool selected: index === tabs.currentIndex
                width: tabs.width / tabs.model.length
                height: tabs.height
                color: selected ? Theme.primaryContainer : "transparent"
                Rectangle { visible: parent.index > 0; width: 1; height: parent.height; color: Theme.outline }
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Icon { visible: selected && tabs.check; path: Icons.check; size: 18; color: Theme.primaryContainerOn; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        width: Math.min(implicitWidth, tabs.width / tabs.model.length - 8)
                        elide: Text.ElideRight
                        text: modelData
                        color: selected ? Theme.primaryContainerOn : Theme.surfaceVariantOn
                        font.pixelSize: Theme.sp(14)
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                TapHandler { margin: Theme.tapMargin(parent); onTapped: tabs.picked(index) }
                Accessible.onPressAction: { tabs.picked(index) }
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: selected
            }
        }
    }
}
