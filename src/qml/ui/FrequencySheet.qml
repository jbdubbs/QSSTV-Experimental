import QtQuick
import QtQuick.Controls.Material
import QSSTV

// One entry of the frequency list: a new one, or one to change. A change to an old entry is kept at once; a new one is added with the button.
BottomSheet {
    id: sheet
    property int editIndex: -1          // -1: a new entry
    property string freq: ""
    property bool drm: false
    property string mod: "USB"
    property string pass: "Normal"
    readonly property bool valid: parseFloat(freq.replace(",", ".")) > 0
    title: editIndex >= 0 ? qsTr("%1 MHz").arg(freq) : qsTr("New frequency")

    function openNew() { editIndex = -1; freq = ""; drm = AppModel.digital; mod = drm ? "PKTUSB" : "USB"; pass = "Normal"; open = true }
    function openEdit(index) {
        const e = Frequencies.entry(index)
        editIndex = index; freq = e.freq; drm = e.drm; mod = e.mod; pass = e.pass
        open = true
    }
    function apply() { if (editIndex >= 0 && valid) Frequencies.update(editIndex, freq, drm, mod, pass) }
    onOpenChanged: if (!open) { field.focus = false; Qt.inputMethod.hide(); apply() }

    Item {
        width: parent.width; height: field.implicitHeight + 16
        Field {
            id: field
            x: 24; y: 4; width: parent.width - 48
            text: sheet.freq
            placeholderText: qsTr("Frequency in MHz")
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            font.pixelSize: Theme.sp(22)
            color: Theme.surfaceOn; Material.accent: Theme.primary
            onTextEdited: sheet.freq = text
            onEditingFinished: sheet.apply()
            Accessible.name: qsTr("Frequency in MHz")
        }
    }
    Column {
        width: parent.width
        spacing: 4
        Text { text: qsTr("For"); color: Theme.primary; font.pixelSize: Theme.sp(13); font.weight: Font.Medium; leftPadding: 24; topPadding: 6 }
        Flow {
            x: 24; width: parent.width - 48; spacing: 8
            Chip { text: qsTr("Analog SSTV"); icon: !sheet.drm ? Icons.check : ""; tone: !sheet.drm ? "selected" : "plain"; onClicked: { sheet.drm = false; sheet.apply() } }
            Chip { text: qsTr("Digital (DRM)"); icon: sheet.drm ? Icons.check : ""; tone: sheet.drm ? "selected" : "plain"; onClicked: { sheet.drm = true; sheet.apply() } }
        }
        Text { text: qsTr("Radio mode"); color: Theme.primary; font.pixelSize: Theme.sp(13); font.weight: Font.Medium; leftPadding: 24; topPadding: 8 }
        Flow {
            x: 24; width: parent.width - 48; spacing: 8
            Repeater {
                model: Frequencies.modulations()
                delegate: Chip {
                    required property string modelData
                    text: modelData; icon: sheet.mod === modelData ? Icons.check : ""; tone: sheet.mod === modelData ? "selected" : "plain"
                    onClicked: { sheet.mod = modelData; sheet.apply() }
                }
            }
        }
        Text { text: qsTr("Passband"); color: Theme.primary; font.pixelSize: Theme.sp(13); font.weight: Font.Medium; leftPadding: 24; topPadding: 8 }
        Flow {
            x: 24; width: parent.width - 48; spacing: 8
            Repeater {
                model: Frequencies.passbands()
                delegate: Chip {
                    required property string modelData
                    text: modelData; icon: sheet.pass === modelData ? Icons.check : ""; tone: sheet.pass === modelData ? "selected" : "plain"
                    onClicked: { sheet.pass = modelData; sheet.apply() }
                }
            }
        }
        Text {
            text: qsTr("Not used with the PKT modes (at least not on Icom).")
            color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(12); leftPadding: 24; rightPadding: 24; topPadding: 4; width: parent.width; wrapMode: Text.WordWrap
        }
        // a new entry: add it
        Item {
            visible: sheet.editIndex < 0
            width: parent.width; height: 72
            WideButton {
                x: 16; y: 8; width: parent.width - 32
                text: qsTr("Add"); icon: Icons.add; enabledLook: sheet.valid
                onClicked: { if (Frequencies.add(sheet.freq, sheet.drm, sheet.mod, sheet.pass) >= 0) sheet.open = false }
            }
        }
        // an old one: where it stands in its list, or gone
        Row {
            visible: sheet.editIndex >= 0
            x: 16; width: parent.width - 32; height: 64; topPadding: 8; spacing: 8
            WideButton { width: (parent.width - 16) / 3; height: 48; text: qsTr("Up"); icon: Icons.up; tonal: true; onClicked: sheet.editIndex = Frequencies.move(sheet.editIndex, -1) }
            WideButton { width: (parent.width - 16) / 3; height: 48; text: qsTr("Down"); icon: Icons.down; tonal: true; onClicked: sheet.editIndex = Frequencies.move(sheet.editIndex, 1) }
            WideButton { width: (parent.width - 16) / 3; height: 48; text: qsTr("Delete"); icon: Icons.remove; danger: true; onClicked: { const i = sheet.editIndex; sheet.open = false; Frequencies.remove(i) } }
        }
        Item { width: 1; height: 8 }
    }
}
