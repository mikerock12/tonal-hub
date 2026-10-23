import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: share
    property var track: ({})
    readonly property string link: /^spotify:track:[A-Za-z0-9]{22}$/.test(track.uri||track.id||"")?"https://open.spotify.com/track/"+(track.uri||track.id).slice(14):""
    property color cardColor: "#9299B0"
    property int style: 0
    property bool includeCover: true
    property string message: ""
    function present(current){track=Object.assign({},current);message="";open()}
    title: "Compartilhar música"
    modal:true; anchors.centerIn:parent; width:Math.min(650,parent.width-32);height:Math.min(820,parent.height-32)
    palette.window:"#141517";palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";palette.base:"#20252d";palette.button:"#263440";palette.buttonText:"#f5f5f5"
    background:Rectangle{color:"#141517";radius:18}
    standardButtons:Dialog.Close
    ColumnLayout {
        objectName:"shareBody"
        anchors.fill:parent
        Item {Layout.fillWidth:true;Layout.fillHeight:true
            Item {id:card;objectName:"sharePreview";anchors.centerIn:parent;width:Math.min(parent.width,parent.height*600/1066);height:width*1066/600
                // Fixed logical coordinates keep preview and export identical.
                Item {width:600;height:1066;scale:card.width/600;transformOrigin:Item.TopLeft
                    Rectangle {anchors.fill:parent;radius:30;color:share.style===2?"#FAF5E9":share.cardColor
                        gradient:share.style===1?fade:null
                        Gradient{id:fade;GradientStop{position:0;color:share.cardColor}GradientStop{position:1;color:"#08090C"}}
                        Column{visible:share.style===2;anchors.fill:parent;spacing:36;Repeater{model:28;Rectangle{width:600;height:2;color:"#D4E5E9"}}}
                    }
                    Rectangle {x:45;y:125;width:510;height:810;radius:22;color:share.style===2?"#FFFBef":"#161A22"
                        Rectangle{x:25;y:30;width:460;height:460;color:share.cardColor;radius:12
                            Rectangle{anchors.centerIn:parent;width:220;height:220;radius:110;color:"#263748";border.color:"white";Rectangle{anchors.centerIn:parent;width:50;height:50;radius:25;color:share.cardColor}}
                            Image{id:cover;anchors.fill:parent;visible:share.includeCover;source:share.track.image||"";fillMode:Image.PreserveAspectFit;asynchronous:true;sourceSize.width:600;sourceSize.height:600}
                        }
                        Label{x:25;y:530;width:460;text:share.track.title||"";textFormat:Text.PlainText;color:share.style===2?"#111111":"white";font.pixelSize:31;font.bold:true;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight}
                        Label{x:25;y:645;width:460;text:share.track.artist||"";textFormat:Text.PlainText;color:share.style===2?"#111111":"#D2D7DF";font.pixelSize:24;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight}
                        Label{x:25;y:730;width:460;text:"Tonalify · Ouvir no Spotify";color:share.style===2?"#111111":"white";font.pixelSize:20}
                    }
                }
            }
        }
        RowLayout {Layout.alignment:Qt.AlignHCenter
            Repeater{model:["#9299B0","#009BD6","#7A538E","#BC634B","#101112"];delegate:AbstractButton{required property string modelData;implicitWidth:44;implicitHeight:44;Accessible.name:"Cor "+modelData;onClicked:{share.cardColor=modelData;colorInput.text=modelData}background:Rectangle{anchors.centerIn:parent;width:30;height:30;radius:15;color:parent.modelData;border.color:"white";border.width:share.cardColor===parent.modelData?3:0}}}
            TextField{id:colorInput;text:"#9299B0";placeholderText:"#RRGGBB";maximumLength:7;Layout.preferredWidth:110;onTextChanged:if(/^#[A-Fa-f0-9]{6}$/.test(text))share.cardColor=text}
        }
        RowLayout{Layout.alignment:Qt.AlignHCenter;ComboBox{currentIndex:share.style;model:["Cor sólida","Degradê","Papel / polaroid"];onActivated:share.style=currentIndex}CheckBox{text:"Incluir capa";checked:true;onToggled:share.includeCover=checked}}
        Label{text:share.link||"Esta gravação não forneceu um link Spotify de faixa.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#A5A8AD"}
        Label{text:share.message;visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap}
        RowLayout{Layout.alignment:Qt.AlignHCenter
            Button{text:"Copiar link";enabled:share.link.length>0;onClicked:{copyField.text=share.link;copyField.selectAll();copyField.copy();share.message="Link copiado"}}
            Button{text:"Salvar cartão PNG…";enabled:share.link.length>0&&cover.status!==Image.Loading;onClicked:save.open()}
        }
        TextEdit{id:copyField;visible:false}
    }
    FileDialog{id:save;title:"Salvar cartão da música";fileMode:FileDialog.SaveFile;defaultSuffix:"png";nameFilters:["Imagem PNG (*.png)"];onAccepted:{let address=selectedFile.toString();if(address.startsWith("file:///")){let path=decodeURIComponent(address.slice(Qt.platform.os==="windows"?8:7));card.grabToImage(function(result){share.message=result.saveToFile(path)?"Cartão salvo. Envie a imagem e o link copiado pelo aplicativo que preferir.":"Não foi possível salvar o cartão."},Qt.size(600,1066))}}}
}
