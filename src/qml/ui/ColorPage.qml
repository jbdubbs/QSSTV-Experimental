import QtQuick
import QtQuick.Controls.Material

// Any color: hue, saturation, brightness and opacity. Each change is passed on while the finger moves, so that the picture above shows it.
// It covers the panel of the editor, and no keyboard is needed.
Rectangle {
    id: page
    objectName: "colorPage"
    property string title
    property real hue: 0
    property real sat: 0
    property real val: 1
    property real alpha: 1
    readonly property color chosen: Qt.hsva(hue, sat, val, alpha)
    signal picked(string color)       // "#aarrggbb"
    signal done()
    color: Theme.surfaceContainer
    MouseArea { anchors.fill: parent }   // a tap here is not a tap on what lies under the page

    function start(from) {
        const c = Qt.color(from)
        if (c.hsvHue >= 0) hue = c.hsvHue       // a gray has no hue: the last one stays
        sat = c.hsvSaturation; val = c.hsvValue; alpha = c.a
        body.contentY = 0
    }
    function changed() { picked(Theme.colorString(chosen)) }

    // a slider on one line: the name, the slider, the value (four of them have to fit in the panel without scrolling)
    component ColorSlider: Item {
        id: cs
        property string title
        property real from: 0
        property real to: 100
        property real value: 0
        property string valueText
        signal moved(real value)
        width: parent ? parent.width : 360
        height: 44
        Text {
            x: 16; width: 88; anchors.verticalCenter: parent.verticalCenter
            text: cs.title; elide: Text.ElideRight; color: Theme.surfaceOn; font.pixelSize: Theme.sp(14)
        }
        ThemedSlider {
            x: 100; width: parent.width - 100 - 64; anchors.verticalCenter: parent.verticalCenter
            from: cs.from; to: cs.to; stepSize: 1; value: cs.value
            Material.accent: Theme.primary
            onMoved: cs.moved(value)
            Accessible.name: cs.title
        }
        Text {
            anchors.right: parent.right; anchors.rightMargin: 16; width: 48; anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: cs.valueText; color: Theme.primary; font.pixelSize: Theme.sp(13); font.weight: Font.Medium
        }
    }

    Item {
        id: head
        x: 16; y: 8; width: parent.width - 32; height: 40
        Text {
            anchors.left: parent.left; anchors.right: doneChip.left; anchors.rightMargin: 8; anchors.verticalCenter: parent.verticalCenter
            text: page.title; elide: Text.ElideRight
            color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); font.weight: Font.Medium
        }
        Chip {
            id: doneChip
            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Done"); tone: "selected"
            onClicked: page.done()
        }
    }
    Flickable {
        id: body
        anchors.top: head.bottom; anchors.topMargin: 4; anchors.bottom: parent.bottom
        width: parent.width
        contentHeight: column.height + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        Column {
            id: column
            width: parent.width
            // what it looks like, over a checkerboard so that the transparent part shows
            Item {
                x: 16; width: parent.width - 32; height: 40
                Item {
                    anchors.fill: parent; clip: true
                    Repeater {
                        model: Math.ceil(parent.width / 12) * 4   // (a few rows more than needed: clipped)
                        delegate: Rectangle {
                            required property int index
                            readonly property int cols: Math.ceil(parent.width / 12)
                            x: (index % cols) * 12; y: Math.floor(index / cols) * 12; width: 12; height: 12
                            color: ((index % cols) + Math.floor(index / cols)) % 2 === 0 ? "#f4f4f4" : "#cccccc"
                        }
                    }
                    Rectangle { anchors.fill: parent; color: page.chosen }
                }
                Rectangle { anchors.fill: parent; color: "transparent"; border.width: 1; border.color: Theme.outline }
                Text {
                    anchors.centerIn: parent
                    text: "#" + Theme.colorString(page.chosen).substring(3).toUpperCase()
                    color: page.val > 0.6 && page.sat < 0.5 && page.alpha > 0.5 ? "#000000" : "#ffffff"
                    style: Text.Outline; styleColor: page.val > 0.6 && page.sat < 0.5 && page.alpha > 0.5 ? "#80ffffff" : "#80000000"
                    font.pixelSize: Theme.sp(13); font.weight: Font.Medium
                }
            }
            ColorSlider {
                title: qsTr("Hue"); to: 360; value: Math.round(page.hue * 360); valueText: Math.round(value) + "°"
                onMoved: (v) => { page.hue = Math.min(v, 359.99) / 360; page.changed() }
            }
            ColorSlider {
                title: qsTr("Saturation"); value: Math.round(page.sat * 100); valueText: Math.round(value) + " %"
                onMoved: (v) => { page.sat = v / 100; page.changed() }
            }
            ColorSlider {
                title: qsTr("Brightness"); value: Math.round(page.val * 100); valueText: Math.round(value) + " %"
                onMoved: (v) => { page.val = v / 100; page.changed() }
            }
            ColorSlider {
                title: qsTr("Opacity"); value: Math.round(page.alpha * 100); valueText: Math.round(value) + " %"
                onMoved: (v) => { page.alpha = v / 100; page.changed() }
            }
        }
    }
}
