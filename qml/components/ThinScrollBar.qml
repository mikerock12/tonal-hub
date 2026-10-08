import QtQuick
import QtQuick.Controls

// Barra de rolagem fina que só aparece ao rolar/passar o mouse.
ScrollBar {
    id: bar
    policy: ScrollBar.AsNeeded
    contentItem: Rectangle {
        implicitWidth: 10
        color: Qt.rgba(1, 1, 1, bar.pressed ? 0.5 : 0.3)
        opacity: bar.active || bar.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 300 } }
    }
    background: null
}
