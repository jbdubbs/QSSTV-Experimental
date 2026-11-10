import QtQuick

// A button in the colors of the look: filled in the main color (highlighted), or only its words (flat), or an outline. A press that is held repeats
// the click when autoRepeat is set (the − and + of a stepper).
Item {
    id: button
    property string text
    property bool highlighted: false
    property bool flat: false
    property int pixelSize: Theme.sp(14)
    property bool autoRepeat: false
    property int autoRepeatDelay: 400
    property int autoRepeatInterval: 60
    property bool down: tap.pressed
    signal clicked()
    implicitWidth: Math.max(flat ? 48 : 64, label.implicitWidth + (flat ? 16 : 48))
    implicitHeight: flat ? 48 : 40
    opacity: enabled ? 1 : 0.38
    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: button.highlighted ? Theme.primary : button.down ? Theme.surfaceHigh : "transparent"
        border.width: button.highlighted || button.flat ? 0 : 1
        border.color: Theme.outline
    }
    Text {
        id: label
        anchors.centerIn: parent
        text: button.text
        color: button.highlighted ? Theme.primaryOn : Theme.primary
        font.pixelSize: button.pixelSize
        font.weight: Font.Medium
    }
    property bool repeating: false
    Timer { id: delay; interval: button.autoRepeatDelay; onTriggered: { button.repeating = true; button.clicked(); again.start() } }
    Timer { id: again; interval: button.autoRepeatInterval; repeat: true; onTriggered: button.clicked() }
    TapHandler {
        id: tap
        enabled: button.enabled
        margin: Theme.tapMargin(button)
        onPressedChanged: {
            if (pressed && button.autoRepeat) { button.repeating = false; delay.restart() }
            else { delay.stop(); again.stop() }
        }
        onTapped: if (!button.repeating) button.clicked()
    }
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: if (button.enabled) button.clicked()
}
