import QtQuick
import QtQuick.Controls

// Escolha do tema visual. Cada cartão mostra as cores do tema e a barra de
// progresso com o marcador dele; o clique aplica na hora e fica salvo.
ModalDialog {
    id: root
    title: qsTr("Temas")
    width: Math.min(620, parent ? parent.width - 48 : 620)

    Label {
        width: parent.width
        text: qsTr("Mudam cores, barra de progresso e o marcador dela. Valem no PC e no celular com os mesmos nomes.")
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
    }

    Grid {
        id: grid
        objectName: "themeGrid"
        width: parent.width
        columns: width > 520 ? 2 : 1
        spacing: 12

        Repeater {
            model: Theme.themes
            delegate: Rectangle {
                id: card
                required property var modelData
                readonly property bool chosen: modelData.id === Theme.themeId
                objectName: "theme_" + modelData.id
                width: (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns
                height: 118
                radius: 10
                color: modelData.panel
                border.width: chosen ? 2 : 1
                border.color: chosen ? modelData.accent : Qt.rgba(1, 1, 1, cardMouse.containsMouse ? 0.3 : 0.12)

                Column {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Row {
                        spacing: 6
                        Repeater {
                            model: [card.modelData.bg, card.modelData.card, card.modelData.accent, card.modelData.accentText]
                            Rectangle {
                                required property var modelData
                                width: 16; height: 16; radius: 8
                                color: modelData
                                border.color: Qt.rgba(1, 1, 1, 0.25)
                            }
                        }
                        Label {
                            text: card.chosen ? "  " + qsTr("Em uso") : ""
                            color: card.modelData.accentText
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }
                    }
                    Label {
                        text: card.modelData.name
                        color: card.modelData.text
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }
                    Label {
                        width: parent.width
                        text: card.modelData.description || ""
                        color: card.modelData.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    SeekBar {
                        width: parent.width
                        value: 0.62
                        showThumb: true
                        styleOverride: card.modelData.seek
                    }
                }
                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Theme.setTheme(card.modelData.id)
                }
            }
        }
    }

    buttons: [
        PillButton { text: qsTr("Fechar"); onClicked: root.close() }
    ]
}
