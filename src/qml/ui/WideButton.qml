import QtQuick

// The wide, rounded main button at the bottom of a screen (Send): filled, or tonal when it cannot be used yet
Rectangle {
    id: button
    property string text
    property string icon
    property bool enabledLook: true
    property bool danger: false
    signal clicked()
    implicitHeight: 56
    radius: height / 2
    color: danger ? Theme.errorContainer : enabledLook ? Theme.primary : Theme.surfaceHigh
    readonly property color content: danger ? Theme.error : enabledLook ? Theme.primaryOn : Theme.surfaceVariantOn
    opacity: enabledLook || danger ? 1 : 0.7
    Row {
        anchors.centerIn: parent
        spacing: 10
        Icon { visible: button.icon !== ""; path: button.icon; color: button.content; anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: button.text
            color: button.content
            font.pixelSize: Theme.sp(16)
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    TapHandler { margin: Theme.tapMargin(parent); enabled: button.enabledLook || button.danger; onTapped: button.clicked() }
    Accessible.onPressAction: if (button.enabledLook || button.danger) { button.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: text
}
