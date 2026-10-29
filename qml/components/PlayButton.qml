import QtQuick
import QtQuick.Controls

// Botão redondo grande de play/pause na cor de destaque.
Item {
    id: root

    property bool playing: false
    property real diameter: 56
    property color color: Theme.accent
    property color hoverColor: Theme.accentHover
    property color iconColor: Theme.textOnAccent
    property string tooltip: playing ? qsTr("Pausar") : qsTr("Tocar")

    signal clicked()

    implicitWidth: diameter
    implicitHeight: diameter

    Rectangle {
        id: circle
        anchors.centerIn: parent
        width: root.diameter
        height: root.diameter
        radius: width / 2
        color: Theme.themeId === "halloween" ? (mouse.containsMouse ? "#542116" : "#1b101d") : (mouse.containsMouse ? root.hoverColor : root.color)
        border.width: Theme.themeId === "halloween" ? 2 : 0
        border.color: "#f07732"
        scale: mouse.pressed ? 0.95 : (mouse.containsMouse ? 1.04 : 1)
        Behavior on scale { NumberAnimation { duration: 90 } }

        Rectangle {
            anchors.centerIn: parent; width: parent.width - 6; height: width; radius: width/2
            color: "transparent"; border.width: 2; border.color: "#ffb35a"; opacity: 0
            NumberAnimation on opacity { id: emberFlash; from: .9; to: 0; duration: 380; running: false }
        }
        Icon {
            anchors.centerIn: parent
            // o triângulo de play fica opticamente centralizado com 1px à direita
            anchors.horizontalCenterOffset: root.playing ? 0 : root.diameter * 0.02
            size: root.diameter * 0.42
            fillPath: root.playing ? Icons.pauseF : Icons.playF
            color: Theme.themeId === "halloween" ? "#ff9b4a" : root.iconColor
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: { if (Theme.themeId === "halloween") emberFlash.restart(); root.clicked() }
    }

    ToolTip.visible: tooltip !== "" && mouse.containsMouse
    ToolTip.text: tooltip
    ToolTip.delay: 700
}
