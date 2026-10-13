import QtQuick
import QtQuick.Controls

// Menu escuro do app. Os submenus e itens criados pelo próprio Menu também usam o DarkMenuItem.
Menu {
    id: root

    delegate: DarkMenuItem { }
    background: Rectangle {
        implicitWidth: 240
        color: "#282828"
        radius: 4
        border.color: Qt.rgba(1, 1, 1, 0.06)
    }
}
