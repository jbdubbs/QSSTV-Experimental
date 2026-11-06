import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts

// The top bar of a screen: its title, the route chip (where the sound goes) and the menu
Item {
    id: bar
    property string title
    property string routeText
    property alias actions: extra.data     // more icon buttons before the menu
    signal routeClicked()
    signal settingsClicked()
    signal helpClicked()
    signal aboutClicked()
    implicitHeight: Theme.appBarHeight

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.margin
        spacing: 6
        Text {
            id: titleText
            text: bar.title
            color: Theme.surfaceOn
            font.pixelSize: Theme.sp(22)
            Layout.fillWidth: true
            Layout.minimumWidth: Math.min(implicitWidth, bar.width * 0.4)
            elide: Text.ElideRight
            Accessible.role: Accessible.Heading
            Accessible.name: bar.title
        }
        Rectangle {
            id: routeChip
            visible: bar.routeText !== ""
            implicitHeight: 32
            implicitWidth: routeRow.implicitWidth + 20
            Layout.maximumWidth: routeRow.implicitWidth + 20
            Layout.minimumWidth: 56
            radius: 8
            color: "transparent"
            border.width: 1
            border.color: Theme.outline
            Row {
                id: routeRow
                anchors.centerIn: parent
                spacing: 6
                Icon { path: Icons.phone; size: 18; color: Theme.good; anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: bar.routeText
                    color: Theme.surfaceVariantOn
                    font.pixelSize: Theme.sp(13)
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                    width: Math.min(implicitWidth, Math.max(0, routeChip.width - 20 - 24))
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            TapHandler { onTapped: bar.routeClicked() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Sound goes to %1").arg(bar.routeText)
        }
        Row { id: extra }
        Item {
            implicitWidth: Theme.tap
            implicitHeight: Theme.tap
            Icon { anchors.centerIn: parent; path: Icons.more; color: Theme.surfaceVariantOn }
            TapHandler { onTapped: menu.open() }
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("More options")
            Menu {
                id: menu
                y: parent.height
                MenuItem { text: qsTr("Settings"); onTriggered: bar.settingsClicked() }
                MenuItem { text: qsTr("Help"); onTriggered: bar.helpClicked() }
                MenuItem { text: qsTr("About"); onTriggered: bar.aboutClicked() }
            }
        }
    }
}
