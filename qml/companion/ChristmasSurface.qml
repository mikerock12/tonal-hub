import QtQuick
import QtQuick.Window

// Seek bar as a light string. Each bulb stays dark until the star reaches it, then
// flares and settles into the twinkle; seeking back switches it off again.
Item {
    id: lights
    property real progress: 0
    property real barHeight: 6
    property real thumbSize: 22
    property real trackInset: 0
    property bool showThumb: true
    property bool playing: false
    property bool animationAllowed: true
    property real phase: 0
    property bool ownClock: true   // false when a page drives phase, so surfaces tick together
    readonly property bool animationRunning: visible && enabled && playing && animationAllowed && (!Window.window || Window.window.active)
    readonly property real trackWidth: Math.max(0, width - 2 * trackInset)
    readonly property real edge: trackWidth * Math.max(0, Math.min(1, progress))
    readonly property int count: Math.max(6, Math.floor(trackWidth / 28))
    readonly property real wireY: height / 2
    readonly property real bulbLength: Math.max(14, Math.min(19, thumbSize * .8))
    readonly property var colors: ["red", "gold", "green", "blue"]
    implicitHeight: 68
    Timer { interval: 67; repeat: true; running: lights.ownClock && lights.animationRunning; onTriggered: lights.phase += .067 }

    function twinkle(i, t) {
        const h = Math.abs(Math.sin(i * 12.9898) * 43758.5453) % 1
        return .58 + .42 * (.5 + .5 * Math.sin(t * (1.8 + 3 * h) + h * 6.283))
    }

    Image {
        x: lights.trackInset
        y: lights.wireY - height / 2
        width: lights.trackWidth
        height: 16
        source: "qrc:/companion/resources/seasonal/wire.webp"
        sourceSize: Qt.size(48, 16)
        fillMode: Image.TileHorizontally
        verticalAlignment: Image.AlignVCenter
        smooth: true
    }

    Repeater {
        model: lights.count
        Item {
            id: lamp
            required property int index
            readonly property real position: lights.trackWidth * (index + .5) / lights.count
            readonly property bool lit: lights.edge >= position
            readonly property bool upright: index % 3 === 2
            readonly property real wobble: Math.sin(index * 2.4) * 12
            readonly property real side: lights.bulbLength * 3.5
            property real flare: 0
            property real level: lit ? 1 : 0
            Behavior on level { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
            onLitChanged: if (lit) ignite.restart()
            NumberAnimation { id: ignite; target: lamp; property: "flare"; from: 1; to: 0; duration: 760; easing.type: Easing.OutQuad }
            x: lights.trackInset + position
            y: lights.wireY
            rotation: (upright ? 0 : 180) + wobble
            Image {
                x: -width / 2; y: -height * .92
                width: lights.bulbLength * .46; height: lights.bulbLength * .52
                source: "qrc:/companion/resources/seasonal/socket.webp"
                smooth: true
            }
            Item { id: glass; x: 0; y: -lights.bulbLength * .92 - lights.bulbLength * .36; width: 0; height: 0 }
            Image {
                anchors.centerIn: glass; width: lamp.side; height: lamp.side
                source: "qrc:/companion/resources/seasonal/bulb_" + lights.colors[lamp.index % 4] + "_off.webp"
                smooth: true
            }
            Image {
                anchors.centerIn: glass; width: lamp.side; height: lamp.side
                source: "qrc:/companion/resources/seasonal/bulb_" + lights.colors[lamp.index % 4] + ".webp"
                smooth: true
                visible: lamp.level > .01
                scale: 1 + lamp.flare * .42
                opacity: Math.min(1, lamp.level * (lights.twinkle(lamp.index, lights.phase) + lamp.flare))
            }
        }
    }

    Item {
        id: star
        visible: lights.showThumb
        x: lights.trackInset + lights.edge
        y: lights.wireY
        readonly property real pulse: .5 + .5 * Math.sin(lights.phase * 2.7)
        Image {
            anchors.centerIn: parent
            width: lights.thumbSize * 6.4; height: width
            source: "qrc:/companion/resources/seasonal/star_glow.webp"
            smooth: true
            rotation: lights.phase * 9
            opacity: .62 + .38 * star.pulse
            scale: .92 + .14 * star.pulse
        }
        Image {
            anchors.centerIn: parent
            width: lights.thumbSize * 3.8; height: width
            source: "qrc:/companion/resources/seasonal/star.webp"
            smooth: true
        }
    }
}
