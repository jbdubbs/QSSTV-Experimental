import QtQuick
import QtQuick.Controls.Material

// A settings row with a text field: what is typed is kept when the field is left (no OK button)
Item {
    id: row
    property string label
    property string text
    property string hint
    property int capitalization: Qt.ImhNone
    signal committed(string text)
    width: parent ? parent.width : 360
    implicitHeight: field.implicitHeight + 16
    TextField {
        id: field
        x: 24; y: 8
        width: parent.width - 48
        text: row.text
        placeholderText: row.hint
        inputMethodHints: Qt.ImhNoPredictiveText | row.capitalization
        Material.accent: Theme.primary
        color: Theme.surfaceOn
        placeholderTextColor: Theme.outline
        font.pixelSize: Theme.sp(16)
        onEditingFinished: { row.committed(text); cursorPosition = 0 }   // a long text shows its beginning when the field is left
        onActiveFocusChanged: if (!activeFocus) cursorPosition = 0
        Accessible.name: row.label
        Accessible.role: Accessible.EditableText
    }
    Binding { target: field; property: "text"; value: row.text; when: !field.activeFocus }
}
