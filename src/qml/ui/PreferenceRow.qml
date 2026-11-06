import QtQuick
import QtQuick.Controls.Material

// A settings row: title, a line of state under it, and a switch or a chevron on the right
Item {
    id: row
    property string title
    property string summary
    property bool checkable: false
    property bool checked: false
    property bool chevron: false
    signal toggled(bool checked)
    signal clicked()
    width: parent ? parent.width : 360
    implicitHeight: Math.max(56, text.implicitHeight + 24)
    Column {
        id: text
        anchors.left: parent.left; anchors.leftMargin: 24
        anchors.right: trailing.left; anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        Text { text: row.title; color: Theme.surfaceOn; font.pixelSize: Theme.sp(16); width: parent.width; wrapMode: Text.WordWrap }
        Text { visible: row.summary !== ""; text: row.summary; color: Theme.surfaceVariantOn; font.pixelSize: Theme.sp(13); width: parent.width; wrapMode: Text.WordWrap }
    }
    Item {
        id: trailing
        anchors.right: parent.right; anchors.rightMargin: 24
        anchors.verticalCenter: parent.verticalCenter
        width: row.checkable ? sw.implicitWidth : (row.chevron ? 24 : 0)
        height: Theme.tap
        Switch { id: sw; visible: row.checkable; anchors.centerIn: parent; checked: row.checked; onToggled: row.toggled(checked)
            Accessible.name: row.title }
        Icon { visible: row.chevron; anchors.centerIn: parent; path: Icons.chevron }
    }
    TapHandler { enabled: !row.checkable; onTapped: row.clicked() }
    TapHandler { enabled: row.checkable; onTapped: { sw.toggle(); row.toggled(sw.checked) } }
    Accessible.role: checkable ? Accessible.CheckBox : Accessible.Button
    Accessible.name: summary !== "" ? title + ", " + summary : title
}
