import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    required property var controller
    property int step: 0
    signal authorizationRequested()
    signal localPlaylistRequested(string playlistId)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(960,parent ? parent.width-32 : 900)
    height: Math.min(790,parent ? parent.height-32 : 740)
    modal: true
    title: "Text to Tonalify"
    closePolicy: controller.syncing ? Popup.NoAutoClose : Popup.CloseOnEscape
    font.family:"Segoe UI";font.pixelSize:14
    palette.window: "#151719";palette.windowText:"#f5f5f5";palette.text: "#f5f5f5";palette.button:"#263440";palette.buttonText: "#f5f5f5";palette.base: "#22262a";palette.highlight:"#009bd6";palette.highlightedText:"#ffffff"
    background: Rectangle {color:"#151719";radius:18;border.color:"#343a42"}
    onClosed: if(!controller.syncing)controller.cancel()
    function resume(id) {if(controller.loadSaved(id)){step=3;open()}}
    function fresh() {step=0;open()}
    ColumnLayout {
        anchors.fill: parent; spacing:14
        Label { text: step===0?"Sua próxima playlist começa com um texto":step===1?"Escolha as gravações certas":step===2?"Dê sua identidade à playlist":"Pronta para ouvir";color:"#ffffff";font.pixelSize:25;font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap }
        Label {text:"1 · Colar texto     2 · Revisar músicas     3 · Personalizar     4 · Salvar";color:"#86cce7";font.pixelSize:13}
        Rectangle {Layout.fillWidth:true;implicitHeight:notice.implicitHeight+22;color:"#202c35";radius:10
            Label {id:notice;anchors.fill:parent;anchors.margins:11;text:root.controller.status || "Cole uma lista, setlist ou músicas copiadas de um show.";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:"#d7eaf3"}
        }
        ColumnLayout {visible:root.step===0;Layout.fillWidth:true;Layout.fillHeight:true
            RowLayout {Layout.fillWidth:true
                ComboBox {id:format;model:["Automático / Artista — Música","Artista — Música","Música — Artista"];Layout.fillWidth:true}
                TextField {id:artist;placeholderText:"Artista padrão (opcional)";maximumLength:300;Layout.fillWidth:true}
            }
            ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true
                TextArea {id:sourceText;objectName:"playlistTextInput";placeholderText:"Nirvana - MTV Unplugged (Full Concert)\n0:00 About a Girl\n3:39 Come as You Are\n\nOu:\nVeruca Salt - Seether\nL7 - Shove";wrapMode:TextEdit.Wrap;selectByMouse:true;color:"#ffffff";font.pixelSize:16}
            }
            Label {text:"Timestamps e cabeçalhos são limpos automaticamente. Até 500 músicas; nenhuma alteração na conta nesta etapa.";color:"#a8b0b7";Layout.fillWidth:true;wrapMode:Text.WordWrap}
        }
        ColumnLayout {visible:root.step===1;Layout.fillWidth:true;Layout.fillHeight:true
            RowLayout {Layout.fillWidth:true
                Label {text:root.controller.selectedCount+" músicas selecionadas";color:"#f5f5f5";Layout.fillWidth:true}
                BusyIndicator {running:root.controller.busy;visible:running;implicitWidth:30;implicitHeight:30}
                Button {text:root.controller.busy?"Interromper busca":"Buscar / tentar pendentes";onClicked:root.controller.busy?root.controller.cancel():root.controller.search()}
            }
            ListView {id:review;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:root.controller.rows;cacheBuffer:180
                delegate: Rectangle {required property var modelData;required property int index;width:review.width;height:116;color:index%2?"#1b2025":"#20262c";radius:10
                    ColumnLayout {anchors.fill:parent;anchors.margins:10;spacing:4
                        RowLayout {Layout.fillWidth:true
                            CheckBox {checked:modelData.selected;enabled:!root.controller.busy&&modelData.choice>=0;onClicked:root.controller.choose(index,modelData.choice,checked)}
                            Label {text:(index+1)+". "+modelData.artist+" — "+modelData.title;textFormat:Text.PlainText;elide:Text.ElideRight;color:"white";font.bold:true;Layout.fillWidth:true}
                            ToolButton {text:"↑";enabled:!root.controller.busy&&index>0;onClicked:root.controller.move(index,-1);ToolTip.text:"Mover para cima";ToolTip.visible:hovered}
                            ToolButton {text:"↓";enabled:!root.controller.busy&&index+1<root.controller.rows.length;onClicked:root.controller.move(index,1);ToolTip.text:"Mover para baixo";ToolTip.visible:hovered}
                            Button {text:"Editar busca";enabled:!root.controller.busy;onClicked:{editDialog.row=index;editArtist.text=modelData.artist;editTitle.text=modelData.title;editDialog.open()}}
                        }
                        RowLayout {Layout.fillWidth:true
                            Label {text:modelData.status;textFormat:Text.PlainText;color:modelData.selected?"#5ed1fa":"#f3c98a";Layout.preferredWidth:150;elide:Text.ElideRight}
                            ComboBox {Layout.fillWidth:true;enabled:!root.controller.busy;model:modelData.candidates.map(t=>t.title+" • "+t.artist+" • "+t.album);currentIndex:modelData.choice;onActivated:root.controller.choose(index,currentIndex,true)}
                        }
                    }
                }
            }
            Label {text:"Correspondências duvidosas ficam desmarcadas. Escolha a versão, corrija a busca ou deixe a faixa fora da playlist.";wrapMode:Text.WordWrap;color:"#a8b0b7";Layout.fillWidth:true}
        }
        ColumnLayout {visible:root.step===2;Layout.fillWidth:true;Layout.fillHeight:true;spacing:16
            TextField {id:playlistName;objectName:"importPlaylistName";placeholderText:"Nome da playlist";text:"Minha playlist de texto";maximumLength:100;Layout.fillWidth:true}
            TextArea {id:description;objectName:"importPlaylistDescription";placeholderText:"Descrição (opcional)";wrapMode:TextEdit.Wrap;Layout.fillWidth:true;Layout.preferredHeight:100;onTextChanged:if(text.length>300)text=text.slice(0,300)}
            RowLayout {Layout.fillWidth:true;spacing:18
                Image {source:root.controller.cover;sourceSize.width:240;sourceSize.height:240;fillMode:Image.PreserveAspectFit;Layout.preferredWidth:200;Layout.preferredHeight:200}
                ColumnLayout {Button {text:"Escolher capa…";onClicked:coverFile.open()}Button {text:"Remover capa";enabled:root.controller.cover.length>0;onClicked:root.controller.clearCover()}Label {text:"A capa fica no backup local. Para enviá-la ao Spotify pode ser necessário autorizar imagens.";color:"#a8b0b7";Layout.maximumWidth:400;wrapMode:Text.WordWrap}}
            }
            CheckBox {id:deduplicate;text:"Remover músicas repetidas";checked:false}
            Item {Layout.fillHeight:true}
        }
        ColumnLayout {visible:root.step===3;Layout.fillWidth:true;Layout.fillHeight:true;spacing:18
            RowLayout {Layout.fillWidth:true;spacing:18
                Image {source:root.controller.details.cover || "";sourceSize.width:220;sourceSize.height:220;fillMode:Image.PreserveAspectFit;Layout.preferredWidth:160;Layout.preferredHeight:160}
                ColumnLayout {Layout.fillWidth:true
                    Label {text:root.controller.details.name || "";textFormat:Text.PlainText;color:"white";font.pixelSize:25;font.bold:true;wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    Label {text:root.controller.details.description || "";textFormat:Text.PlainText;color:"#b2bcc5";wrapMode:Text.WordWrap;Layout.fillWidth:true}
                    Label {text:(root.controller.details.tracks || []).length+" músicas • salva neste aparelho";color:"#63d5ff"}
                }
            }
            Button {text:"Ouvir / abrir playlist local";enabled:!root.controller.busy;onClicked:{let id=root.controller.savedId;root.close();root.localPlaylistRequested(id)}}
            Label {text:"Salvar também na conta Spotify?";font.pixelSize:21;font.bold:true;color:"white"}
            RowLayout {Layout.fillWidth:true
                ComboBox {id:visibility;model:["Privada","Pública"];enabled:!root.controller.busy&&!root.controller.details.spotifyId}
                Button {objectName:"playlistSendSpotify";text:root.controller.details.spotifyId?"Concluir / conferir envio":"Salvar também no Spotify";enabled:!root.controller.busy;onClicked:root.controller.sync(visibility.currentIndex===1)}
                BusyIndicator {running:root.controller.syncing;visible:running;implicitWidth:32;implicitHeight:32}
            }
            Button {text:"Autorizar permissões do Spotify…";visible:root.controller.status.indexOf("Autorize")>=0;enabled:!root.controller.busy;onClicked:root.authorizationRequested()}
            Button {text:"Já conferi a conta; permitir nova criação…";visible:!!root.controller.details.cloudUncertain&&!root.controller.details.spotifyId;enabled:!root.controller.busy;onClicked:uncertain.open()}
            Label {text:"O envio é uma cópia. Você continua com a playlist local se estiver sem rede ou se a API recusar uma operação.";color:"#a8b0b7";Layout.fillWidth:true;wrapMode:Text.WordWrap}
            Item {Layout.fillHeight:true}
        }
        RowLayout {Layout.fillWidth:true
            Button {text:root.step===3?"Concluir":"Fechar";enabled:!root.controller.syncing;onClicked:root.close()}
            Item {Layout.fillWidth:true}
            Button {text:"Voltar";visible:root.step>0&&root.step<3;enabled:!root.controller.busy;onClicked:root.step--}
            Button {objectName:"playlistContinue";text:root.step===0?"Interpretar e buscar":root.step===1?"Personalizar":"Salvar no Tonalify";visible:root.step<3;enabled:!root.controller.busy&&(root.step!==1||root.controller.selectedCount>0)
                onClicked:{if(root.step===0){root.controller.parse(sourceText.text,format.currentIndex,artist.text);if(root.controller.rows.length){root.step=1;root.controller.search()}}else if(root.step===1)root.step=2;else if(root.controller.saveLocal(playlistName.text,description.text,deduplicate.checked))root.step=3}
            }
        }
    }
    FileDialog {id:coverFile;title:"Capa da playlist";nameFilters:["Imagens (*.jpg *.jpeg *.png *.webp)"];onAccepted:root.controller.setCover(selectedFile)}
    Dialog {id:editDialog;property int row:-1;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Corrigir busca";standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {TextField {id:editArtist;placeholderText:"Artista";maximumLength:300;Layout.preferredWidth:500}TextField {id:editTitle;placeholderText:"Música";maximumLength:300;Layout.preferredWidth:500}}
        onAccepted:root.controller.edit(row,editArtist.text,editTitle.text)
    }
    Dialog {id:uncertain;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Você conferiu sua Biblioteca Spotify?";standardButtons:Dialog.Yes|Dialog.No
        Label {text:"A resposta anterior pode ter se perdido depois da criação. Só continue se conferiu a conta e deseja permitir uma nova playlist.";width:480;wrapMode:Text.WordWrap}
        onAccepted:root.controller.acknowledgeUncertainCreate()
    }
}
