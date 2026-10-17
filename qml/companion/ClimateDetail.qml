import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: detail
    required property var controller
    property int days:7
    property var report:({})
    onOpened:report=controller.insights(days)
    title:"Clima do repertório • últimos "+days+" dias"
    parent:Overlay.overlay;anchors.centerIn:parent;modal:true
    width:Math.min(760,parent ? parent.width-40 : 720);height:Math.min(720,parent ? parent.height-40 : 680)
    standardButtons:Dialog.Close
    ScrollView{anchors.fill:parent;clip:true
        ColumnLayout{width:detail.availableWidth-18;spacing:16
            Label{text:detail.report.vibe || "Seu repertório";font.pixelSize:28;font.bold:true}
            Label{text:detail.report.explanation || "";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label{text:"Temas das letras";font.pixelSize:22;font.bold:true}
            Label{visible:!(detail.report.themes || []).length;text:"Sem evidência suficiente para destacar um tema. Abra letras para analisar seu repertório.";wrapMode:Text.Wrap;Layout.fillWidth:true}
            Repeater{model:detail.report.themes || [];Label{required property var modelData;text:modelData.label+" • "+modelData.percent.toFixed(0)+"% do tempo analisado";font.pixelSize:18}}
            Label{text:"Cobertura: "+(detail.report.modelCoverage || 0)+"/"+(detail.report.coverage || 0)+" músicas. Os temas podem se sobrepor.";Layout.fillWidth:true;wrapMode:Text.Wrap}
            Repeater{model:[{key:"artists",label:"Artistas em destaque"},{key:"albums",label:"Álbuns em destaque"},{key:"genres",label:"Estilos informados"}]
                ColumnLayout{required property var modelData;Layout.fillWidth:true
                    Label{text:modelData.label;font.pixelSize:21;font.bold:true}
                    Repeater{model:(detail.report[modelData.key] || []).slice(0,6);Button{required property var modelData;Layout.fillWidth:true;text:modelData.label+" • "+(modelData.ms/60000).toFixed(1)+" min";onClicked:{drill.report=modelData;drill.open()}}}
                }
            }
            Label{text:detail.report.model || "";Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12}
            Label{text:"Modelo experimental treinado em situações sintéticas PT/EN/ES. Reconhece pistas de temas; pode errar em metáforas, ironia e outros idiomas. Resume o repertório, sem avaliar seu estado emocional. Sugestões de artistas vêm de relações editoriais por gênero. Código, dados e pesos MIT no projeto.";Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:13}
        }
    }
    StatisticsDetail{id:drill;controller:detail.controller}
}
