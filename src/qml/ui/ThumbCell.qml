import QtQuick
import QtQuick.Effects
import QSSTV

// One thumbnail of the gallery: tap opens it (or selects, while selecting), holding selects
Item {
    id: cell
    property var info
    Rectangle {
        anchors.fill: parent
        anchors.margins: cell.info.selected ? 8 : 0
        Behavior on anchors.margins { NumberAnimation { duration: Theme.ms(90) } }
        radius: cell.info.selected ? 10 : 6
        color: Theme.surfaceHigh
        clip: true
        Image {
            anchors.fill: parent
            source: cell.info.thumb
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
            smooth: true
            cache: true
            layer.enabled: Theme.tint
            layer.effect: MultiEffect { colorization: 1.0; colorizationColor: "#e8341f" }
        }
        Rectangle {
            visible: cell.info.badge !== ""
            anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 4
            radius: 4; color: Theme.pictureShade
            width: badgeText.implicitWidth + 10; height: badgeText.implicitHeight + 2
            Text { id: badgeText; anchors.centerIn: parent; text: cell.info.badge; color: Theme.onPicture; font.pixelSize: Theme.sp(12); font.weight: Font.Bold }
        }
        Rectangle {
            visible: cell.info.isTemplate
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 4
            radius: 4; color: Theme.pictureShade
            width: tmplText.implicitWidth + 10; height: tmplText.implicitHeight + 2
            Text { id: tmplText; anchors.centerIn: parent; text: qsTr("Template"); color: Theme.onPicture; font.pixelSize: Theme.sp(12); font.weight: Font.Bold }
        }
    }
    Rectangle {
        visible: cell.info.selected
        x: 10; y: 10; width: 24; height: 24; radius: 12
        color: Theme.primary
        Icon { anchors.centerIn: parent; path: Icons.check; size: 16; color: Theme.primaryOn }
    }
    TapHandler { margin: Theme.tapMargin(parent);
        onTapped: GalleryModel.tap(cell.info.index)
        onLongPressed: { GalleryModel.startSelecting(cell.info.index); AppModel.haptic(1) }
    }
    Accessible.role: Accessible.Button
    Accessible.name: (cell.info.badge !== "" ? cell.info.badge + ", " : "") + cell.info.name
    Accessible.selected: cell.info.selected
    Accessible.onPressAction: GalleryModel.tap(cell.info.index)
}
