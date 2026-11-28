import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    required property var controller
    property int step: 0
    // Section filter: same section names as TextPlaylistImport::sectionOf (Windows) and TextPlaylistWizard (Android).
    property string filter: "all"
    readonly property var sections: [{key:"all",label:"Todas"},{key:"pending",label:"Pendências"},{key:"selected",label:"Encontradas e selecionadas"},{key:"equivalent",label:"Equivalentes para confirmar"},
        {key:"alternate",label:"Versões alternativas"},{key:"ambiguous",label:"Artista ambíguo (cadastros)"},{key:"wrongArtist",label:"Só artista errado"},{key:"notFound",label:"Não encontradas"},
        {key:"unavailable",label:"Indisponíveis na região"},{key:"error",label:"Busca incompleta / erro"},{key:"duplicate",label:"Já está na playlist"},{key:"ignored",label:"Ignoradas"},
        {key:"replaced",label:"Substituídas"},{key:"batch",label:"Só adicionadas agora"},{key:"waiting",label:"Aguardando busca"}]
    readonly property string lastBatch: controller.batches.length?controller.batches[controller.batches.length-1].batchId:""
    property string exitAction: "close"
    property var batchPreview: ({})
    // Leaving with unsaved review work always asks (V3.1 section 13): X, Fechar, Esc and "Interpretar" again.
    function requestExit(action){exitAction=action||"close";if(controller.dirty&&controller.rows.length>0&&step>0&&step<3&&!controller.syncing)exitDialog.open();else proceed()}
    function proceed(){if(exitAction==="parse")startParse();else close()}
    function startParse(){controller.parse(sourceText.text,format.currentIndex,artist.text);if(controller.rows.length){step=1;filter="all";controller.search()}}
    function groupTitle(g){return g==="requested"?"Correspondências do artista solicitado":g==="versions"?"Outras versões e edições do mesmo artista":g==="homonyms"?"Cadastros homônimos a revisar":"Resultados não correspondentes (outros artistas ou músicas)"}
    property var playlistTargets: Playlists.editable || []
    function inSection(row,key){const section=row.section||"waiting";if(key==="batch")return row.batchId===lastBatch&&section!=="replaced";if(section==="replaced")return key==="replaced"||key==="all";
        return key==="all"||key===section||(key==="pending"&&section!=="selected"&&section!=="ignored")}
    function sectionCount(key){let count=0;for(let i=0;i<controller.rows.length;i++)if(inSection(controller.rows[i],key))count++;return count}
    function missingCount(){return sectionCount("pending")}
    function reviewRows(){const result=[];for(let i=0;i<controller.rows.length;i++)if(inSection(controller.rows[i],filter))result.push(Object.assign({},controller.rows[i],{reviewIndex:i}));return result}
    function reasonOf(row){const c=row.choice>=0&&row.choice<row.candidates.length?row.candidates[row.choice]:null;return c&&c.reason?c.reason:(row.reason||"")}
    function statusColor(row){const s=row.section||"";return row.selected?"#5ed1fa":s==="error"?"#ffb36b":(s==="wrongArtist"||s==="notFound")?"#f3c98a":"#c7d0d8"}
    // "Escolher gravação" (V3 section 7): the requested artist, other versions/editions of that artist, homonymous profiles to
    // review and non-matching results, each under its own header; non-matching results always last.
    function versionEntries(candidates){const out=[];const groups=["requested","versions","homonyms","other"];let requested=0;
        for(let i=0;i<candidates.length;i++)if(candidates[i].requestedArtist)requested++;
        for(const g of groups){let header=false;for(let i=0;i<candidates.length;i++){const c=candidates[i];const cg=c.group||(c.requestedArtist?"requested":"other");if(cg!==g)continue;
            if(!header){out.push({header:g==="other"&&!requested?"Nenhuma gravação do artista pedido · resultados não correspondentes":groupTitle(g)});header=true}out.push(Object.assign({index:i},c))}}
        return out}
    function chooseReview(row,candidate,selected){const y=review.contentY;controller.choose(row,candidate,selected);Qt.callLater(()=>review.contentY=Math.min(y,Math.max(0,review.contentHeight-review.height)))}
    signal authorizationRequested()
    signal localPlaylistRequested(string playlistId)
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(960,parent ? parent.width-32 : 900)
    height: Math.min(790,parent ? parent.height-32 : 740)
    modal: true
    title: "Text to Tonalify"
    closePolicy: controller.syncing||(controller.dirty&&step>0&&step<3) ? Popup.NoAutoClose : Popup.CloseOnEscape
    font.family:"Segoe UI";font.pixelSize:14
    palette.window: "#151719";palette.windowText:"#f5f5f5";palette.text: "#f5f5f5";palette.button:"#263440";palette.buttonText: "#f5f5f5";palette.base: "#22262a";palette.highlight:"#009bd6";palette.highlightedText:"#ffffff"
    background: Rectangle {color:"#151719";radius:18;border.color:"#343a42"}
    onClosed: if(!controller.syncing)controller.cancel()
    function resume(id) {if(controller.loadSaved(id)){step=3;open()}}
    function fresh() {step=0;controller.refreshDrafts();open()}
    Shortcut {sequences:[StandardKey.Cancel];enabled:root.opened&&!exitDialog.opened&&root.closePolicy===Popup.NoAutoClose&&!root.controller.syncing;onActivated:root.requestExit("close")}
    ColumnLayout {
        anchors.fill: parent; spacing:14
        Label { text: step===0?"Sua próxima playlist começa com um texto":step===1?"Escolha as gravações certas":step===2?"Dê sua identidade à playlist":"Pronta para ouvir";color:"#ffffff";font.pixelSize:25;font.bold:true;Layout.fillWidth:true;wrapMode:Text.WordWrap }
        Label {text:"1 · Colar texto     2 · Revisar músicas     3 · Personalizar     4 · Salvar";color:"#86cce7";font.pixelSize:13}
        Rectangle {Layout.fillWidth:true;implicitHeight:notice.implicitHeight+22;color:"#202c35";radius:10
            Label {id:notice;anchors.fill:parent;anchors.margins:11;text:root.controller.status || "Cole uma lista, setlist ou músicas copiadas de um show.";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:"#d7eaf3"}
        }
        ColumnLayout {visible:root.step===0;Layout.fillWidth:true;Layout.fillHeight:true
            ColumnLayout {objectName:"draftsPanel";visible:root.controller.drafts.length>0;Layout.fillWidth:true;spacing:6
                Label {text:"Continuar rascunho";color:"#ffffff";font.bold:true;font.pixelSize:16}
                Repeater {model:root.controller.drafts
                    Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:54;radius:10;color:"#20262c"
                        RowLayout {anchors.fill:parent;anchors.margins:8;spacing:8
                            ColumnLayout {Layout.fillWidth:true;spacing:0
                                Label {text:modelData.name || "Rascunho sem nome";textFormat:Text.PlainText;color:modelData.recovery?"#f3c98a":"white";font.bold:true;elide:Text.ElideRight;Layout.fillWidth:true}
                                Label {text:modelData.total+" músicas · "+modelData.selected+" selecionadas · "+modelData.pending+" pendentes · "+String(modelData.updatedAt).replace("T"," ").slice(0,16);color:"#a8b0b7";font.pixelSize:12}
                            }
                            Button {objectName:"draftContinue";text:"Continuar";onClicked:if(root.controller.loadDraft(modelData.id)){root.step=1;root.filter="all"}}
                            Button {text:"Renomear";visible:!modelData.recovery;onClicked:{renameDialog.draft=modelData.id;renameField.text=modelData.name;renameDialog.open()}}
                            Button {text:"Excluir";onClicked:{deleteDraftDialog.draft=modelData.id;deleteDraftDialog.name=modelData.name;deleteDraftDialog.open()}}
                        }
                    }
                }
                Label {text:"Ou cole uma nova lista abaixo para criar outra playlist; os rascunhos acima não são alterados.";color:"#a8b0b7";font.pixelSize:12}
            }
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
                Label {objectName:"reviewSummary";text:root.controller.summary.active+" ativas"+(root.controller.summary.replacedTotal?" ("+root.controller.summary.replacedTotal+" substituídas)":"")+" · "+root.controller.selectedCount+" selecionadas · "+root.missingCount()+" pendentes · "+root.sectionCount("ignored")+" ignoradas · a salvar: "+root.controller.summary.toSave;color:"#f5f5f5";Layout.fillWidth:true;wrapMode:Text.WordWrap}
                ComboBox {objectName:"sectionFilter";Layout.preferredWidth:300;enabled:!root.controller.busy;model:root.sections.map(s=>s.label+" ("+root.sectionCount(s.key)+")");currentIndex:Math.max(0,root.sections.findIndex(s=>s.key===root.filter));onActivated:index=>{root.filter=root.sections[index].key;review.positionViewAtBeginning()}}
                Button {objectName:"missingFilter";text:root.filter==="pending"?"Mostrar todas":"Só pendências ("+root.missingCount()+")";enabled:!root.controller.busy;onClicked:{root.filter=root.filter==="pending"?"all":"pending";review.positionViewAtBeginning()}}
                BusyIndicator {running:root.controller.busy;visible:running;implicitWidth:30;implicitHeight:30}
                Button {text:root.controller.busy?"Interromper busca":"Buscar / tentar pendentes";onClicked:root.controller.busy?root.controller.cancel():root.controller.search()}
            }
            RowLayout {Layout.fillWidth:true
                Button {objectName:"addMoreSongs";text:"Adicionar mais músicas";enabled:!root.controller.busy;onClicked:{addText.text="";root.batchPreview={};addDialog.open()}}
                Button {objectName:"undoBatch";text:"Desfazer última importação";visible:root.controller.batches.length>1;enabled:!root.controller.busy;onClicked:root.controller.undoLastBatch()}
                Button {objectName:"saveDraftButton";text:"Salvar rascunho";enabled:!root.controller.busy&&root.controller.dirty;onClicked:root.controller.saveDraft()}
                Item {Layout.fillWidth:true}
                Label {visible:Object.keys(root.controller.pins).length>0;text:"Cadastro fixado: "+Object.keys(root.controller.pins).map(k=>root.controller.pins[k].name+" ("+root.controller.pins[k].album+")").join(", ");color:"#8ed7f2";font.pixelSize:12}
            }
            ListView {id:review;objectName:"reviewList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:root.reviewRows();cacheBuffer:180
                delegate: Rectangle {id:reviewCard;required property var modelData;required property int index;readonly property int sourceIndex:modelData.reviewIndex;opacity:modelData.section==="replaced"?0.55:1;readonly property var selectedTrack:modelData.choice>=0&&modelData.choice<modelData.candidates.length?modelData.candidates[modelData.choice]:({title:"",artist:"",album:"",image:""});width:review.width;height:176;color:index%2?"#1b2025":"#20262c";radius:10
                    ColumnLayout {anchors.fill:parent;anchors.margins:10;spacing:6
                        RowLayout {Layout.fillWidth:true
                            CheckBox {checked:modelData.selected;enabled:!root.controller.syncing&&modelData.choice>=0;onClicked:root.chooseReview(sourceIndex,modelData.choice,checked);Accessible.name:"Incluir "+modelData.title}
                            Rectangle {width:56;height:56;radius:8;color:"#303842"
                                Image {id:reviewArt;anchors.fill:parent;source:reviewCard.selectedTrack.image || "";asynchronous:true;fillMode:Image.PreserveAspectCrop;sourceSize.width:112;sourceSize.height:112}
                                Label {anchors.centerIn:parent;visible:reviewArt.status!==Image.Ready;text:"♪";font.pixelSize:24;color:"#a8b0b7"}
                            }
                            ColumnLayout {Layout.fillWidth:true
                                Label {text:(sourceIndex+1)+". "+modelData.title;textFormat:Text.PlainText;elide:Text.ElideRight;color:"white";font.bold:true;Layout.fillWidth:true}
                                Label {text:modelData.artist+" • "+modelData.status+(modelData.replacementOf>0?" · substitui uma pendência":"")+(modelData.batchId&&modelData.batchId!=="b1"?" · "+modelData.batchId.replace("b","lote "):"");textFormat:Text.PlainText;color:root.statusColor(modelData);Layout.fillWidth:true;elide:Text.ElideRight}
                            }
                            ToolButton {text:"↑";enabled:!root.controller.busy&&sourceIndex>0;onClicked:root.controller.move(sourceIndex,-1);ToolTip.text:"Mover para cima";ToolTip.visible:hovered}
                            ToolButton {text:"↓";enabled:!root.controller.busy&&sourceIndex+1<root.controller.rows.length;onClicked:root.controller.move(sourceIndex,1);ToolTip.text:"Mover para baixo";ToolTip.visible:hovered}
                            Button {text:"Editar busca";enabled:!root.controller.busy;onClicked:{editDialog.row=sourceIndex;editArtist.text=modelData.artist;editTitle.text=modelData.title;editDialog.open()}}
                        }
                        RowLayout {Layout.fillWidth:true
                            ColumnLayout {Layout.fillWidth:true;Layout.leftMargin:66
                                Label {visible:!!selectedTrack.title;text:selectedTrack.title+" · "+selectedTrack.artist;textFormat:Text.PlainText;color:"white";Layout.fillWidth:true;elide:Text.ElideRight}
                                Label {visible:!!selectedTrack.album;text:selectedTrack.album;textFormat:Text.PlainText;color:"#a8b0b7";Layout.fillWidth:true;elide:Text.ElideRight}
                                Label {objectName:"reviewReason";visible:text.length>0;text:root.reasonOf(modelData);textFormat:Text.PlainText;color:"#9fb3c2";font.pixelSize:12;Layout.fillWidth:true;elide:Text.ElideRight;ToolTip.text:text;ToolTip.visible:reasonHover.hovered;HoverHandler{id:reasonHover}}
                            }
                            Button {text:"Gravações";enabled:!root.controller.syncing&&modelData.candidates.length>0;onClicked:{versionDialog.row=sourceIndex;versionDialog.currentChoice=modelData.choice;versionDialog.candidates=modelData.candidates;versionDialog.requested=modelData.artist;versionDialog.open()}}
                            Button {objectName:"reviewRetry";text:modelData.section==="notFound"||modelData.section==="wrongArtist"?"Buscar na discografia":"Tentar de novo";visible:modelData.section!=="replaced"&&(modelData.manual||modelData.section==="error"||modelData.section==="notFound"||modelData.section==="wrongArtist");enabled:!root.controller.busy;onClicked:root.controller.retry(sourceIndex)}
                            Button {objectName:"reviewIgnore";text:"Ignorar";visible:!modelData.ignored&&!modelData.manual&&modelData.section!=="replaced"&&modelData.section!=="error"&&modelData.section!=="notFound"&&modelData.section!=="wrongArtist";enabled:!root.controller.syncing;onClicked:root.controller.ignore(sourceIndex)}
                        }
                    }
                }
            }
            Label {text:"Encontradas e equivalentes comprovadas ficam marcadas. Versões alternativas, ambíguas e resultados de outros artistas ficam desmarcados; homônimos nunca contam como encontrados.";wrapMode:Text.WordWrap;color:"#a8b0b7";Layout.fillWidth:true}
        }
        ColumnLayout {visible:root.step===2;Layout.fillWidth:true;Layout.fillHeight:true;spacing:16
            ComboBox {id:importTarget;objectName:"importTargetPlaylist";textRole:"name";valueRole:"id";model:[{id:"",name:"Criar uma nova playlist"}].concat(root.playlistTargets);Layout.fillWidth:true}
            Label {visible:importTarget.currentIndex>0;text:"As músicas revisadas serão acrescentadas ao final da playlist escolhida. O conteúdo existente não será substituído; uma cópia local das adições fica neste aparelho.";color:"#8ed7f2";wrapMode:Text.WordWrap;Layout.fillWidth:true}
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
            Label {text:root.controller.details.appendMode?"Adicionar à playlist Spotify existente":"Salvar também na conta Spotify?";font.pixelSize:21;font.bold:true;color:"white"}
            RowLayout {Layout.fillWidth:true
                ComboBox {id:visibility;model:["Privada","Pública"];visible:!root.controller.details.appendMode;enabled:!root.controller.busy&&!root.controller.details.spotifyId}
                Button {objectName:"playlistSendSpotify";text:root.controller.details.appendMode?"Adicionar músicas à playlist":root.controller.details.spotifyId?"Concluir / conferir envio":"Salvar também no Spotify";enabled:!root.controller.busy;onClicked:root.controller.sync(visibility.currentIndex===1)}
                BusyIndicator {running:root.controller.syncing;visible:running;implicitWidth:32;implicitHeight:32}
            }
            Button {text:"Autorizar permissões do Spotify…";visible:root.controller.status.indexOf("Autorize")>=0;enabled:!root.controller.busy;onClicked:root.authorizationRequested()}
            Button {text:"Já conferi a conta; permitir nova criação…";visible:!!root.controller.details.cloudUncertain&&!root.controller.details.spotifyId;enabled:!root.controller.busy;onClicked:uncertain.open()}
            Label {text:"O envio é uma cópia. Você continua com a playlist local se estiver sem rede ou se a API recusar uma operação.";color:"#a8b0b7";Layout.fillWidth:true;wrapMode:Text.WordWrap}
            Item {Layout.fillHeight:true}
        }
        RowLayout {Layout.fillWidth:true
            Button {objectName:"playlistClose";text:root.step===3?"Concluir":"Fechar";enabled:!root.controller.syncing;onClicked:root.requestExit("close")}
            Item {Layout.fillWidth:true}
            Button {text:"Voltar";visible:root.step>0&&root.step<3;enabled:!root.controller.busy;onClicked:root.step--}
            Button {objectName:"playlistContinue";text:root.step===0?"Interpretar e buscar":root.step===1?"Personalizar":importTarget.currentIndex>0?"Salvar adições no Tonalify":"Salvar no Tonalify";visible:root.step<3;enabled:!root.controller.busy&&(root.step!==1||root.controller.selectedCount>0)
                onClicked:{if(root.step===0){if(root.controller.dirty&&root.controller.rows.length>0){root.exitAction="parse";exitDialog.open()}else root.startParse()}else if(root.step===1)root.step=2;else {root.controller.setTargetPlaylist(importTarget.currentIndex>0&&importTarget.currentIndex<=root.playlistTargets.length?root.playlistTargets[importTarget.currentIndex-1].id:"");if(root.controller.saveLocal(playlistName.text,description.text,deduplicate.checked))root.step=3}}
            }
        }
    }
    FileDialog {id:coverFile;title:"Capa da playlist";nameFilters:["Imagens (*.jpg *.jpeg *.png *.webp)"];onAccepted:root.controller.setCover(selectedFile)}
    Dialog {id:versionDialog;objectName:"versionDialog";property int row:-1;property int currentChoice:-1;property var candidates:[];property string requested:"";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Escolher gravação";width:Math.min(760,parent.width-32);height:Math.min(640,parent.height-48);standardButtons:Dialog.Close
        palette.window:"#151719";palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";palette.button:"#263440";palette.buttonText:"#f5f5f5"
        background:Rectangle{color:"#151719";radius:18;border.color:"#343a42"}
        ColumnLayout{anchors.fill:parent;spacing:10
            Label{text:"Gravações do artista pedido aparecem primeiro, com o motivo da classificação. Resultados de outros artistas ficam separados.";color:"#a8b0b7";wrapMode:Text.WordWrap;Layout.fillWidth:true}
            ListView{objectName:"versionList";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8;model:root.versionEntries(versionDialog.candidates)
                delegate:Rectangle{required property var modelData;readonly property bool isHeader:modelData.header!==undefined;width:ListView.view.width;height:isHeader?34:92;radius:10;color:isHeader?"transparent":modelData.index===versionDialog.currentChoice?"#29424d":"#20262c"
                    Label{visible:isHeader;anchors.verticalCenter:parent.verticalCenter;x:6;text:modelData.header || "";color:"#f3c98a";font.bold:true}
                    RowLayout{visible:!isHeader;anchors.fill:parent;anchors.margins:10;spacing:12
                        Rectangle{width:56;height:56;radius:8;color:"#303842"
                            Image{id:candidateArt;anchors.fill:parent;source:isHeader?"":(modelData.image || "");asynchronous:true;fillMode:Image.PreserveAspectCrop;sourceSize.width:112;sourceSize.height:112}
                            Label{anchors.centerIn:parent;visible:candidateArt.status!==Image.Ready;text:"♪";font.pixelSize:24;color:"#a8b0b7"}
                        }
                        ColumnLayout{Layout.fillWidth:true;spacing:1
                            Label{text:modelData.title || "";textFormat:Text.PlainText;color:"white";font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}
                            Label{text:(modelData.artist || "")+(modelData.album?" · "+modelData.album:"");textFormat:Text.PlainText;color:"#8ed7f2";Layout.fillWidth:true;elide:Text.ElideRight}
                            Label{text:(modelData.matchLabel || "")+(modelData.reason?" · "+modelData.reason:"");textFormat:Text.PlainText;color:modelData.requestedArtist?"#a8b0b7":"#f3c98a";font.pixelSize:12;Layout.fillWidth:true;wrapMode:Text.WordWrap;maximumLineCount:2;elide:Text.ElideRight}
                        }
                        Label{visible:!isHeader&&modelData.index===versionDialog.currentChoice;text:"✓";font.pixelSize:24;color:"#63d5ff"}
                    }
                    MouseArea{enabled:!isHeader;anchors.fill:parent;cursorShape:Qt.PointingHandCursor
                        onClicked:{const g=modelData.group||(modelData.requestedArtist?"requested":"other");
                            if(g==="homonyms"){pinDialog.index=modelData.index;pinDialog.detail=modelData.title+" · "+modelData.artist+" · "+(modelData.album||"")+(modelData.year?" ("+modelData.year+")":"")+"\n\n"+(modelData.reason||"");pinDialog.open()}
                            else if(g==="other"){otherArtist.index=modelData.index;otherArtist.detail=modelData.title+" · "+modelData.artist+"\n\n"+(modelData.reason||"");otherArtist.open()}
                            else{root.chooseReview(versionDialog.row,modelData.index,true);versionDialog.close()}}}
                }
            }
        }
    }
    Dialog {id:otherArtist;objectName:"otherArtistDialog";property int index:-1;property string detail:"";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Usar resultado de outro artista?";standardButtons:Dialog.Yes|Dialog.Cancel
        Label{width:480;wrapMode:Text.WordWrap;text:otherArtist.detail+"\n\nIsso não é a gravação pedida ("+versionDialog.requested+"). Use só se for mesmo a substituição desejada."}
        onAccepted:{root.chooseReview(versionDialog.row,index,true);versionDialog.close()}
    }
    Dialog {id:pinDialog;objectName:"pinArtistDialog";property int index:-1;property string detail:"";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Cadastro homônimo no Spotify"
        ColumnLayout{spacing:10
            Label{Layout.preferredWidth:520;wrapMode:Text.WordWrap;text:pinDialog.detail+"\n\nUsar este cadastro só para esta música ou para todas as músicas de "+versionDialog.requested+" nesta lista? Você pode desfazer depois."}
            RowLayout{Button{objectName:"pinForList";text:"Fixar para a lista";onClicked:{root.chooseReview(versionDialog.row,pinDialog.index,true);root.controller.pinArtist(versionDialog.row,pinDialog.index);pinDialog.close();versionDialog.close()}}
                Button{text:"Só esta música";onClicked:{root.chooseReview(versionDialog.row,pinDialog.index,true);pinDialog.close();versionDialog.close()}}
                Button{text:"Cancelar";onClicked:pinDialog.close()}}
        }
    }
    // V3.1 13.1: exactly three actions; tapping outside or Esc never discards.
    Dialog {id:exitDialog;objectName:"exitDraftDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;closePolicy:Popup.NoAutoClose;title:root.exitAction==="parse"?"Trocar a lista em revisão?":"Sair da criação da playlist?"
        palette.window:"#151719";palette.windowText:"#f5f5f5";palette.button:"#263440";palette.buttonText:"#f5f5f5"
        ColumnLayout{spacing:12
            Label{Layout.preferredWidth:520;wrapMode:Text.WordWrap;text:"Você pode salvar um rascunho e continuar depois. As músicas, escolhas e pendências serão mantidas."}
            RowLayout{spacing:8
                Button{objectName:"exitSaveDraft";text:"Salvar rascunho e sair";onClicked:{if(root.controller.saveDraft()){exitDialog.close();root.proceed()}}}
                Button{objectName:"exitDiscard";text:"Sair sem salvar";onClicked:{root.controller.discardChanges();exitDialog.close();root.proceed()}}
                Button{objectName:"exitKeepEditing";text:"Continuar editando";highlighted:true;onClicked:exitDialog.close()}
            }
            Label{visible:root.controller.status.indexOf("Não foi possível salvar")>=0||root.controller.status.indexOf("não pôde ser conferido")>=0;text:root.controller.status;color:"#ffb36b";wrapMode:Text.WordWrap;Layout.preferredWidth:520}
        }
    }
    Dialog {id:addDialog;objectName:"addSongsDialog";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Adicionar mais músicas a esta playlist";width:Math.min(720,parent.width-32);height:Math.min(640,parent.height-48)
        palette.window:"#151719";palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";palette.button:"#263440";palette.buttonText:"#f5f5f5";palette.base:"#22262a"
        background:Rectangle{color:"#151719";radius:18;border.color:"#343a42"}
        ColumnLayout{anchors.fill:parent;spacing:8
            Label{text:"Mesmo formato da lista inicial (Artista - Música). As músicas já revisadas, escolhas e capa ficam como estão.";color:"#a8b0b7";wrapMode:Text.WordWrap;Layout.fillWidth:true}
            ComboBox{id:addMode;objectName:"addMode";model:["Adicionar ao final (o total cresce)","Substituir pendentes (mantém o total)"];Layout.fillWidth:true;onActivated:root.batchPreview=({})}
            ScrollView{Layout.fillWidth:true;Layout.preferredHeight:150;clip:true
                TextArea{id:addText;objectName:"addSongsText";placeholderText:"Artista - Música\nArtista - Música";wrapMode:TextEdit.Wrap;color:"white";onTextChanged:root.batchPreview=({})}}
            Button{objectName:"addPreview";text:"Conferir lista";enabled:addText.text.trim().length>0;onClicked:root.batchPreview=root.controller.previewBatch(addText.text,format.currentIndex,artist.text)}
            Label{objectName:"addPreviewText";visible:root.batchPreview.count!==undefined||root.batchPreview.error!==undefined;Layout.fillWidth:true;wrapMode:Text.WordWrap;color:"#d7eaf3"
                text:root.batchPreview.error?root.batchPreview.error:(root.batchPreview.count||0)+" referência(s) reconhecida(s) · "+(root.batchPreview.newCount||0)+" nova(s) · "+((root.batchPreview.duplicates||[]).length)+" já está(ão) na lista"+(root.batchPreview.repeatedList?" · esta mesma lista já foi adicionada":"")
                    +((root.batchPreview.duplicates||[]).length?"\nJá na lista: "+root.batchPreview.duplicates.map(d=>d.artist+" - "+d.title+" (linha "+d.existingRow+")").join("; "):"")}
            ListView{id:pendingList;visible:addMode.currentIndex===1&&(root.batchPreview.pending||[]).length>0;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:root.batchPreview.pending||[]
                property var chosen:({})
                delegate:CheckBox{required property var modelData;required property int index;width:ListView.view.width;text:modelData.artist+" - "+modelData.title+" ("+modelData.section+")";checked:index<(root.batchPreview.newCount||0);
                    onCheckedChanged:{const c=Object.assign({},pendingList.chosen);c[modelData.requestId]=checked;pendingList.chosen=c}Component.onCompleted:{const c=Object.assign({},pendingList.chosen);c[modelData.requestId]=checked;pendingList.chosen=c}}}
            Item{visible:!pendingList.visible;Layout.fillHeight:true}
            CheckBox{id:includeDuplicates;objectName:"addIncludeDuplicates";visible:((root.batchPreview.duplicates||[]).length>0)||!!root.batchPreview.repeatedList;text:"Adicionar mesmo assim (repetições intencionais)";checked:false}
            RowLayout{Item{Layout.fillWidth:true}
                Button{text:"Cancelar";onClicked:addDialog.close()}
                Button{objectName:"addConfirm";text:"Adicionar à playlist atual";enabled:root.batchPreview.count>0&&!root.controller.busy
                    onClicked:{const ids=[];if(addMode.currentIndex===1)for(const p of (root.batchPreview.pending||[]))if(pendingList.chosen[p.requestId])ids.push(p.requestId);
                        if(root.controller.addBatch(addText.text,format.currentIndex,artist.text,addMode.currentIndex===1?"replace":"add",ids,includeDuplicates.checked)){addDialog.close();root.filter="batch"}}}
            }
        }
    }
    Dialog {id:renameDialog;property string draft:"";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Renomear rascunho";standardButtons:Dialog.Ok|Dialog.Cancel
        TextField{id:renameField;maximumLength:100;width:420}
        onAccepted:root.controller.renameDraft(draft,renameField.text)
    }
    Dialog {id:deleteDraftDialog;objectName:"deleteDraftDialog";property string draft:"";property string name:"";parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Excluir rascunho?";standardButtons:Dialog.Yes|Dialog.Cancel
        Label{width:420;wrapMode:Text.WordWrap;text:"\""+deleteDraftDialog.name+"\" será apagado deste computador. Os outros rascunhos e as playlists já salvas não são afetados."}
        onAccepted:root.controller.deleteDraft(draft)
    }
    Dialog {id:editDialog;property int row:-1;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Corrigir busca";standardButtons:Dialog.Ok|Dialog.Cancel
        ColumnLayout {TextField {id:editArtist;placeholderText:"Artista";maximumLength:300;Layout.preferredWidth:500}TextField {id:editTitle;placeholderText:"Música";maximumLength:300;Layout.preferredWidth:500}}
        onAccepted:root.controller.edit(row,editArtist.text,editTitle.text)
    }
    Dialog {id:uncertain;parent:Overlay.overlay;anchors.centerIn:parent;modal:true;title:"Você conferiu sua Biblioteca Spotify?";standardButtons:Dialog.Yes|Dialog.No
        Label {text:"A resposta anterior pode ter se perdido depois da criação. Só continue se conferiu a conta e deseja permitir uma nova playlist.";width:480;wrapMode:Text.WordWrap}
        onAccepted:root.controller.acknowledgeUncertainCreate()
    }
}
