import QtQuick
import QtQuick.Window
import "ChristmasGarlandData.js" as Data

// One side of the strands wrapped around the player cover: "back" sits under the
// art and "front" over it. Wire, sockets and shadows are a single baked image, so
// the only per-frame work is the opacity of the lit bulb sprites.
Item {
    id: garland
    property string plane: "front"
    property real coverWidth: 0
    property real coverHeight: 0
    property bool playing: false
    property bool animationAllowed: true
    property real phase: 0
    property bool ownClock: true   // false when another surface drives phase
    readonly property bool animationRunning: visible && playing && animationAllowed && (!Window.window || Window.window.active)
    readonly property real bulbSide: coverWidth * Data.bulbLength * Data.spriteSpan

    width: coverWidth * (1 + 2 * Data.margin)
    height: coverHeight * (1 + 2 * Data.margin)

    Timer { interval: 67; repeat: true; running: garland.ownClock && garland.animationRunning; onTriggered: garland.phase += .067 }

    // Four patterns take turns so the strand never settles into one rhythm. Mirrored
    // in ChristmasLights.level on Android.
    function level(i, t) {
        const h = Math.abs(Math.sin(i * 12.9898) * 43758.5453) % 1
        const slot = Math.floor(t / 7) % 4, within = (t % 7) / 7, mix = Math.min(1, Math.max(0, (within - .86) / .14))
        return .16 + .84 * (pattern(slot, i, h, t) * (1 - mix) + pattern((slot + 1) % 4, i, h, t) * mix)
    }
    function pattern(kind, i, h, t) {
        if (kind === 0) return .5 + .5 * Math.sin(t * (1.4 + 2.6 * h) + h * 6.283)
        if (kind === 1) return Math.pow(Math.max(0, Math.cos(i * .55 - t * 3.2)), 3)
        if (kind === 2) return .5 + .5 * Math.sin(t * 2.4 + (i % 2) * Math.PI)
        return Math.pow(.5 + .5 * Math.sin(t * (5 + 7 * h) + h * 40), 6)
    }

    Image {
        objectName: "garlandImage"
        anchors.fill: parent
        source: "qrc:/companion/resources/seasonal/garland_" + garland.plane + ".webp"
        asynchronous: true
        smooth: true
        mipmap: true
    }
    Repeater {
        model: garland.visible ? Data.bulbs.length : 0
        Image {
            required property int index
            readonly property var bulb: Data.bulbs[index]
            visible: bulb.layer === garland.plane
            source: visible ? "qrc:/companion/resources/seasonal/bulb_" + bulb.color + ".webp" : ""
            asynchronous: true
            smooth: true
            width: garland.bulbSide
            height: garland.bulbSide
            x: (bulb.x + Data.margin) * garland.coverWidth - width / 2
            y: (bulb.y + Data.margin) * garland.coverHeight - height / 2
            rotation: bulb.angle
            opacity: garland.level(index, garland.phase)
        }
    }
}
