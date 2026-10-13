import QtQuick
import QtQuick.Controls

// Botão em pílula dos diálogos: o principal no azul de destaque, o secundário só com contorno.
Item {
    id: root

    property string text
    property bool primary: true
    signal clicked()

    implicitHeight: 40
    implicitWidth: label.implicitWidth + 40
    opacity: enabled ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        scale: mouse.pressed ? 0.97 : 1
        color: !root.primary ? "transparent" : mouse.containsMouse ? Theme.accentHover : Theme.accent
        border.width: root.primary ? 0 : 1
        border.color: mouse.containsMouse ? Theme.textPrimary : Theme.textMuted
    }

    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.primary ? Theme.textOnAccent : Theme.textPrimary
        font.pixelSize: 14
        font.weight: Font.Bold
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
