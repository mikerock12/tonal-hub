import QtQuick

// Arte das "Músicas Curtidas": degradê com coração.
Rectangle {
    id: root
    property real iconScale: 0.4
    radius: Theme.radiusCover
    gradient: Gradient {
        orientation: Gradient.Horizontal
        GradientStop { position: 0; color: Theme.accentDeep }
        GradientStop { position: 1; color: Theme.accentText }
    }
    Icon {
        anchors.centerIn: parent
        size: Math.min(parent.width, parent.height) * root.iconScale
        fillPath: Icons.heart
        color: "white"
    }
}
