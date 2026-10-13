import QtQuick
import QtQuick.Controls

// Item dos menus escuros (menu de faixa, menu da playlist). Também desenha a seta dos submenus.
MenuItem {
    id: root

    implicitHeight: 40
    contentItem: Label {
        leftPadding: 4
        rightPadding: root.subMenu ? 20 : 0
        text: root.text
        color: root.enabled ? Theme.textPrimary : Theme.textMuted
        font.pixelSize: 14
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    arrow: Icon {
        x: root.width - width - 12
        y: (root.height - height) / 2
        visible: !!root.subMenu
        size: 14
        strokePath: Icons.chevronRightS
        color: Theme.textSecondary
    }
    background: Rectangle {
        implicitWidth: 240
        color: root.highlighted ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
        radius: 2
    }
}
