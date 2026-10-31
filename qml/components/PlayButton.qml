import QtQuick
import QtQuick.Controls
import "qrc:/companion/qml/companion" as Personal

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
        color: Theme.themeId === "halloween" ? (mouse.containsMouse ? "#542116" : "#1b101d") : Theme.themeId === "natal" ? "#0c2118" : (mouse.containsMouse ? root.hoverColor : root.color)
        border.width: Theme.themeId === "halloween" || Theme.themeId === "natal" ? 2 : 0
        border.color: Theme.themeId === "natal" ? "#b88a35" : "#f07732"
        scale: mouse.pressed ? 0.95 : (mouse.containsMouse ? 1.04 : 1)
        Behavior on scale { NumberAnimation { duration: 90 } }

        Personal.EmberRing { objectName: "emberRing"; anchors.fill: parent; visible: Theme.themeId === "halloween"; animationAllowed: root.visible; z: 0 }
        Personal.ChristmasRing { objectName: "christmasRing"; anchors.fill: parent; visible: Theme.themeId === "natal"; animationAllowed: root.visible; z: 0 }

        Rectangle {
            anchors.centerIn: parent; width: parent.width - 6; height: width; radius: width/2
            color: "transparent"; border.width: 2; border.color: "#ffb35a"; opacity: 0
            NumberAnimation on opacity { id: emberFlash; from: .9; to: 0; duration: 380; running: false }
        }
        Icon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: root.playing ? 0 : root.diameter * 0.02
            size: root.diameter * 0.42
            fillPath: root.playing ? Icons.pauseF : Icons.playF
            color: Theme.themeId === "halloween" ? "#ff9b4a" : root.iconColor
            visible: Theme.themeId !== "halloween" && Theme.themeId !== "natal"
        }
        Personal.TransportGlyph {
            objectName: "transportGlyph"
            anchors.centerIn: parent
            width: root.diameter * .58; height: width
            playing: root.playing
            ink: Theme.themeId === "halloween" ? "#ff9b4a" : "#ffe39c"
            animationAllowed: root.visible
            visible: Theme.themeId === "halloween" || Theme.themeId === "natal"
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
