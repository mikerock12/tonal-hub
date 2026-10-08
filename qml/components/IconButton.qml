import QtQuick
import QtQuick.Controls

// Botão só com ícone (controles do player, barra superior etc.)
Item {
    id: root

    property string fillPath: ""
    property string strokePath: ""
    property real iconSize: 20
    property real strokeWidth: 2
    property bool active: false          // ex.: aleatório ligado
    property bool showActiveDot: false   // pontinho embaixo, como no app oficial
    property string tooltip: ""
    property color baseColor: Theme.textSecondary
    property color hoverColor: Theme.textPrimary
    property color background: "transparent"
    property color backgroundHover: background
    property bool circle: false
    readonly property bool hovered: mouse.containsMouse

    signal clicked()

    implicitWidth: iconSize + 16
    implicitHeight: iconSize + 16
    opacity: enabled ? 1 : 0.35

    Rectangle {
        anchors.fill: parent
        radius: root.circle ? width / 2 : 4
        color: mouse.containsMouse ? root.backgroundHover : root.background
        visible: color.a > 0
    }

    Icon {
        id: icon
        anchors.centerIn: parent
        size: root.iconSize
        fillPath: root.fillPath
        strokePath: root.strokePath
        strokeWidth: root.strokeWidth
        color: root.active ? (mouse.containsMouse ? Theme.accentHover : Theme.accent)
                           : (mouse.containsMouse ? root.hoverColor : root.baseColor)
        scale: mouse.pressed ? 0.92 : (mouse.containsMouse ? 1.04 : 1)
        Behavior on scale { NumberAnimation { duration: 90 } }
    }

    Rectangle {
        visible: root.active && root.showActiveDot
        width: 4; height: 4; radius: 2
        color: Theme.accent
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: icon.bottom
        anchors.topMargin: 2
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    ToolTip.visible: tooltip !== "" && mouse.containsMouse
    ToolTip.text: tooltip
    ToolTip.delay: 700
}
