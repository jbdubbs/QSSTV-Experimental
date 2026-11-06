import QtQuick
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQml

// The top bar of a screen: its title, the route chip (where the sound goes) and the menu
Item {
    id: bar
    property string title
    property string routeText
    property alias actions: extra.data     // more icon buttons before the menu
    property var entries: []               // the menu: [{id, text}]
    signal routeClicked()
    signal entryTriggered(string id)
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
        TextMetrics {   // how wide the route text wants to be, whatever room the chip is given
            id: routeMetrics
            text: bar.routeText
            font.pixelSize: Theme.sp(13)
            font.weight: Font.Medium
        }
        Rectangle {
            id: routeChip
            visible: bar.routeText !== ""
            implicitHeight: 32
            implicitWidth: routeMetrics.width + 18 + 6 + 20
            Layout.maximumWidth: routeMetrics.width + 18 + 6 + 20
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
                    width: Math.min(routeMetrics.width, Math.max(0, routeChip.width - 20 - 24))
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
                Instantiator {
                    model: bar.entries
                    delegate: MenuItem {
                        required property var modelData
                        text: modelData.text
                        onTriggered: bar.entryTriggered(modelData.id)
                    }
                    onObjectAdded: (index, object) => menu.insertItem(index, object)
                    onObjectRemoved: (index, object) => menu.removeItem(object)
                }
            }
        }
    }
}
