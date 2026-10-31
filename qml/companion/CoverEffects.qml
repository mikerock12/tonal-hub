import QtQuick
import QtQuick.Window

Canvas {
    id: effects
    property string themeId
    property bool animationAllowed: true
    property real phase: 0
    readonly property bool animationRunning: visible && enabled && animationAllowed && width >= 120 && height >= 120 && (themeId === "halloween" || themeId === "natal") && (!Window.window || Window.window.active)
    onPhaseChanged: requestPaint()
    onThemeIdChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onVisibleChanged: requestPaint()
    Timer { interval: 67; repeat: true; running: effects.animationRunning; onTriggered: effects.phase += .067 }

    function bulb(c, x, y, size, color, glow) {
        c.fillStyle = color; c.globalAlpha = .10 + glow * .14; c.beginPath(); c.arc(x, y, size * 2.1, 0, Math.PI * 2); c.fill()
        c.globalAlpha = .8 + glow * .2; c.beginPath(); c.ellipse(x - size * .55, y - size * .15, size * 1.1, size * 1.55); c.fill()
        c.fillStyle = "#fff8dc"; c.globalAlpha = .65; c.beginPath(); c.ellipse(x - size * .25, y, size * .18, size * .5); c.fill(); c.globalAlpha = 1
    }
    function web(c, x, y, radius, start, end, spokes, rings) {
        c.save(); c.translate(x, y)
        for (let pass = 0; pass < 2; pass++) {
            c.strokeStyle = pass ? "#d9d5d2" : "#27232b"; c.globalAlpha = pass ? .58 : .7; c.lineWidth = pass ? .65 : 1.8
            for (let i = 0; i <= spokes; i++) {
                const a = start + (end - start) * i / spokes; c.beginPath(); c.moveTo(0, 0)
                c.quadraticCurveTo(Math.cos(a + .018) * radius * .52, Math.sin(a + .018) * radius * .52, Math.cos(a) * radius, Math.sin(a) * radius); c.stroke()
            }
            for (let r = 1; r <= rings; r++) {
                const fraction = r / rings; c.beginPath()
                for (let i = 0; i <= spokes; i++) {
                    const a = start + (end - start) * i / spokes, x1 = Math.cos(a) * radius * fraction, y1 = Math.sin(a) * radius * fraction
                    if (i === 0) c.moveTo(x1, y1)
                    else { const mid = a - (end - start) / spokes * .5; c.quadraticCurveTo(Math.cos(mid) * radius * fraction * .91, Math.sin(mid) * radius * fraction * .91, x1, y1) }
                }
                c.stroke()
            }
        }
        c.restore(); c.globalAlpha = 1
    }
    function spider(c, x, y, size, weave) {
        c.strokeStyle = "#d7d1cf"; c.globalAlpha = .55; c.lineWidth = .7; c.beginPath(); c.moveTo(x, 0); c.lineTo(x, y - size * .6); c.stroke(); c.globalAlpha = 1
        if (weave > 0) { c.strokeStyle = "#c8c1c2"; c.globalAlpha = .55; c.beginPath(); for (let i = 0; i < 28; i++) { const a = i * .65, r = weave * i / 28, px = x + Math.cos(a) * r, py = y + size + Math.sin(a) * r; i ? c.lineTo(px, py) : c.moveTo(px, py) } c.stroke(); c.globalAlpha = 1 }
        c.save(); c.translate(x, y); c.strokeStyle = "#17131a"; c.lineWidth = Math.max(1, size * .14); c.lineCap = "round"
        for (let side = -1; side <= 1; side += 2) for (let i = 0; i < 4; i++) {
            const yy = (i - 1.5) * size * .34, reach = size * (1.15 + (i === 1 || i === 2 ? .3 : 0)); c.beginPath(); c.moveTo(side * size * .35, yy)
            c.lineTo(side * size * .9, yy + (i - 1.5) * size * .25); c.lineTo(side * reach, yy + (i - 1.5) * size * .65); c.stroke()
        }
        let g = c.createRadialGradient(-size * .25, -size * .4, size * .1, 0, 0, size * 1.2); g.addColorStop(0, "#5e555e"); g.addColorStop(.35, "#282129"); g.addColorStop(1, "#08070a"); c.fillStyle = g
        c.beginPath(); c.ellipse(-size * .65, -size * .8, size * 1.3, size * 1.45); c.fill(); c.beginPath(); c.ellipse(-size * .43, size * .25, size * .86, size * .85); c.fill()
        c.fillStyle = "#a52b35"; c.globalAlpha = .75; c.beginPath(); c.moveTo(0, -size * .6); c.lineTo(size * .28, -size * .15); c.lineTo(0, size * .05); c.lineTo(-size * .28, -size * .15); c.closePath(); c.fill(); c.globalAlpha = 1
        c.fillStyle = "#d9c9b7"; for (let side = -1; side <= 1; side += 2) { c.beginPath(); c.arc(side * size * .2, size * .58, Math.max(.6, size * .07), 0, Math.PI * 2); c.fill() }
        c.restore()
    }
    onPaint: {
        const c = getContext("2d"); c.reset(); const w = width, h = height
        if (themeId === "natal") {
            const colors = ["#ff4653", "#ffd45e", "#4fd67c", "#62bfff"], count = Math.max(7, Math.floor(w / 42))
            c.strokeStyle = "#1c3525"; c.lineWidth = 2.2; c.beginPath(); c.moveTo(0, 8); c.quadraticCurveTo(w * .5, 44, w, 8); c.stroke()
            for (let i = 0; i <= count; i++) { const t = i / count, x = t * w, y = 8 + 28 * t * (1 - t) * 2, color = colors[(i + Math.floor(phase * .8)) % colors.length]; bulb(c, x, y + 5, Math.max(3, Math.min(w, h) * .018), color, .5 + .5 * Math.sin(phase * 3 + i)) }
        } else if (themeId === "halloween") {
            const m = Math.min(w, h); web(c, 0, 0, m * .62, 0, Math.PI / 2, 9, 7); web(c, w, 0, m * .31, Math.PI / 2, Math.PI, 6, 4)
            const cycle = phase % 14; let y = h * .18, weave = 0
            if (cycle >= 2 && cycle < 6) y = h * (.18 + (cycle - 2) / 4 * .52)
            else if (cycle >= 6 && cycle < 8) { y = h * .70; weave = (cycle - 6) * m * .035 }
            else if (cycle >= 8 && cycle < 12) y = h * (.70 - (cycle - 8) / 4 * .52)
            spider(c, w * .76, y, Math.max(5, Math.min(13, m * .035)), weave)
        }
    }
}
