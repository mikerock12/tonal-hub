import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: detail
    required property var controller
    property var report: ({})
    title: report.label || "Escutas"
    parent: Overlay.overlay;anchors.centerIn:parent;modal:true
    width:Math.min(760,parent ? parent.width-40 : 720);height:Math.min(680,parent ? parent.height-40 : 640)
    standardButtons:Dialog.Close
    ScrollView {anchors.fill:parent;clip:true
        ColumnLayout {width:detail.availableWidth-18;spacing:14
            Label{text:((detail.report.ms || 0)/60000).toFixed(1)+" minutos";font.pixelSize:30;font.bold:true}
            Label{text:(detail.report.plays || 0)+" escutas • "+(detail.report.trackCount || 0)+" músicas • "+(detail.report.albumCount || 0)+" álbuns"}
            Label{text:"Tempo por álbum";font.pixelSize:21;font.bold:true}
            Repeater{model:detail.report.albums || [];Label{required property var modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;textFormat:Text.PlainText;text:modelData.album+" • "+(modelData.ms/60000).toFixed(1)+" min • "+modelData.plays+" escutas • "+modelData.trackCount+" músicas"}}
            Label{text:"Músicas ouvidas";font.pixelSize:21;font.bold:true}
            Repeater{model:detail.report.tracks || [];Rectangle{
                required property var modelData;Layout.fillWidth:true;implicitHeight:92;radius:10;color:"#24313b"
                RowLayout{anchors.fill:parent;anchors.margins:12
                    ColumnLayout{Layout.fillWidth:true
                        Label{text:modelData.title;textFormat:Text.PlainText;font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}
                        Label{text:(modelData.artist || "")+" • "+(modelData.album || "");textFormat:Text.PlainText;Layout.fillWidth:true;elide:Text.ElideRight}
                        Label{text:(modelData.listenedPlays || 0)+" escutas • "+((modelData.listenedMs || 0)/60000).toFixed(1)+" min";color:"#a9d8e8"}
                    }
                    ActionIcon{symbol:"play";description:"Reproduzir "+modelData.title;onClicked:detail.controller.play([modelData],0)}
                }
            }}
            Label{Layout.fillWidth:true;wrapMode:Text.Wrap;text:"Tempo realmente ouvido, sem pausas ou saltos. Uma escuta qualificada exige 30 s ou metade da faixa, o que for menor.";font.pixelSize:12}
        }
    }
}
