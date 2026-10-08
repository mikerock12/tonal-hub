import QtQuick
import QtQuick.Controls

// "Pílula" de filtro (Playlists, Álbuns, Artistas...).
Item {
    id: root

    property string text
    property bool checked: false
    property bool closable: false
    signal clicked()

    implicitHeight: 32
    implicitWidth: label.implicitWidth + 24 + (closable ? 20 : 0)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.textPrimary
                            : (mouse.containsMouse ? Theme.elevatedHover : Qt.rgba(1, 1, 1, 0.07))
    }

    Label {
        id: label
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: root.closable ? -8 : 0
        text: root.text
        color: root.checked ? "black" : Theme.textPrimary
        font.pixelSize: 14
    }

    Icon {
        visible: root.closable
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        size: 12
        strokePath: Icons.closeS
        color: root.checked ? "black" : Theme.textSecondary
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
