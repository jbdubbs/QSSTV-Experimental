import QtQuick
import QtQuick.Templates as T

// A slider in the colors of the look: a thin track with the part before the handle filled, a round handle
T.Slider {
    id: control
    implicitWidth: 200
    implicitHeight: 48
    padding: 12
    background: Item {
        x: control.leftPadding; y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth; height: 6
        opacity: control.enabled ? 1 : 0.4
        Rectangle { anchors.fill: parent; radius: 3; color: Theme.outlineVariant }
        Rectangle { width: control.visualPosition * parent.width; height: parent.height; radius: 3; color: Theme.primary }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.pressed ? 24 : 20; height: width; radius: width / 2
        color: Theme.primary
        opacity: control.enabled ? 1 : 0.4
    }
}
