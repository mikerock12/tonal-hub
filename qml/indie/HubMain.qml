import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ApplicationWindow {
    id: window
    width: 1020; height: 620; minimumWidth: 760; minimumHeight: 500
    visible: true; title: "Tonal Hub"; color: "#10161c"
    palette.text: "#f2f6fa"; palette.windowText: "#f2f6fa"; palette.button: "#2d414f"; palette.buttonText: "#f2f6fa"; palette.base: "#253340"; palette.highlight: "#0caac9"
    palette.placeholderText: "#9aaebb"
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 24
        Label { text: "Tonal Hub"; font.pixelSize: 34; font.bold: true }
        Label { text: "Sua música. Outros caminhos para descobrir."; font.pixelSize: 22 }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 22
            Repeater {
                model: [{name:"Tonalify",subtitle:"Spotify",detail:"Sua biblioteca, busca e áudio atuais.",indie:false},{name:"IndieTonal",subtitle:"Fontes abertas e sua coleção",detail:"Discografias, shows, Audius e bibliotecas próprias.",indie:true}]
                Rectangle {
                    required property var modelData
                    Layout.fillWidth: true; Layout.fillHeight: true; color: modelData.indie ? "#173341" : "#19362c"; radius: 18
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 24; spacing: 16
                        Label { text: modelData.name; font.pixelSize: 30; font.bold: true }
                        Label { text: modelData.subtitle; font.pixelSize: 17; color: "#b4ced6" }
                        Label { text: modelData.detail; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        Button { text: "Abrir " + modelData.name; onClicked: modelData.indie ? hub.indie() : hub.spotify() }
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: query; Layout.fillWidth: true; placeholderText: "Banda, artista ou show para explorar no IndieTonal"; onAccepted: hub.indie(text) }
            Button { text: "Explorar"; onClicked: hub.indie(query.text) }
        }
        Label { text: "Cada player tem processo e conexões próprios. O hub pode continuar aberto."; color: "#a5b8c6" }
        Label { id: error; color: "#ffbf78"; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    }
    Connections { target: hub; function onError(message) { error.text=message } }
}
