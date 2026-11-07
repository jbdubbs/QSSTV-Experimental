import QtQuick

// The wide, rounded main button at the bottom of a screen (Send): filled, or tonal when it cannot be used yet
Rectangle {
    id: button
    property string text
    property string icon
    property bool enabledLook: true
    property bool danger: false
    property bool tonal: false          // a quieter button next to the main one
    signal clicked()
    implicitHeight: 56
    radius: height / 2
    color: danger ? Theme.errorContainer : tonal ? Theme.primaryContainer : enabledLook ? Theme.primary : Theme.surfaceHigh
    readonly property color content: danger ? Theme.error : tonal ? Theme.primaryContainerOn : enabledLook ? Theme.primaryOn : Theme.surfaceVariantOn
    opacity: enabledLook || danger || tonal ? 1 : 0.7
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
    TapHandler { margin: Theme.tapMargin(parent); enabled: button.enabledLook || button.danger || button.tonal; onTapped: button.clicked() }
    Accessible.onPressAction: if (button.enabledLook || button.danger || button.tonal) { button.clicked() }
    Accessible.role: Accessible.Button
    Accessible.name: text
}
