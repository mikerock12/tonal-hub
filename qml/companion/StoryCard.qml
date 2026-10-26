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
        Label {text: "As histórias das suas músicas"; font.pixelSize: 23; font.bold: true; color: "#f5f5f5"; Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text: card.info.summary || "Uma leitura dos temas e contrastes nas letras do período, feita no seu aparelho."; textFormat:Text.PlainText; font.pixelSize:18;color:"#f5f5f5";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text: card.info.status || "Análise local indisponível nesta versão.";font.pixelSize:14;color:"#b8c6d4";Layout.fillWidth:true;wrapMode:Text.Wrap}
        ProgressBar {visible:!!card.info.downloading;Layout.fillWidth:true;value:card.info.progress || 0}
        Flow {Layout.fillWidth:true;spacing:8
            Button {visible:!card.info.ready;enabled:!!card.engine&&!card.info.downloading;text:"Baixar modelo · 1,19 GiB";onClicked:card.engine.download()}
            Button {visible:!!card.info.ready;enabled:!card.info.busy;text:card.info.partial?"Continuar análise":card.info.summary?"Atualizar análise":"Analisar letras";onClicked:card.controller.analyzeStories(card.days)}
            Button {visible:!!card.info.busy||!!card.info.downloading;text:"Cancelar";onClicked:card.engine.cancel()}
            Button {visible:(!!card.info.ready||(card.info.legacyBytes || 0)>0)&&!card.info.busy;text:"Remover modelo";onClicked:card.engine.removeModel()}
        }
        Button {visible:(card.info.items || []).length>0;text:card.detailsVisible?"Ocultar leitura por música":"Ver leitura por música ("+(card.info.items || []).length+")";onClicked:card.detailsVisible=!card.detailsVisible}
        ColumnLayout {visible:card.detailsVisible;Layout.fillWidth:true;spacing:14
            Repeater {model:card.info.items || []
                ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:5
                    Label {text:modelData.title+" — "+modelData.artist;textFormat:Text.PlainText;font.pixelSize:17;font.bold:true;color:"#f5f5f5";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    Label {text:modelData.theme;textFormat:Text.PlainText;font.pixelSize:15;font.bold:true;color:"#8ed7ef";Layout.fillWidth:true;wrapMode:Text.Wrap}
                    Label {text:modelData.meaning;textFormat:Text.PlainText;font.pixelSize:15;color:"#d6e2ed";Layout.fillWidth:true;wrapMode:Text.Wrap}
                }
            }
        }
        Label {visible:!!card.info.sources;text:"Lote selecionado: "+(card.info.sources || "");textFormat:Text.PlainText;font.pixelSize:13;color:"#b8c6d4";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text:"Qwen3.5 2B (Apache 2.0) com llama.cpp (MIT). Até 8 letras por lote, com leitura por música; trechos podem omitir contexto. Interpretação experimental: pode errar metáforas, ironia e sentidos; não descreve sua saúde ou humor. O modelo só ocupa memória durante a análise e fica fora do backup da biblioteca.";font.pixelSize:12;color:"#a5b5c6";Layout.fillWidth:true;wrapMode:Text.Wrap}
        Label {text:"Modelo no disco: "+((card.info.modelBytes || 0)/1048576).toFixed(1)+" MiB · Cache de análise: "+((card.info.cacheBytes || 0)/1024).toFixed(1)+" KiB";font.pixelSize:11;color:"#a5b5c6";Layout.fillWidth:true;wrapMode:Text.Wrap}
    }
}
