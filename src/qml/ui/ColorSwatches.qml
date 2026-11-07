import QtQuick

// A row of colors to choose from: the common choice is one tap
Flow {
    id: sw
    property string current              // "#rrggbb" or "#aarrggbb"
    property var colors: ["#ffffff", "#000000", "#e53935", "#ffb300", "#43a047", "#00897b", "#1e88e5", "#8e24aa", "#757575", "#e0e0e0"]
    signal picked(string color)
    spacing: 8
    function same(a, b) {
        const x = Qt.color(a), y = Qt.color(b)
        return Math.abs(x.r - y.r) < 0.01 && Math.abs(x.g - y.g) < 0.01 && Math.abs(x.b - y.b) < 0.01
    }
    Repeater {
        model: sw.colors
        delegate: Item {
            required property string modelData
            readonly property bool on: sw.current !== "" && sw.same(sw.current, modelData)
            width: 48; height: 48
            Rectangle {
                anchors.centerIn: parent; width: 36; height: 36; radius: 18
                color: parent.modelData
                border.width: parent.on ? 3 : 1
                border.color: parent.on ? Theme.primary : Theme.outline
            }
            TapHandler { onTapped: sw.picked(parent.modelData) }
            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData
            Accessible.checked: on
            Accessible.onPressAction: sw.picked(modelData)
        }
    }
}
