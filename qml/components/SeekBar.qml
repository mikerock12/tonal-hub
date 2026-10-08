import QtQuick

// Barra fina estilo Spotify (progresso e volume): fica na cor de destaque e
// mostra o "pino" ao passar o mouse. Mais leve que um Slider completo.
Item {
    id: root

    property real value: 0                // 0..1 vindo do modelo
    property bool wheelEnabled: false     // roda do mouse ajusta (volume)
    readonly property bool dragging: mouse.pressed
    readonly property bool hovered: mouse.containsMouse
    property real dragValue: 0
    readonly property real shownValue: dragging ? dragValue : Math.max(0, Math.min(1, value))

    signal moving(real v)   // enquanto arrasta
    signal moved(real v)    // ao soltar

    implicitHeight: 16

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: 2
        color: Theme.trackBg

        Rectangle {
            width: parent.width * root.shownValue
            height: parent.height
            radius: 2
            color: root.hovered || root.dragging ? Theme.accent : Theme.textPrimary
        }
    }

    Rectangle {
        visible: root.hovered || root.dragging
        width: 12
        height: 12
        radius: 6
        color: Theme.textPrimary
        anchors.verticalCenter: parent.verticalCenter
        x: track.width * root.shownValue - width / 2
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.topMargin: -4
        anchors.bottomMargin: -4
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.PointingHandCursor

        function valueAt(px) { return Math.max(0, Math.min(1, px / width)) }

        onPressed: (e) => { root.dragValue = valueAt(e.x); root.moving(root.dragValue) }
        onPositionChanged: (e) => {
            if (pressed) {
                root.dragValue = valueAt(e.x)
                root.moving(root.dragValue)
            }
        }
        onReleased: (e) => root.moved(valueAt(e.x))
        onWheel: (w) => {
            if (!root.wheelEnabled) { w.accepted = false; return }
            const step = w.angleDelta.y > 0 ? 0.05 : -0.05
            root.moved(Math.max(0, Math.min(1, root.value + step)))
        }
    }
}
