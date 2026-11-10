import QtQuick
import QtQuick.Controls.Material
import QSSTV

// The gradient of the chosen item: the ramp it makes, its colors (the stops) to choose, add, remove and change, and its direction
Column {
    id: ge
    objectName: "gradientEditor"
    readonly property var s: TemplateEditor.sel
    readonly property var stops: s.gradientStops !== undefined ? s.gradientStops : []
    readonly property string itemId: s.id !== undefined ? s.id : ""
    property int stop: 0                                           // the stop being changed
    readonly property int at: Math.max(0, Math.min(stop, stops.length - 1))
    readonly property var cur: stops.length > 0 ? stops[at] : ({pos: 0, color: "#ff000000"})
    readonly property color curColor: Qt.color(cur.color)
    readonly property bool angled: s.fillKind === "linear" || s.fillKind === "conical"
    signal customRequested()                                       // a color that is not one of the swatches, for the stop being changed
    onItemIdChanged: stop = 0
    width: parent.width
    spacing: 4

    // the ramp: the colors from one end to the other, over a checkerboard so that the transparent part shows (a radial one runs out from the
    // middle, a conical one round it: the same colors)
    Item {
        id: ramp
        x: 16; width: parent.width - 32; height: 36
        Item {
            anchors.fill: parent; clip: true
            Repeater {
                model: Math.ceil(ramp.width / 12) * 3
                delegate: Rectangle {
                    required property int index
                    readonly property int cols: Math.ceil(ramp.width / 12)
                    x: (index % cols) * 12; y: Math.floor(index / cols) * 12; width: 12; height: 12
                    color: ((index % cols) + Math.floor(index / cols)) % 2 === 0 ? "#f4f4f4" : "#cccccc"
                }
            }
            // the part before the first stop and after the last is the color of that stop; between two stops it changes from one to the other
            Repeater {
                model: ge.stops.length > 0 ? ge.stops.length + 1 : 0
                delegate: Rectangle {
                    required property int index
                    readonly property var a: index > 0 ? ge.stops[index - 1] : null
                    readonly property var b: index < ge.stops.length ? ge.stops[index] : null
                    x: (a ? a.pos : 0) * ramp.width
                    width: Math.max(0, ((b ? b.pos : 1) - (a ? a.pos : 0)) * ramp.width)
                    height: parent.height
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0; color: (a || b) ? (a || b).color : "transparent" }
                        GradientStop { position: 1; color: (b || a) ? (b || a).color : "transparent" }
                    }
                }
            }
        }
        Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: Theme.outline }
    }

    // the stops, in order: the chosen one is ringed
    Flow {
        x: 16; width: parent.width - 32; spacing: 8
        Repeater {
            model: ge.stops.length
            delegate: Item {
                id: dot
                required property int index
                readonly property bool on: index === ge.at
                width: 48; height: 48
                Rectangle {
                    anchors.centerIn: parent; width: 36; height: 36; radius: 18
                    color: "#d0d0d0"
                    border.width: dot.on ? 3 : 1; border.color: dot.on ? Theme.primary : Theme.outline
                    Rectangle { anchors.fill: parent; anchors.margins: dot.on ? 3 : 1; radius: width / 2; color: ge.stops[dot.index] ? ge.stops[dot.index].color : "transparent" }
                }
                TapHandler { onTapped: ge.stop = dot.index }
                Accessible.role: Accessible.RadioButton
                Accessible.name: qsTr("Color %1 of %2").arg(index + 1).arg(ge.stops.length)
                Accessible.checked: on
                Accessible.onPressAction: ge.stop = index
            }
        }
    }
    Row {
        x: 16; spacing: 8; height: 40
        Chip {
            objectName: "addStop"
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Add")
            enabled: ge.stops.length < (ge.s.maxStops || 16); opacity: enabled ? 1 : 0.4
            onClicked: ge.stop = TemplateEditor.addGradientStop(ge.at)
        }
        Chip {
            objectName: "removeStop"
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Remove")
            enabled: ge.stops.length > 2; opacity: enabled ? 1 : 0.4
            onClicked: ge.stop = TemplateEditor.removeGradientStop(ge.at)
        }
        Chip {
            objectName: "reverseStops"
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Reverse")
            onClicked: TemplateEditor.reverseGradient()
        }
    }
    Text { text: qsTr("Color %1").arg(ge.at + 1); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); leftPadding: 16; topPadding: 4 }
    ColorSwatches {
        x: 16; width: parent.width - 32
        current: ge.cur.color
        onPicked: (c) => TemplateEditor.setGradientStopColor(ge.at, c)
        onCustomRequested: ge.customRequested()
    }
    SliderRow {
        title: qsTr("Position of the color"); from: 0; to: 100; stepSize: 1; value: Math.round(ge.cur.pos * 100)
        valueText: Math.round(value) + " %"
        onMoved: (v) => TemplateEditor.setGradientStopPos(ge.at, v / 100)
    }
    SliderRow {
        title: qsTr("Opacity of the color"); from: 0; to: 100; stepSize: 1; value: Math.round(ge.curColor.a * 100)
        valueText: Math.round(value) + " %"
        onMoved: (v) => TemplateEditor.setGradientStopColor(ge.at, Theme.colorString(Qt.rgba(ge.curColor.r, ge.curColor.g, ge.curColor.b, v / 100)))
    }
    SliderRow {
        visible: ge.angled
        title: qsTr("Direction"); from: 0; to: 355; stepSize: 5; value: ge.s.gradientAngle !== undefined ? ge.s.gradientAngle : 0
        valueText: Math.round(value) + "°"
        onMoved: (v) => TemplateEditor.setProp("gradientAngle", v)
    }
}
