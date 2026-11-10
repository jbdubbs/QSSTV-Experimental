import QtQuick

// A settings row with a radio button: one of several choices that are all in view (Appearance). A tap anywhere on the row chooses it.
Item {
    id: row
    property string title
    property string summary
    property bool checked: false
    signal clicked()
    width: parent ? parent.width : 360
    implicitHeight: Math.max(56, labels.implicitHeight + 16)
    RadioMark { x: 20; anchors.verticalCenter: parent.verticalCenter; checked: row.checked }
    Column {
        id: labels
        x: 72; width: parent.width - 96
        anchors.verticalCenter: parent.verticalCenter
        Text { width: parent.width; text: row.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); wrapMode: Text.WordWrap }
        Text { visible: row.summary !== ""; width: parent.width; text: row.summary; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); wrapMode: Text.WordWrap }
    }
    TapHandler { margin: Theme.tapMargin(parent); onTapped: row.clicked() }
    Accessible.role: Accessible.RadioButton
    Accessible.name: summary !== "" ? title + ", " + summary : title
    Accessible.checked: row.checked
    Accessible.onPressAction: row.clicked()
}
