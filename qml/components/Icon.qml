import QtQuick
import QtQuick.Shapes

// Ícone vetorial leve: desenha paths SVG (grid 24x24) com QtQuick.Shapes.
// Sem PNGs, sem fontes de ícone, nítido em qualquer escala de DPI.
Item {
    id: root

    property string fillPath: ""
    property string strokePath: ""
    property color color: Theme.textPrimary
    property real strokeWidth: 2
    property real size: 24

    implicitWidth: size
    implicitHeight: size

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: root.size / 24
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: root.fillPath !== "" ? root.color : "transparent"
            strokeWidth: -1
            strokeColor: "transparent"
            PathSvg { path: root.fillPath !== "" ? root.fillPath : "M0 0" }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: root.strokePath !== "" ? root.color : "transparent"
            strokeWidth: root.strokePath !== "" ? root.strokeWidth : -1
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.strokePath !== "" ? root.strokePath : "M0 0" }
        }
    }
}
