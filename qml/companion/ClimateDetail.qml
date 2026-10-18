import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: detail
    required property var controller
    property int days:7
    property var report:({})
    onOpened:{controller.refreshMetadata();report=controller.insights(days)}
    Connections{target:detail.controller;function onChanged(){if(detail.visible)detail.report=detail.controller.insights(detail.days)}}
    title:"Seu repertório · últimos "+days+" dias"
    parent:Overlay.overlay;anchors.centerIn:parent;modal:true
    width:Math.min(820,parent?parent.width-40:780);height:Math.min(820,parent?parent.height-40:760)
    background:Rectangle{color:"#14171c";radius:22;border.color:"#303946"}
    standardButtons:Dialog.Close
    ScrollView{anchors.fill:parent;clip:true
        ColumnLayout{width:detail.availableWidth-18;spacing:18
            Rectangle{Layout.fillWidth:true;implicitHeight:hero.implicitHeight+40;radius:18;color:"#263748"
                ColumnLayout{id:hero;anchors.fill:parent;anchors.margins:20;spacing:10
                    Label{text:"SUAS HISTÓRIAS EM MÚSICA";color:"#7ddbff";font.pixelSize:12}
                    Label{text:detail.report.vibe||"Seu repertório";textFormat:Text.PlainText;color:"#f5f5f5";font.pixelSize:30;font.bold:true}
                    Label{text:((detail.report.ms||0)/60000).toFixed(1)+" min · "+(detail.report.plays||0)+" escutas";font.pixelSize:19;color:"#f5f5f5"}
                    Label{text:(detail.report.coverage||0)+" músicas · "+(detail.report.artistCount||0)+" artistas · "+(detail.report.albumCount||0)+" álbuns";color:"#b8c6d4";font.pixelSize:15}
                }
            }
            Rectangle{Layout.fillWidth:true;implicitHeight:themes.implicitHeight+40;radius:18;color:"#303044"
                ColumnLayout{id:themes;anchors.fill:parent;anchors.margins:20;spacing:12
                    Label{text:"As histórias das suas músicas";font.pixelSize:23;font.bold:true;color:"#f5f5f5"}
                    Label{text:"Pistas nas letras, sem diagnosticar seu humor.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#b8c6d4";font.pixelSize:15}
                    Label{visible:!(detail.report.themes||[]).length;text:"Ainda não há um tema com evidência suficiente. A análise se enriquece conforme as letras são carregadas.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#f5f5f5";font.pixelSize:16}
                    Repeater{model:detail.report.themes||[]
                        ColumnLayout{required property var modelData;Layout.fillWidth:true
                            Label{text:modelData.label+" · "+modelData.percent.toFixed(0)+"%";textFormat:Text.PlainText;font.pixelSize:17;color:"#f5f5f5"}
                            ProgressBar{Layout.fillWidth:true;from:0;to:100;value:modelData.percent}
                        }
                    }
                    Label{text:"Letras: "+(detail.report.lyricCoverage||0)+"/"+(detail.report.coverage||0)+" · cobertura de temas: "+(detail.report.modelCoverage||0)+"/"+(detail.report.coverage||0);color:"#b8c6d4";font.pixelSize:13;Layout.fillWidth:true;wrapMode:Text.Wrap}
                }
            }
            Repeater{model:[{key:"artists",label:"Sua companhia musical"},{key:"albums",label:"Álbuns que marcaram"},{key:"genres",label:"Estilos do repertório"}]
                Rectangle{id:section;required property var modelData;Layout.fillWidth:true;implicitHeight:sectionRows.implicitHeight+40;radius:18;color:"#20252d"
                    ColumnLayout{id:sectionRows;anchors.fill:parent;anchors.margins:20;spacing:14
                        Label{text:section.modelData.label;font.pixelSize:23;font.bold:true;color:"#f5f5f5"}
                        Label{visible:!(detail.report[section.modelData.key]||[]).length;text:section.modelData.key==="genres"?"Consultando estilos nas fontes musicais. Identificações ambíguas permanecem pendentes.":"Os detalhes aparecem quando a fonte identifica a gravação.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#b8c6d4";font.pixelSize:15}
                        Repeater{model:(detail.report[section.modelData.key]||[]).slice(0,6)
                            Item{required property var modelData;Layout.fillWidth:true;implicitHeight:Math.max(58,labels.implicitHeight+12)
                                RowLayout{anchors.fill:parent;spacing:14
                                    Rectangle{visible:section.modelData.key!=="genres";Layout.preferredWidth:54;Layout.preferredHeight:54;radius:9;color:"#304452";clip:true
                                        Image{anchors.fill:parent;source:(modelData.tracks||[]).length?modelData.tracks[0].image||"":"";sourceSize.width:108;sourceSize.height:108;fillMode:Image.PreserveAspectCrop;asynchronous:true}
                                    }
                                    ColumnLayout{id:labels;Layout.fillWidth:true;spacing:5
                                        Label{text:modelData.label;textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:18;font.bold:true;color:"#f5f5f5"}
                                        Label{text:(modelData.ms/60000).toFixed(1)+" min · "+modelData.plays+" escutas · "+modelData.trackCount+" músicas";Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:14;color:"#b8c6d4"}
                                    }
                                    ActionIcon{symbol:"next";description:"Ver detalhes";onClicked:{drill.report=modelData;drill.open()}}
                                }
                                TapHandler{onTapped:{drill.report=modelData;drill.open()}}
                            }
                        }
                    }
                }
            }
            Label{text:(detail.report.model||"")+". Temas podem se sobrepor; metáforas e ironia podem causar erros. Os estilos vêm das fontes identificadas. Código, dados e pesos MIT.";textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:13;color:"#a5b5c6"}
        }
    }
    StatisticsDetail{id:drill;controller:detail.controller}
}
