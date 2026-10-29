import QtQuick

// Barra fina de progresso e volume. Trilho, preenchimento (degradê) e marcador
// seguem o tema escolhido (Theme.seek). Enquanto arrasta, mostra só a posição
// arrastada: o valor vindo do player não puxa o marcador de volta.
Item {
    id: root

    property real value: 0                // 0..1 vindo do modelo
    property bool wheelEnabled: false     // roda do mouse ajusta (volume)
    readonly property bool dragging: mouse.pressed
    readonly property bool hovered: mouse.containsMouse
    readonly property bool active: hovered || dragging
    property real dragValue: 0
    property real wheelRemainder: 0
    property double wheelStamp: 0
    readonly property real shownValue: dragging ? dragValue : Math.max(0, Math.min(1, value))

    property var styleOverride: null      // pré-visualização de outro tema
    readonly property var style: styleOverride || Theme.seek
    readonly property int barHeight: active ? style.activeHeight : style.height
    readonly property var fillStops: active ? style.activeFill : style.fill
    property bool showThumb: false        // força o marcador (pré-visualização)
    readonly property bool thumbVisible: enabled && (style.thumbAlways || active || showThumb)
    readonly property int thumbSize: style.thumbSize + (dragging ? 2 : 0)

    signal moving(real v)   // enquanto arrasta
    signal moved(real v)    // ao soltar

    implicitHeight: Math.max(16, style.thumbSize + 6)

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: root.barHeight
        radius: height / 2
        color: root.style.track

        Rectangle {
            width: Math.max(height, parent.width * root.shownValue)
            visible: root.shownValue > 0
            height: parent.height
            radius: height / 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: root.fillStops[0] }
                GradientStop { position: 1; color: root.fillStops[root.fillStops.length - 1] }
            }
        }
    }

    Repeater {
        model: root.style.thumbShape === "drop" && root.shownValue > 0 ? 7 : 0
        Rectangle {
            x: track.width * root.shownValue * (index + 1) / 8
            y: track.y + track.height / 2
            width: 2 + index % 2; height: 4 + (index * 7 % 8)
            radius: width / 2; color: root.fillStops[0]
        }
    }

    Item {
        id: thumb
        visible: root.thumbVisible
        width: root.style.thumbShape === "pill" ? Math.round(root.thumbSize * 1.6) : root.thumbSize
        height: root.style.thumbShape === "pill" ? Math.round(root.thumbSize * 0.8) : root.thumbSize
        anchors.verticalCenter: parent.verticalCenter
        x: Math.max(-width / 2, Math.min(track.width - width / 2, track.width * root.shownValue - width / 2))

        Canvas {
            anchors.fill: parent
            visible: root.style.thumbShape === "drop"
            onVisibleChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c=getContext("2d"); c.reset(); c.fillStyle=root.style.thumb; c.strokeStyle=root.style.thumbBorder; c.lineWidth=1.5
                c.beginPath(); c.moveTo(width/2,0); c.bezierCurveTo(width*.35,height*.3,0,height*.55,width*.13,height*.77); c.bezierCurveTo(width*.28,height*1.08,width*.72,height*1.08,width*.87,height*.77); c.bezierCurveTo(width,height*.55,width*.65,height*.3,width/2,0); c.closePath(); c.fill(); c.stroke()
            }
        }
        // Halo do tema Neon: um retângulo translúcido, sem efeito de shader.
        Rectangle {
            visible: root.style.glow
            anchors.centerIn: parent
            width: parent.width + 10
            height: parent.height + 10
            radius: height / 2
            color: root.style.thumbBorder
            opacity: root.dragging ? 0.45 : 0.28
        }
        Rectangle {
            visible: root.style.thumbShape !== "drop"
            anchors.centerIn: parent
            width: root.style.thumbShape === "diamond" ? Math.round(parent.width * 0.75) : parent.width
            height: root.style.thumbShape === "diamond" ? Math.round(parent.height * 0.75) : parent.height
            rotation: root.style.thumbShape === "diamond" ? 45 : 0
            radius: root.style.thumbShape === "diamond" ? 2 : height / 2
            color: root.style.thumb
            border.width: root.style.thumbShape === "ring" ? 3 : (root.style.thumbBorder !== root.style.thumb ? 2 : 0)
            border.color: root.style.thumbBorder
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        anchors.topMargin: -4
        anchors.bottomMargin: -4
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.PointingHandCursor

        function valueAt(px) { return Math.max(0, Math.min(1, px / Math.max(1, width - 1))) }

        onPressed: (e) => { root.wheelRemainder = 0; root.dragValue = valueAt(e.x); root.moving(root.dragValue) }
        onPositionChanged: (e) => {
            if (pressed) {
                root.dragValue = valueAt(e.x)
                root.moving(root.dragValue)
            }
        }
        onReleased: (e) => root.moved(valueAt(e.x))
        onWheel: (w) => {
            if (!root.wheelEnabled) { w.accepted = false; return }
            // Two percentage points per detent (120 units). Accumulate fine
            // touchpad deltas; horizontal/phase-only events never lower sound.
            const delta = w.angleDelta.y !== 0 ? w.angleDelta.y / 60 : w.pixelDelta.y / 20
            if (delta === 0) { w.accepted = false; return }
            const now = Date.now()
            if (now - root.wheelStamp > 500 || delta * root.wheelRemainder < 0)
                root.wheelRemainder = 0
            root.wheelStamp = now
            root.wheelRemainder += delta
            const steps = Math.trunc(root.wheelRemainder)
            root.wheelRemainder -= steps
            if (steps !== 0)
                root.moved(Math.max(0, Math.min(1, root.value + steps / 100)))
        }
    }
}
