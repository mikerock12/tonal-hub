import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: about
    anchors.centerIn: parent
    width: Math.min(520, parent.width - 40)
    title: "Sobre o Tonal Hub"
    modal: true
    readonly property color surface: "#17232e"
    readonly property color foreground: "#f4f6f8"
    palette.window: surface
    palette.windowText: foreground
    palette.text: foreground
    background: Rectangle { color: about.surface; border.color: "#42586a"; radius: 10 }
    header: Label { text: about.title; color: about.foreground; font.pixelSize: 20; font.bold: true; padding: 16 }
    contentItem: ColumnLayout {
        Label { text: "Versão " + Qt.application.version; color: about.foreground }
        Label {
            Layout.fillWidth: true
            color: about.foreground
            text: "Projeto original de Matheus Nunes (natheus). Tonal Hub mantido por Maicon Nunes (mikerock12). Licença MIT."
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            color: about.foreground
            text: desktopUpdate.status
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            color: about.foreground
            visible: !!desktopUpdate.releaseNotes
            text: desktopUpdate.releaseNotes
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button { text: "Verificar atualização"; enabled: !desktopUpdate.busy; onClicked: desktopUpdate.check() }
            Button {
                visible: !!desktopUpdate.availableVersion
                enabled: !desktopUpdate.busy
                text: desktopUpdate.installed ? "Atualizar e reiniciar" : "Abrir portal"
                onClicked: {
                    if (!desktopUpdate.installed) Qt.openUrlExternally("https://softwares.smellsliketech.com.br/apps/tonal/")
                    else if (desktopUpdate.ready) desktopUpdate.install()
                    else desktopUpdate.download()
                }
            }
            Button { text: "Fechar"; onClicked: about.close() }
        }
    }
    Popup {
        id: notice
        parent: Overlay.overlay
        x: parent.width - width - 18
        y: 18
        width: Math.min(390, parent.width - 36)
        modal: false
        background: Rectangle { color: about.surface; border.color: "#42586a"; radius: 10 }
        contentItem: ColumnLayout {
            Label {
                Layout.fillWidth: true
                color: about.foreground
                text: "Versão " + desktopUpdate.availableVersion + " disponível\n" + desktopUpdate.releaseNotes
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                maximumLineCount: 6
                elide: Text.ElideRight
            }
            RowLayout {
                Button { text: "Ver atualização"; onClicked: { notice.close(); about.open() } }
                Button { text: "Agora não"; onClicked: notice.close() }
            }
        }
    }
    Connections {
        target: desktopUpdate
        function onUpdateAvailable() { if (!about.opened) notice.open() }
    }
}
