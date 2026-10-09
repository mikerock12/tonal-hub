import QtQuick
import QtQuick.Controls

// Seletor de dispositivo (Spotify Connect) + estado do motor de áudio local.
Popup {
    id: root

    width: 320
    padding: 16
    margins: 8
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: "#282828"
        radius: 8
        border.color: Qt.rgba(1, 1, 1, 0.06)
    }

    contentItem: Column {
        spacing: 12

        Label {
            text: qsTr("Conectar a um dispositivo")
            color: Theme.textPrimary
            font.pixelSize: 18
            font.weight: Font.Bold
        }

        // Motor local com problema
        Rectangle {
            visible: App.librespot.state !== Librespot.Ready
            width: parent.width
            height: engineCol.implicitHeight + 24
            radius: 6
            color: Qt.rgba(1, 1, 1, 0.06)
            Column {
                id: engineCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    text: App.librespot.statusText || qsTr("O motor de áudio local está desligado.")
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
                Row {
                    spacing: 8
                    Chip { text: qsTr("Reiniciar"); onClicked: App.librespot.restart() }
                    Chip { text: qsTr("Refazer login"); onClicked: App.librespot.resetLogin() }
                }
            }
        }

        Repeater {
            model: App.player.devices
            delegate: Rectangle {
                id: dev
                required property var modelData
                width: root.availableWidth
                height: 56
                radius: 6
                color: devMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 26
                        strokePath: Icons.devicesS
                        strokeWidth: 1.6
                        color: dev.modelData.active ? Theme.accent : Theme.textPrimary
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Label {
                            text: dev.modelData.local ? qsTr("Este computador (Tonal)") : dev.modelData.name
                            color: dev.modelData.active ? Theme.accentText : Theme.textPrimary
                            font.pixelSize: 15
                            font.weight: dev.modelData.active ? Font.DemiBold : Font.Normal
                        }
                        Label {
                            text: dev.modelData.active ? qsTr("Tocando agora") : (dev.modelData.local ? dev.modelData.name : dev.modelData.type)
                            color: Theme.textSecondary
                            font.pixelSize: 12
                        }
                    }
                }

                MouseArea {
                    id: devMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (!dev.modelData.active)
                            App.player.transferTo(dev.modelData.id)
                        root.close()
                    }
                }
            }
        }

        Label {
            visible: App.player.devices.length === 0
            width: root.availableWidth
            wrapMode: Text.WordWrap
            text: qsTr("Nenhum dispositivo encontrado. Abra o Spotify em outro aparelho ou aguarde o motor de áudio iniciar.")
            color: Theme.textSecondary
            font.pixelSize: 13
        }
    }
}
