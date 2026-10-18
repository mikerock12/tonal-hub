import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: dialog
    required property var controller
    parent: Overlay.overlay;anchors.centerIn:parent
    title:"Personalizar letras";modal:true
    width:Math.min(600,parent?parent.width-32:560)
    height:Math.min(710,parent?parent.height-32:680)
    standardButtons:Dialog.Close
    ScrollView{anchors.fill:parent;clip:true
        ColumnLayout{width:dialog.availableWidth-18;spacing:20
            Repeater{model:[{prefix:"lyric",label:"Letra original"},{prefix:"translation",label:"Tradução"}]
                ColumnLayout{
                    id:section
                    required property var modelData
                    readonly property string prefix:modelData.prefix
                    Layout.fillWidth:true;spacing:10
                    Label{text:section.modelData.label;font.pixelSize:22;font.bold:true}
                    RowLayout{
                        ComboBox{model:["Sans","Serif","Mono","Rounded"];currentIndex:Math.max(0,model.indexOf(dialog.controller.lyricStyle[section.prefix+"Font"]));onActivated:dialog.controller.setLyricStyle(section.prefix+"Font",currentText)}
                        SpinBox{from:16;to:44;value:dialog.controller.lyricStyle[section.prefix+"Size"];onValueModified:dialog.controller.setLyricStyle(section.prefix+"Size",value)}
                        ComboBox{model:["Normal","Contorno","Brilho","Neon","Piscante"];currentIndex:Math.max(0,model.indexOf(dialog.controller.lyricStyle[section.prefix+"Effect"]));onActivated:dialog.controller.setLyricStyle(section.prefix+"Effect",currentText)}
                    }
                    RowLayout{Repeater{model:[{key:"Bold",label:"Negrito"},{key:"Italic",label:"Itálico"},{key:"Underline",label:"Sublinhar"}];CheckBox{required property var modelData;text:modelData.label;checked:!!dialog.controller.lyricStyle[section.prefix+modelData.key];onToggled:dialog.controller.setLyricStyle(section.prefix+modelData.key,checked)}}}
                    Repeater{model:[{suffix:"Color",label:"Cor do texto"},{suffix:"OutlineColor",label:"Contorno / brilho"}]
                        ColumnLayout{
                            id:colorRow;required property var modelData
                            readonly property string targetKey:section.prefix+modelData.suffix
                            Layout.fillWidth:true
                            Label{text:colorRow.modelData.label}
                            RowLayout{Repeater{model:["#F5F5F5","#CEE9F0","#7DDBFF","#FFE39C","#FFBBD9","#AEE8B7","#101722"]
                                Button{required property string modelData;implicitWidth:40;implicitHeight:40;Accessible.name:"Cor "+modelData;background:Rectangle{radius:20;color:parent.modelData;border.color:"#808080";border.width:1}onClicked:{hex.text=modelData;dialog.controller.setLyricStyle(colorRow.targetKey,modelData)}}
                            }}
                            RowLayout{
                                TextField{id:hex;text:dialog.controller.lyricStyle[colorRow.targetKey] || "#101722";maximumLength:7;placeholderText:"#RRGGBB";onEditingFinished:if(/^#[0-9a-fA-F]{6}$/.test(text))dialog.controller.setLyricStyle(colorRow.targetKey,text)}
                                Button{text:"Escolher cor…";onClicked:{colors.targetField=hex;colors.targetKey=colorRow.targetKey;colors.selectedColor=dialog.controller.lyricStyle[colors.targetKey];colors.open()}}
                            }
                        }
                    }
                }
            }
        }
    }
    ColorDialog{id:colors;property string targetKey;property var targetField:null;onAccepted:{const value=selectedColor.toString().slice(0,7);if(targetField)targetField.text=value;dialog.controller.setLyricStyle(targetKey,value)}}
}
