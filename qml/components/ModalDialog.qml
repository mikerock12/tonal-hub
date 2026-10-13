import QtQuick
import QtQuick.Controls

// Janela modal do app: título, conteúdo (os filhos declarados) e os botões à direita, embaixo.
Popup {
    id: root

    property string title
    default property alias content: body.data
    property alias buttons: buttonRow.data

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(480, parent ? parent.width - 48 : 480)
    modal: true
    focus: true
    padding: 24
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.6) }
    background: Rectangle {
        radius: 8
        color: Theme.elevated
        border.color: Theme.divider
    }

    contentItem: Column {
        spacing: 20

        Label {
            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: 22
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }
        Column {
            id: body
            width: parent.width
            spacing: 12
        }
        Row {
            id: buttonRow
            anchors.right: parent.right
            spacing: 12
        }
    }
}
