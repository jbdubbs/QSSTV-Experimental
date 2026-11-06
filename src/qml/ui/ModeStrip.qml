import QtQuick

// The one analog / digital choice, the same on Receive and Transmit: a single-select segmented button
Rectangle {
    id: strip
    implicitHeight: 40
    radius: height / 2
    color: "transparent"
    border.width: 1
    border.color: Theme.outline
    clip: true
    readonly property bool digital: AppModel.digital
    Row {
        anchors.fill: parent
        Repeater {
            model: [qsTr("Analog SSTV"), qsTr("Digital (DRM)")]
            delegate: Rectangle {
                required property int index
                required property string modelData
                readonly property bool selected: (index === 1) === strip.digital
                width: strip.width / 2
                height: strip.height
                color: selected ? Theme.primaryContainer : "transparent"
                Rectangle {   // the divider
                    visible: parent.index === 1
                    width: 1; height: parent.height
                    color: Theme.outline
                }
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Icon { visible: selected; path: Icons.check; size: 18; color: Theme.primaryContainerOn; anchors.verticalCenter: parent.verticalCenter }
                    Text {
                        text: modelData
                        color: selected ? Theme.primaryContainerOn : Theme.surfaceVariantOn
                        font.pixelSize: Theme.sp(14)
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                TapHandler { onTapped: AppModel.digital = (index === 1) }
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: selected
            }
        }
    }
}
