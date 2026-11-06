import QtQuick
import QtQuick.Shapes

// A Material icon from Icons.qml: Icon { path: Icons.mic; color: Theme.primary; size: 24 }
Item {
    id: root
    property string path
    property color color: Theme.surfaceVariantOn
    property int size: 24
    implicitWidth: size
    implicitHeight: size
    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft
        ShapePath {
            fillColor: root.color
            strokeColor: "transparent"
            fillRule: ShapePath.WindingFill
            PathSvg { path: root.path }
        }
    }
}
