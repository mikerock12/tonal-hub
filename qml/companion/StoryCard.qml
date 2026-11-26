import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: card
    required property var controller
    property int days: 7
    property var engine: controller.stories || null
    property var info: engine ? engine.state : ({})
    property bool detailsVisible: false
    onDaysChanged: detailsVisible=false
    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + 40
    radius: 18; color: "#263748"
    ColumnLayout {
        id: content
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        Label {text: "Gêneros e clima do seu repertório"; font.pixelSize: 23; font.bold: true; color: "#f5f5f5"; Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {objectName:"readerSummary";text: card.info.summary || "Um resumo dos gêneros e das pistas de clima já registrados nas músicas do período, calculado por regras no seu aparelho."; textFormat:Text.PlainText; font.pixelSize:18;color:"#f5f5f5";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text: card.info.status || "Resumo local por regras, sem modelo.";font.pixelSize:14;color:"#b8c6d4";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Button {objectName:"readerAnalyzeButton";enabled:!card.info.busy;text:card.info.summary?"Atualizar resumo":"Resumir repertório";onClicked:card.controller.analyzeStories(card.days)}
        Button {visible:(card.info.items || []).length>0;text:card.detailsVisible?"Ocultar detalhes por música":"Ver detalhes por música ("+(card.info.items || []).length+")";onClicked:card.detailsVisible=!card.detailsVisible}
        ColumnLayout {visible:card.detailsVisible;Layout.fillWidth:true;spacing:14
            Repeater {model:card.info.items || []
                ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:5
                    Label {text:modelData.title+" — "+modelData.artist;textFormat:Text.PlainText;font.pixelSize:17;font.bold:true;color:"#f5f5f5";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    Label {text:modelData.theme;textFormat:Text.PlainText;font.pixelSize:15;font.bold:true;color:"#8ed7ef";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    Label {text:modelData.meaning;textFormat:Text.PlainText;font.pixelSize:15;color:"#d6e2ed";Layout.fillWidth:true;wrapMode:Text.Wrap}
                }
            }
        }
        Label {visible:!!card.info.sources;text:"Faixas consideradas: "+(card.info.sources || "");textFormat:Text.PlainText;font.pixelSize:13;color:"#b8c6d4";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text:"Resumo determinístico de até 8 faixas: conta os gêneros e escolhe as pistas de clima mais frequentes já presentes na biblioteca. Não interpreta o sentido das letras, não usa modelo de IA nem faz download. Metadados ausentes ficam identificados como não identificados; isto não é diagnóstico de humor ou saúde.";font.pixelSize:12;color:"#a5b5c6";Layout.fillWidth:true;wrapMode:Text.Wrap}
    }
}
