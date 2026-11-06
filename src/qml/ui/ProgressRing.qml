import QtQuick
import QtQuick.Shapes

// A ring that fills as value goes from 0 to 1; its content goes in the middle
Item {
    id: ring
    property real value: 0
    property real thickness: 12
    default property alias content: inner.data
    implicitWidth: 180
    implicitHeight: 180
    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.GeometryRenderer
        ShapePath {
            strokeColor: Theme.outlineVariant
            strokeWidth: ring.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.width / 2 - ring.thickness / 2; radiusY: ring.height / 2 - ring.thickness / 2
                startAngle: -90; sweepAngle: 359.9
            }
        }
        ShapePath {
            strokeColor: Theme.primary
            strokeWidth: ring.thickness
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.width / 2; centerY: ring.height / 2
                radiusX: ring.width / 2 - ring.thickness / 2; radiusY: ring.height / 2 - ring.thickness / 2
                startAngle: -90; sweepAngle: Math.max(0.1, Math.min(1, ring.value)) * 359.9
            }
        }
    }
    Item { id: inner; anchors.fill: parent }
    Accessible.role: Accessible.ProgressBar
}
