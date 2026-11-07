import QtQuick
import QtQuick.Controls.Material
import QSSTV

// The properties of the selected item: Text (the words and the letters), Style (colors), Place (position, size, turn). Not a modal sheet:
// the item stays in view above it.
Rectangle {
    id: panel
    objectName: "editPanel"
    color: Theme.surfaceContainer
    property int tab: 0
    property real bottomInset: 0
    signal openMacros()
    signal closeRequested()
    readonly property var s: TemplateEditor.sel
    readonly property bool isText: s.kind === "text"
    readonly property bool hasSel: TemplateEditor.hasSelection
    // the words are being typed: the panel shrinks to the field and the codes and sits above the keyboard (the screen moves it). It does so as
    // soon as the field has the focus, before the keyboard is up, so that Android has no reason to shift the whole window up
    readonly property bool typing: textField.activeFocus
    property bool keyboardSeen: false
    onTypingChanged: if (!typing) keyboardSeen = false
    Connections {
        target: Qt.inputMethod
        function onVisibleChanged() {
            if (Qt.inputMethod.visible) panel.keyboardSeen = true
            else if (panel.keyboardSeen) { panel.keyboardSeen = false; textField.focus = false }   // the keyboard was closed (Back): the typing is over
        }
    }
    onIsTextChanged: if (!isText && tab === 0) tab = 1

    // the segmented tabs
    Row {
        id: tabs
        visible: !panel.typing
        x: 16; y: 8; width: parent.width - 32; height: 40
        readonly property var names: panel.isText ? [qsTr("Text"), qsTr("Style"), qsTr("Place")] : [qsTr("Style"), qsTr("Place")]
        Repeater {
            model: tabs.names
            delegate: Rectangle {
                required property int index
                required property string modelData
                readonly property int tabIndex: panel.isText ? index : index + 1
                width: tabs.width / tabs.names.length; height: 40
                color: panel.tab === tabIndex ? Theme.primaryContainer : "transparent"
                border.width: 1; border.color: Theme.outline
                Text { anchors.centerIn: parent; text: parent.modelData; font.pixelSize: Theme.sp(14); font.weight: Font.Medium
                       color: panel.tab === parent.tabIndex ? Theme.primaryContainerOn : Theme.surfaceVariantOn }
                TapHandler { margin: Theme.tapMargin(parent); onTapped: panel.tab = parent.tabIndex }
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData
                Accessible.checked: panel.tab === tabIndex
                Accessible.onPressAction: panel.tab = tabIndex
            }
        }
    }
    Flickable {
        id: body
        anchors.top: panel.typing ? parent.top : tabs.bottom; anchors.topMargin: panel.typing ? 4 : 8
        anchors.bottom: parent.bottom; anchors.bottomMargin: panel.bottomInset
        width: parent.width
        contentHeight: Math.max(textTab.height, styleTab.height, placeTab.height) + 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        // ---- Text ----
        Column {
            id: textTab
            visible: panel.tab === 0 && panel.isText
            width: parent.width
            topPadding: 10   // the label of the field rises above its frame
            Field {
                id: textField
                x: 16; width: parent.width - 32
                placeholderText: qsTr("Text")
                font.pixelSize: Theme.sp(16); color: Theme.surfaceOn; Material.accent: Theme.primary
                Binding on text { when: !textField.activeFocus; restoreMode: Binding.RestoreNone; value: panel.s.text !== undefined ? panel.s.text : "" }
                onTextEdited: TemplateEditor.setProp("text", text)
                EnterKey.type: Qt.EnterKeyDone
                onAccepted: { focus = false; Qt.inputMethod.hide() }
                Accessible.name: qsTr("Text of the item")
            }
            Text { text: qsTr("Insert"); color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); leftPadding: 16; topPadding: 6 }
            Flickable {
                width: parent.width; height: 48; contentWidth: macroRow.width + 32; clip: true; boundsBehavior: Flickable.StopAtBounds
                Row {
                    id: macroRow
                    x: 16; spacing: 8; height: 48
                    Repeater {
                        model: [{tag: "m", n: qsTr("My call")}, {tag: "c", n: qsTr("To call")}, {tag: "r", n: qsTr("Report")}, {tag: "t", n: qsTr("Time")}, {tag: "d", n: qsTr("Date")}]
                        delegate: Chip {
                            required property var modelData
                            anchors.verticalCenter: parent.verticalCenter
                            text: "%" + modelData.tag + " " + modelData.n
                            onClicked: { textField.insert(textField.cursorPosition, "%" + modelData.tag); TemplateEditor.setProp("text", textField.text) }
                        }
                    }
                    Chip { anchors.verticalCenter: parent.verticalCenter; text: qsTr("All..."); tone: "selected"; onClicked: panel.openMacros() }
                }
            }
            SliderRow {
                title: qsTr("Size"); from: 6; to: 120; stepSize: 1; value: panel.s.fontSize !== undefined ? panel.s.fontSize : 24
                valueText: Math.round(value)
                onMoved: (v) => TemplateEditor.setProp("fontSize", v)
            }
            Row {
                x: 16; spacing: 8; height: 56
                Chip { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Bold"); tone: panel.s.bold === true ? "selected" : "plain"; onClicked: TemplateEditor.setProp("bold", !(panel.s.bold === true)) }
                Chip { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Italic"); tone: panel.s.italic === true ? "selected" : "plain"; onClicked: TemplateEditor.setProp("italic", !(panel.s.italic === true)) }
            }
        }

        // ---- Style ----
        Column {
            id: styleTab
            visible: panel.tab === 1
            width: parent.width
            spacing: 4
            PrefGroup { visible: panel.s.hasFill === true || panel.s.kind === "line"; text: panel.s.hasFill ? qsTr("Fill") : qsTr("Color") }
            Item {
                visible: panel.s.hasFill === true || panel.s.kind === "line"
                x: 16; width: parent.width - 32; height: swFill.implicitHeight
                ColorSwatches {
                    id: swFill
                    width: parent.width
                    current: panel.s.kind === "line" ? (panel.s.outlineColor || "") : (panel.s.fill || "")
                    onPicked: (c) => TemplateEditor.setProp(panel.s.kind === "line" ? "outlineColor" : "fill", c)
                }
            }
            SliderRow {
                title: qsTr("Opacity"); from: 0; to: 100; stepSize: 1; value: Math.round((panel.s.opacity !== undefined ? panel.s.opacity : 1) * 100)
                valueText: Math.round(value) + " %"
                onMoved: (v) => TemplateEditor.setProp("opacity", v / 100)
            }
            PreferenceRow {
                visible: panel.s.hasOutline === true && panel.s.kind !== "line"
                title: panel.isText ? qsTr("Edge of the letters") : qsTr("Outline"); checkable: true; checked: panel.s.outlineOn === true
                onToggled: (on) => TemplateEditor.setProp("outlineOn", on)
            }
            Item {
                visible: panel.s.outlineOn === true && panel.s.kind !== "line"
                x: 16; width: parent.width - 32; height: swOut.implicitHeight
                ColorSwatches {
                    id: swOut
                    width: parent.width
                    current: panel.s.outlineColor || ""
                    onPicked: (c) => TemplateEditor.setProp("outlineColor", c)
                }
            }
            SliderRow {
                visible: panel.s.outlineOn === true || panel.s.kind === "line"
                title: panel.s.kind === "line" ? qsTr("Thickness") : qsTr("Outline width"); from: 1; to: 20; stepSize: 1
                value: panel.s.outlineWidth !== undefined ? panel.s.outlineWidth : 1
                valueText: Math.round(value)
                onMoved: (v) => TemplateEditor.setProp("outlineWidth", v)
            }
            SliderRow {
                visible: panel.s.hasRadius === true
                title: qsTr("Rounded corners"); from: 0; to: 80; stepSize: 1; value: panel.s.radius !== undefined ? panel.s.radius : 0
                valueText: Math.round(value)
                onMoved: (v) => TemplateEditor.setProp("radius", v)
            }
            PreferenceRow {
                title: qsTr("Shadow"); checkable: true; checked: panel.s.shadow === true
                onToggled: (on) => TemplateEditor.setProp("shadow", on)
            }
        }

        // ---- Place ----
        Column {
            id: placeTab
            visible: panel.tab === 2
            width: parent.width
            spacing: 6
            Grid {
                x: 16; width: parent.width - 32; columns: 2; columnSpacing: 12; rowSpacing: 4
                readonly property real cell: (width - 12) / 2
                Stepper { width: parent.cell; label: qsTr("Left"); value: Math.round(panel.s.x !== undefined ? panel.s.x : 0); from: -2000; to: 2000
                          onChanged: (v) => TemplateEditor.setProp("x", v) }
                Stepper { width: parent.cell; label: qsTr("Top"); value: Math.round(panel.s.y !== undefined ? panel.s.y : 0); from: -2000; to: 2000
                          onChanged: (v) => TemplateEditor.setProp("y", v) }
                Stepper { width: parent.cell; label: qsTr("Width"); value: Math.round(panel.s.w !== undefined ? panel.s.w : 0); from: 1; to: 2000; enabledStepper: panel.s.freeSize === true
                          onChanged: (v) => TemplateEditor.setProp("w", v) }
                Stepper { width: parent.cell; label: qsTr("Height"); value: Math.round(panel.s.h !== undefined ? panel.s.h : 0); from: 1; to: 2000; enabledStepper: panel.s.freeSize === true && panel.s.kind !== "line"
                          onChanged: (v) => TemplateEditor.setProp("h", v) }
                Stepper { width: parent.cell; label: qsTr("Turn (degrees)"); value: Math.round(panel.s.rotation !== undefined ? panel.s.rotation : 0); from: -180; to: 180
                          onChanged: (v) => TemplateEditor.setProp("rotation", v) }
                Item {
                    width: parent.cell; height: 72
                    Rectangle {
                        y: 22; width: parent.width; height: 48; radius: 8; color: "transparent"; border.width: 1; border.color: Theme.outline
                        Text { anchors.centerIn: parent; text: qsTr("Straighten"); color: Theme.primary; font.pixelSize: Theme.sp(14); font.weight: Font.Medium }
                        TapHandler { onTapped: TemplateEditor.setProp("rotation", 0) }
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Turn the item back to straight")
                        Accessible.onPressAction: TemplateEditor.setProp("rotation", 0)
                    }
                }
            }
            PrefGroup { text: qsTr("When the picture is another size") }
            Text { x: 24; width: parent.width - 48; text: panel.s.anchorText !== undefined ? panel.s.anchorText : ""; color: Theme.surfaceOn; font.pixelSize: Theme.sp(15); wrapMode: Text.WordWrap }
        }
    }
}
