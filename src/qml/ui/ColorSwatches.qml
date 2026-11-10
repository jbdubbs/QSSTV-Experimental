import QtQuick

// A row of colors to choose from: the common choice is one tap; the last one opens the chooser for any other color
Flow {
    id: sw
    property string current              // "#aarrggbb" or "#rrggbb"
    property var colors: ["#ffffff", "#000000", "#e53935", "#ffb300", "#43a047", "#00897b", "#1e88e5", "#8e24aa", "#757575", "#e0e0e0"]
    property bool custom: true            // the last swatch: any color (customRequested)
    signal picked(string color)
    signal customRequested()
    spacing: 8
    function same(a, b) {
        const x = Qt.color(a), y = Qt.color(b)
        return Math.abs(x.r - y.r) < 0.01 && Math.abs(x.g - y.g) < 0.01 && Math.abs(x.b - y.b) < 0.01
    }
    // the current color is none of the swatches: it is one of the chooser's, and its swatch shows it
    readonly property bool other: current !== "" && !colors.some(c => same(current, c))
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
    Item {
        visible: sw.custom
        width: 48; height: 48
        Rectangle {
            anchors.centerIn: parent; width: 36; height: 36; radius: 18
            color: sw.other ? "#d0d0d0" : "transparent"          // a color with no opacity must still show as a swatch
            border.width: sw.other ? 3 : 1
            border.color: sw.other ? Theme.primary : Theme.outline
            Rectangle { visible: sw.other; anchors.fill: parent; anchors.margins: 3; radius: width / 2; color: sw.current }
            Icon { visible: !sw.other; anchors.centerIn: parent; path: Icons.palette; size: 20; color: Theme.surfaceVariantOn }
        }
        TapHandler { onTapped: sw.customRequested() }
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Other color")
        Accessible.onPressAction: sw.customRequested()
    }
}
