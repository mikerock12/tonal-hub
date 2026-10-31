import QtQuick
import QtQuick.Window

Canvas {
    id: lights
    property real progress: 0
    property real barHeight: 6
    property real thumbSize: 22
    property real trackInset: 0
    property bool showThumb: true
    property bool playing: false
    property bool animationAllowed: true
    property real phase: 0
    readonly property bool animationRunning: visible && enabled && playing && animationAllowed && (!Window.window || Window.window.active)
    implicitHeight: 44
    onProgressChanged: requestPaint()
    onPhaseChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onShowThumbChanged: requestPaint()
    Timer { interval: 67; repeat: true; running: lights.animationRunning; onTriggered: lights.phase += .067 }

    function bulb(c, x, y, size, color, lit, glow) {
        if (lit) { c.globalAlpha = .12 + glow * .15; c.fillStyle = color; c.beginPath(); c.arc(x, y + size * .45, size * 1.65, 0, Math.PI * 2); c.fill(); c.globalAlpha = 1 }
        c.fillStyle = "#263d32"; c.fillRect(x - size * .27, y - size * .35, size * .54, size * .42)
        c.fillStyle = lit ? color : "#63736a"; c.beginPath(); c.ellipse(x - size * .48, y, size * .96, size * 1.25); c.fill()
        if (lit) { c.fillStyle = "#fff7d6"; c.globalAlpha = .55; c.beginPath(); c.ellipse(x - size * .22, y + size * .16, size * .17, size * .36); c.fill(); c.globalAlpha = 1 }
    }
    function star(c, x, y, outer, inner) {
        c.save(); c.translate(x, y); c.beginPath()
        for (let i = 0; i < 10; i++) { const a = -Math.PI / 2 + i * Math.PI / 5, r = i % 2 ? inner : outer, px = Math.cos(a) * r, py = Math.sin(a) * r; i ? c.lineTo(px, py) : c.moveTo(px, py) }
        c.closePath(); const g = c.createLinearGradient(-outer, -outer, outer, outer); g.addColorStop(0, "#fff7bd"); g.addColorStop(.45, "#ffd45e"); g.addColorStop(1, "#c78618"); c.fillStyle = g; c.fill()
        c.strokeStyle = "#fff4c2"; c.lineWidth = 1; c.stroke(); c.restore()
    }
    onPaint: {
        const c = getContext("2d"); c.reset(); c.translate(trackInset, 0)
        const w = Math.max(0, width - 2 * trackInset), cy = height / 2 - 2, amount = Math.max(0, Math.min(1, progress)), edge = w * amount
        c.strokeStyle = "#345c43"; c.lineWidth = Math.max(2, barHeight * .42); c.beginPath(); c.moveTo(0, cy)
        for (let x = 0; x <= w; x += 8) c.lineTo(x, cy + Math.sin(x * .16) * 1.2)
        c.stroke(); c.strokeStyle = "#172e22"; c.lineWidth = 1; c.beginPath(); c.moveTo(0, cy - 1); c.lineTo(w, cy - 1); c.stroke()
        const colors = ["#ff4653", "#ffd45e", "#4fd67c", "#62bfff"], count = Math.max(5, Math.floor(w / 42))
        for (let i = 0; i <= count; i++) {
            const x = w * i / count, lit = x <= edge + .5, color = colors[(i + Math.floor(phase * .9)) % colors.length]
            bulb(c, x, cy + 2, Math.max(4, thumbSize * .25), color, lit, .5 + .5 * Math.sin(phase * 4 + i))
        }
        if (showThumb) { c.globalAlpha = .22; c.fillStyle = "#ffe795"; c.beginPath(); c.arc(edge, cy, thumbSize * .85, 0, Math.PI * 2); c.fill(); c.globalAlpha = 1; star(c, edge, cy, thumbSize * .62, thumbSize * .28) }
    }
}
