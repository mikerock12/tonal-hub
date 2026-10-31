import QtQuick

Canvas {
    id: glyph
    property bool playing: false
    property color ink: "#ffffff"
    property bool animationAllowed: true
    property real mix: playing ? 1 : 0
    property bool ready: false

    Component.onCompleted: { mix = playing ? 1 : 0; ready = true; requestPaint() }
    onPlayingChanged: {
        if (!ready) { mix = playing ? 1 : 0; return }
        morph.stop()
        if (animationAllowed) { morph.to = playing ? 1 : 0; morph.restart() }
        else mix = playing ? 1 : 0
    }
    onMixChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    NumberAnimation { id: morph; target: glyph; property: "mix"; duration: 340; easing.type: Easing.InOutCubic }

    onPaint: {
        const c = getContext("2d"); c.reset(); c.save()
        c.translate((width - 24) / 2, (height - 24) / 2)
        c.fillStyle = ink
        c.globalAlpha = 1 - mix
        c.save(); c.translate(12, 12); c.rotate(mix * Math.PI * .42); c.scale(1 - mix * .22, 1 - mix * .22); c.translate(-12, -12)
        c.beginPath(); c.moveTo(7, 4); c.lineTo(20, 12); c.lineTo(7, 20); c.closePath(); c.fill(); c.restore()
        c.globalAlpha = mix
        const spread = 4 + mix * 4, barWidth = 2.4 + mix * 1.6
        c.beginPath(); c.roundedRect(12 - spread - barWidth / 2, 4 + (1 - mix) * 4, barWidth, 16 - (1 - mix) * 8, 1, 1); c.fill()
        c.beginPath(); c.roundedRect(12 + spread - barWidth / 2, 4 + (1 - mix) * 4, barWidth, 16 - (1 - mix) * 8, 1, 1); c.fill()
        c.restore()
    }
}
