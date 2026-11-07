import QtQuick
import QtQuick.Controls.Material

// A number with a large minus and plus, for a phone that has no keyboard to be exact with; holding a button repeats it
Item {
    id: st
    property string label
    property real value: 0
    property real from: -10000
    property real to: 10000
    property real step: 1
    property int decimals: 0
    property bool enabledStepper: true
    signal changed(real value)
    implicitWidth: 140
    implicitHeight: 24 + 48
    opacity: enabledStepper ? 1 : 0.4
    Text { y: 0; text: st.label; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12) }
    Rectangle {
        y: 22; width: parent.width; height: 48
        radius: 8; color: "transparent"; border.width: 1; border.color: Theme.outline
        Button {
            id: minus
            width: 48; height: 48; flat: true; text: "−"; font.pixelSize: Theme.sp(22)
            enabled: st.enabledStepper && st.value > st.from
            autoRepeat: true; autoRepeatDelay: 400; autoRepeatInterval: 60
            onClicked: st.changed(Math.max(st.from, st.value - st.step))
            Material.foreground: Theme.primary
            Accessible.name: qsTr("Less: %1").arg(st.label)
        }
        Text {
            anchors.centerIn: parent
            text: st.value.toFixed(st.decimals)
            color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); font.weight: Font.Medium
        }
        Button {
            id: plus
            x: parent.width - 48; width: 48; height: 48; flat: true; text: "+"; font.pixelSize: Theme.sp(22)
            enabled: st.enabledStepper && st.value < st.to
            autoRepeat: true; autoRepeatDelay: 400; autoRepeatInterval: 60
            onClicked: st.changed(Math.min(st.to, st.value + st.step))
            Material.foreground: Theme.primary
            Accessible.name: qsTr("More: %1").arg(st.label)
        }
    }
}
