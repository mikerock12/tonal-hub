import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: dialog
    required property var controller
    parent: Overlay.overlay
    anchors.centerIn: parent
    title: "Fontes e cores das letras"
    modal: true
    width: Math.min(560,parent ? parent.width-32 : 520)
    standardButtons: Dialog.Close
    ColumnLayout { width: parent.width; spacing: 14
        Repeater { model: [{prefix:"lyric",label:"Letra original"},{prefix:"translation",label:"Tradução"}]
            ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { text:modelData.label; font.pixelSize:20; font.bold:true }
                RowLayout {
                    ComboBox { model:["Sans","Serif","Mono","Rounded"]; currentIndex:Math.max(0,model.indexOf(dialog.controller.lyricStyle[modelData.prefix+"Font"])); onActivated:dialog.controller.setLyricStyle(modelData.prefix+"Font",currentText); Accessible.name:"Fonte da "+modelData.label }
                    SpinBox { from:16;to:44;value:dialog.controller.lyricStyle[modelData.prefix+"Size"];onValueModified:dialog.controller.setLyricStyle(modelData.prefix+"Size",value);Accessible.name:"Tamanho da "+modelData.label }
                    Button { text:"Cor…"; onClicked:{colors.targetKey=modelData.prefix+"Color";colors.selectedColor=dialog.controller.lyricStyle[colors.targetKey];colors.open()} }
                }
            }
        }
        Label { text:"Letra e tradução têm preferências independentes, salvas no backup pessoal.";wrapMode:Text.Wrap;Layout.fillWidth:true }
    }
    ColorDialog { id:colors;property string targetKey;onAccepted:dialog.controller.setLyricStyle(targetKey,selectedColor.toString().slice(0,7)) }
}
