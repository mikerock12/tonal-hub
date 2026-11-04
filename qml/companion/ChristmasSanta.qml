import QtQuick
import QtQuick.Window

// Corner scene of the player cover. Frames decode one at a time (cache: false keeps
// memory flat) and only while music plays; otherwise the current frame is held.
AnimatedImage {
    id: santa
    property bool musicPlaying: false
    property bool animationAllowed: true
    readonly property bool animationRunning: visible && musicPlaying && animationAllowed && (!Window.window || Window.window.active)
    source: visible ? "qrc:/companion/resources/seasonal/santa.webp" : ""
    asynchronous: true
    cache: false
    smooth: true
    fillMode: Image.PreserveAspectFit
    paused: !animationRunning
}
