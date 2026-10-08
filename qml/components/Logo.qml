import QtQuick

// Marca do Tonal: círculo na cor de destaque com barras de onda sonora.
Item {
    id: root
    property real size: 32
    implicitWidth: size
    implicitHeight: size

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Theme.accent

        Row {
            anchors.centerIn: parent
            spacing: root.size * 0.07
            Repeater {
                model: [0.32, 0.56, 0.42, 0.24]
                Rectangle {
                    required property var modelData
                    width: root.size * 0.09
                    height: root.size * modelData
                    radius: width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textOnAccent
                }
            }
        }
    }
}
