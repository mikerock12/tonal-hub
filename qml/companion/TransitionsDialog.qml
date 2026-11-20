import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Transições entre as músicas da fila: liga/desliga, efeito fixo ou
// aleatório e tempo de mesclagem (3 a 10 s). As escolhas valem para toda
// lista de reprodução tocada pelo Tonalify neste computador.
Dialog {
    id: dialog
    objectName: "transitionsDialog"
    required property var player
    property color accent: "#009bd6"
    property color surface: "#1c1f24"
    property color ink: "#f5f5f5"
    property color muted: "#a5a8ad"
    parent: Overlay.overlay
    modal: true
    title: qsTr("Transições entre músicas")
    width: Math.min(560, parent ? parent.width - 40 : 540)
    height: Math.min(700, parent ? parent.height - 60 : 660)
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    palette.window: surface
    palette.windowText: ink
    palette.text: ink
    palette.buttonText: ink
    palette.base: Qt.darker(surface, 1.5)
    palette.button: Qt.lighter(surface, 1.6)
    palette.mid: muted
    palette.dark: accent
    palette.highlight: accent
    background: Rectangle { color: dialog.surface; radius: 18; border.color: "#33ffffff" }
    header: Label { text: dialog.title; color: dialog.ink; font.pixelSize: 22; font.bold: true; padding: 18 }

    contentItem: ScrollView {
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: dialog.availableWidth
            spacing: 12

            Switch {
                objectName: "transitionsEnabled"
                text: qsTr("Transições ligadas")
                checked: dialog.player.transitionsEnabled
                onToggled: dialog.player.transitionsEnabled = checked
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: dialog.muted
                text: qsTr("Mescla o fim de cada música com o começo da próxima, como um DJ.")
            }
            Label {
                Layout.fillWidth: true
                visible: !dialog.player.transitionsAvailable
                wrapMode: Text.Wrap
                color: "#ffb4a8"
                text: qsTr("As transições usam o processamento de áudio do Tonal. Ative-o em Áudio para ouvir os efeitos.")
            }
            Label { text: qsTr("Efeito"); font.bold: true; color: dialog.ink }
            ButtonGroup { id: effects }
            Repeater {
                model: dialog.player.transitionEffects
                delegate: RadioButton {
                    required property var modelData
                    objectName: "transition_" + modelData.id
                    Layout.fillWidth: true
                    text: modelData.name
                    ButtonGroup.group: effects
                    enabled: dialog.player.transitionsEnabled
                    checked: dialog.player.transitionEffect === modelData.id
                    onClicked: dialog.player.transitionEffect = modelData.id
                    contentItem: Column {
                        id: entry
                        readonly property var control: entry.parent
                        leftPadding: control && control.indicator ? control.indicator.width + control.spacing : 28
                        spacing: 2
                        opacity: enabled ? 1 : 0.55
                        Label { text: modelData.name; color: dialog.ink; font.pixelSize: 15; font.bold: modelData.id === "random" }
                        Label { width: dialog.availableWidth - 70; text: modelData.description; color: dialog.muted; font.pixelSize: 12; wrapMode: Text.Wrap }
                    }
                }
            }
            Label { text: qsTr("Tempo de mesclagem: %1 s").arg(seconds.value); font.bold: true; color: dialog.ink }
            Slider {
                id: seconds
                objectName: "transitionSeconds"
                Layout.fillWidth: true
                from: 3; to: 10; stepSize: 1; snapMode: Slider.SnapAlways
                enabled: dialog.player.transitionsEnabled
                value: dialog.player.transitionSeconds
                onMoved: dialog.player.transitionSeconds = Math.round(value)
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: dialog.muted
                font.pixelSize: 12
                text: qsTr("A mesclagem começa alguns segundos antes do fim da música e a próxima entra por baixo. Pular, escolher outra música ou buscar um trecho troca na hora, sem efeito. Não vale para a última música da fila, para a repetição de uma música só nem para arquivos locais.")
            }
            Label {
                Layout.fillWidth: true
                visible: dialog.player.transitionStatus.length > 0
                color: dialog.accent
                text: dialog.player.transitionStatus
            }
        }
    }
}
