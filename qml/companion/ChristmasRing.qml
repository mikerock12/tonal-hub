import QtQuick
import QtQuick.Window

Canvas {
    id: ring
    property bool animationAllowed: true
    property real phase: 0
    readonly property bool animationRunning: visible && enabled && animationAllowed && Window.window && Window.window.active
    onPhaseChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Timer { interval: 67; repeat: true; running: ring.animationRunning; onTriggered: ring.phase += .067 }

    onPaint: {
        const c = getContext("2d"); c.reset(); const cx = width / 2, cy = height / 2, r = Math.min(width, height) * .46
        c.fillStyle = "#0c2118"; c.beginPath(); c.arc(cx, cy, r, 0, Math.PI * 2); c.fill()
        c.lineWidth = Math.max(2, r * .15); c.strokeStyle = "#24583b"; c.beginPath(); c.arc(cx, cy, r * .91, 0, Math.PI * 2); c.stroke()
        c.lineWidth = Math.max(1, r * .045); c.strokeStyle = "#b88a35"; c.globalAlpha = .8; c.beginPath(); c.arc(cx, cy, r * .78, 0, Math.PI * 2); c.stroke(); c.globalAlpha = 1
        const colors = ["#ff4b55", "#ffd86a", "#63d98b", "#76c9ff"]
        for (let i = 0; i < 12; i++) {
            const a = i * Math.PI * 2 / 12 - Math.PI / 2, x = cx + Math.cos(a) * r * .86, y = cy + Math.sin(a) * r * .86
            const glow = .45 + .4 * (.5 + .5 * Math.sin(phase * 3 + i * 1.7))
            c.fillStyle = colors[(i + Math.floor(phase)) % colors.length]; c.globalAlpha = glow * .35; c.beginPath(); c.arc(x, y, Math.max(2, r * .13), 0, Math.PI * 2); c.fill()
            c.globalAlpha = .65 + glow * .35; c.beginPath(); c.arc(x, y, Math.max(1.2, r * .055), 0, Math.PI * 2); c.fill()
        }
        c.globalAlpha = 1
    }
}
