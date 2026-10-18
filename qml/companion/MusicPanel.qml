import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var controller
    required property var audioEngine
    property var mixController:null
    property var transportPlayer:null
    property var songQueueSource:[]
    property var queuedRows:[]
    property bool queueEdited:false
    property string queueAnchor:""
    function queueRows(){return transportPlayer ? transportPlayer.queue.map(t=>({provider:"Spotify",id:t.uri,uri:t.uri,title:t.name,artist:t.artists,image:t.coverUrl,durationMs:t.durationMs})) : []}
    function openQueue(){queueEdited=false;queueAnchor=controller.current.uri || "";songQueueSource=[controller.current].concat(queueRows());queuedRows=songQueueSource;queueDialog.open();if(transportPlayer)transportPlayer.refreshQueue()}
    function remixSong(shake){
        if(!songQueueSource.length||queueAnchor!==(controller.current.uri || "")){queueAnchor=controller.current.uri || "";songQueueSource=[controller.current].concat(queueRows())}
        let source=songQueueSource.slice();if(shake&&mixController&&source.length>2){let rest=source.slice(1);source=[source[0]].concat(mixController.order(rest.length,Math.floor(Math.random()*2147483647)).map(i=>rest[i]))}
        queueEdited=true;queuedRows=[];queueDialog.open();mixController.preview(source,0,randomPlayback,discoveryPlayback,false)
    }
    Connections{target:panel.transportPlayer;ignoreUnknownSignals:true;function onQueueChanged(){if(queueDialog.visible&&!panel.queueEdited){panel.songQueueSource=[panel.controller.current].concat(panel.queueRows());panel.queuedRows=panel.songQueueSource}}}
    Connections{target:panel.mixController;ignoreUnknownSignals:true;function onChanged(){if(panel.queueEdited&&!panel.mixController.busy)panel.queuedRows=panel.mixController.previewQueue}}

    property bool listShaken:false
    property bool randomPlayback:false
    property bool discoveryPlayback:false
    property int mixSeed:Math.floor(Math.random()*2147483647)
    property bool textPlaylistAvailable:false
    signal textPlaylistRequested()
    signal textPlaylistResume(string id)
    property int position: 0
    property bool playing: false
    property int revision: 0
    property string selectedPlaylist: ""
    property var displayTracks: []
    property var drillTracks: null
    property var stats: ({rows:[],recommendations:[]})
    property string notice: ""
    property string lyricTrack: ""
    property string translatedTrack: ""
    property bool autoTranslation: true
    property alias section: tabs.currentIndex
    function ensureLyrics() {
        if (!visible || tabs.currentIndex!==2 || !controller.current.title) return
        const track=controller.current.provider+"|"+(controller.current.id || controller.current.uri)
        if(lyricTrack!==track){lyricTrack=track;translatedTrack="";if(!controller.lines.length&&!controller.busy)controller.fetchLyrics()}
        if(autoTranslation&&!controller.busy&&controller.lines.length&&translatedTrack!==track){translatedTrack=track;controller.translate("auto")}
    }
    function showSong(){tabs.currentIndex=2;open();ensureLyrics()}
    property bool followLyrics: true
    property int highlighted: -1
    property real phase: 0
    signal discover(string artist)
    signal togglePlayback()
    signal previousRequested()
    signal nextRequested()
    parent: Overlay.overlay
    width: Math.min(1120,parent ? parent.width-32 : 1000)
    height: Math.min(800,parent ? parent.height-90 : 700)
    x: parent ? (parent.width-width)/2 : 0
    y:72
    modal: false
    title: "Sua música"
    palette.window:"#141517";palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";palette.base:"#20252d";palette.button:"#263440";palette.buttonText:"#f5f5f5"
    standardButtons: Dialog.NoButton
    background: Rectangle { color: "#141517"; radius: 18; border.color: "#303237" }
    function playSelection(index){if(mixController)mixController.play(displayTracks,index,randomPlayback,discoveryPlayback,false);else controller.play(displayTracks,index)}
    function refresh() {
        revision++
        stats=controller.statistics(period.currentValue || 7,dimension.currentValue || "artists")
        displayTracks=drillTracks!==null ? drillTracks : selectedPlaylist ? controller.playlistTracks(selectedPlaylist) : controller.favorites
        if(listShaken&&mixController){let original=displayTracks;displayTracks=mixController.order(original.length,mixSeed).map(i=>original[i])}
    }
    onOpened: {refresh();controller.refreshMetadata();ensureLyrics()}
    onPositionChanged: {
        const found=controller.lineAt(position)
        highlighted=found
        if(followLyrics && found>=0) lyrics.positionViewAtIndex(found,ListView.Center)
    }
    Connections {
        target: panel.controller
        function onChanged(){if(panel.visible){panel.refresh();if(panel.audioEngine)panel.audioEngine.visualizerActive=tabs.currentIndex===2&&(panel.controller.background==="Ondas"||panel.controller.background==="Espectro")}}
        function onLyricsChanged(){lyrics.model=panel.controller.lines;Qt.callLater(panel.ensureLyrics)}
        function onCurrentChanged(){Qt.callLater(panel.ensureLyrics)}
        function onMessage(text){panel.notice=text}
    }
    onVisibleChanged: if(audioEngine) audioEngine.visualizerActive=visible && tabs.currentIndex===2 && (controller.background==="Ondas" || controller.background==="Espectro")
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: panel.controller.current.title || "Escolha uma música no player"; textFormat: Text.PlainText; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
            ActionIcon { symbol:"heart";filled:{panel.revision;return panel.controller.liked(panel.controller.current)} description:filled?"Remover das favoritas":"Favoritar música"; enabled: !!panel.controller.current.title; onClicked: panel.controller.favorite(panel.controller.current) }
            ActionIcon { symbol:"plus";description:"Adicionar à playlist"; enabled: !!panel.controller.current.title; onClicked: addMenu.open() }
            ActionIcon {symbol:"save";description:"Backup da biblioteca";onClicked:backupMenu.open()}
            ActionIcon {symbol:"close";description:"Fechar página da música";onClicked:panel.close()}
            Menu { id: backupMenu; MenuItem { text:"Exportar backup…"; onTriggered: backupSave.open() } MenuItem { text:"Restaurar backup…"; onTriggered: restoreConfirm.open() } }
            Menu { id: addMenu; Repeater { model: panel.controller.playlists; MenuItem { required property var modelData; text: modelData.name; onTriggered: panel.controller.addToPlaylist(modelData.id,panel.controller.current) } } MenuItem { text: "Criar playlist…"; onTriggered: createDialog.open() } MenuItem {text:"Criar com Text to Tonalify…";visible:panel.textPlaylistAvailable;onTriggered:panel.textPlaylistRequested()} }
        }
        Label { visible: panel.notice.length>0; text: panel.notice; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#ffcc8d"; Layout.fillWidth: true }
        TabBar {
            id: tabs; Layout.fillWidth: true
            onCurrentIndexChanged: {if(panel.audioEngine)panel.audioEngine.visualizerActive=panel.visible && currentIndex===2 && (panel.controller.background==="Ondas" || panel.controller.background==="Espectro");Qt.callLater(panel.ensureLyrics)}
            TabButton { Accessible.name:"Biblioteca";contentItem:ActionIcon{symbol:"library";description:"Biblioteca";filled:tabs.currentIndex===0;onClicked:tabs.currentIndex=0} }
            TabButton { Accessible.name:"Estatísticas";contentItem:ActionIcon{symbol:"chart";description:"Estatísticas";filled:tabs.currentIndex===1;onClicked:tabs.currentIndex=1} }
            TabButton { Accessible.name:"Letras";contentItem:ActionIcon{symbol:"lyrics";description:"Letras";filled:tabs.currentIndex===2;onClicked:tabs.currentIndex=2} }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: tabs.currentIndex
            ColumnLayout {
                RowLayout {visible:!!panel.mixController
                    ActionIcon{symbol:"shake";description:"Shake: sortear ordem; segure para restaurar";filled:panel.listShaken;onClicked:{panel.mixController.cancel();panel.listShaken=true;panel.mixSeed=Math.floor(Math.random()*2147483647);panel.refresh()}onPressAndHold:{panel.listShaken=false;panel.refresh()}}
                    ActionIcon{symbol:"random";description:"Reprodução aleatória";filled:panel.randomPlayback;onClicked:{panel.mixController.cancel();panel.randomPlayback=!panel.randomPlayback}}
                    ActionIcon{symbol:"discover";description:"Descoberta 2+1";filled:panel.discoveryPlayback;onClicked:{panel.mixController.cancel();panel.discoveryPlayback=!panel.discoveryPlayback}}
                    Label{text:panel.mixController?panel.mixController.status:"";wrapMode:Text.WordWrap;Layout.fillWidth:true;color:"#9bcfe1"}
                }
                RowLayout {
                    ComboBox { Layout.fillWidth: true; textRole: "name"; valueRole: "id"; model: [{name:"Músicas favoritas",id:""}].concat(panel.controller.playlists); onActivated: {panel.drillTracks=null;panel.selectedPlaylist=currentValue;panel.refresh()} }
                    Button { text: "Criar"; onClicked: createDialog.open() }
                    Button { text: "Renomear"; enabled: !!panel.selectedPlaylist; onClicked: renameDialog.open() }
                    Button { text: "Excluir playlist"; enabled: !!panel.selectedPlaylist; onClicked: deleteDialog.open() }
                }
                RowLayout {visible:panel.textPlaylistAvailable
                    Button {text:"Criar com Text to Tonalify…";onClicked:panel.textPlaylistRequested()}
                    Button {text:"Enviar / concluir no Spotify…";enabled:panel.selectedPlaylist.length>0;onClicked:panel.textPlaylistResume(panel.selectedPlaylist)}
                }
                RowLayout {visible:panel.selectedPlaylist.length>0;Layout.fillWidth:true
                    property var info:{panel.revision;return panel.selectedPlaylist&&typeof panel.controller.playlistDetails==="function"?panel.controller.playlistDetails(panel.selectedPlaylist):({})}
                    Image {source:parent.info.cover || "";visible:source.toString().length>0;sourceSize.width:120;sourceSize.height:120;Layout.preferredWidth:90;Layout.preferredHeight:90;fillMode:Image.PreserveAspectFit}
                    ColumnLayout {Layout.fillWidth:true
                        Label {text:parent.parent.info.name || "";textFormat:Text.PlainText;font.pixelSize:20;font.bold:true;Layout.fillWidth:true;elide:Text.ElideRight}
                        Label {text:parent.parent.info.description || "";textFormat:Text.PlainText;wrapMode:Text.WordWrap;color:"#b5c9d6";Layout.fillWidth:true}
                    }
                }
                Label { text: "Coleções pessoais neste aparelho. No Tonalify, as playlists da conta também estão em Sua Biblioteca."; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#b5c9d6" }
                ListView {
                    id: saved; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 6; model: panel.displayTracks
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData; required property int index
                        width: saved.width; height: 72; color: "#21313e"; radius: 8
                        RowLayout { anchors.fill: parent; anchors.margins: 10
                            ColumnLayout { Layout.fillWidth: true
                                Label { text: modelData.title; textFormat: Text.PlainText; elide: Text.ElideRight; Layout.fillWidth: true; font.bold: true }
                                Label { text: (modelData.artist || "Artista não informado")+" • "+modelData.provider; textFormat: Text.PlainText; color: "#b5c9d6"; elide: Text.ElideRight; Layout.fillWidth: true }
                            }
                            Button { text: "▶"; onClicked: panel.playSelection(index) }
                            Button { text: panel.selectedPlaylist?"Remover":"♥"; onClicked: panel.selectedPlaylist?panel.controller.removeFromPlaylist(panel.selectedPlaylist,index):panel.controller.favorite(modelData) }
                        }
                    }
                    Label { anchors.centerIn: parent; visible: saved.count===0; text: "Marque uma música com ♥ ou adicione faixas a uma playlist."; wrapMode: Text.WordWrap; width: parent.width-30; horizontalAlignment: Text.AlignHCenter }
                }
            }
            ColumnLayout {
                Label { text:"Seu ritmo"; font.pixelSize:28; font.bold:true }
                Label { text:"Artistas, músicas e estilos que marcaram seus dias"; color:"#a5a8ad" }
                RowLayout {
                    ComboBox { id: period; textRole: "text"; valueRole: "value"; model:[{text:"7 dias",value:7},{text:"30 dias",value:30},{text:"90 dias",value:90},{text:"180 dias",value:180}]; onActivated: panel.refresh() }
                    ComboBox { id: dimension; textRole: "text"; valueRole: "value"; model:[{text:"Bandas / artistas",value:"artists"},{text:"Músicas",value:"tracks"},{text:"Álbuns",value:"albums"},{text:"Estilos",value:"genres"},{text:"Por dia",value:"days"}]; onActivated: panel.refresh() }
                    CheckBox { text: "Registrar escutas locais"; checked: panel.controller.historyEnabled; onToggled: panel.controller.historyEnabled=checked }
                    Button { text: "Apagar histórico"; onClicked: clearDialog.open() }
                }
                Rectangle { Layout.fillWidth:true; implicitHeight:82; color:"#263a45"; radius:14
                    Column { anchors.fill:parent; anchors.margins:14; spacing:5
                        Label { text:((panel.stats.ms || 0)/60000).toFixed(0)+" minutos de música"; font.pixelSize:24; font.bold:true }
                        Label { text:(panel.stats.plays || 0)+" escutas qualificadas • "+(panel.stats.vibe || "Seu histórico começa aqui"); color:"#b8c6ce" }
                    }
                }
                Rectangle {Layout.fillWidth:true;implicitHeight:100;radius:14;color:"#303044"
                    Column {anchors.fill:parent;anchors.margins:14;spacing:5
                        Label{text:"CLIMA DO REPERTÓRIO • abrir análise";font.pixelSize:11;color:"#b5c9d6"}
                        Label{text:panel.stats.vibe || "Dados insuficientes";font.pixelSize:21;font.bold:true}
                        Label{text:panel.stats.explanation || "";width:parent.width;wrapMode:Text.Wrap;font.pixelSize:12}
                    }
                    MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:climate.open()}
                }
                ListView {
                    id: chart; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: panel.stats.rows || []; ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: chart.width; height: 54; color: "#1b2935"; radius: 5
                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: parent.width*modelData.ms/Math.max(1,...panel.stats.rows.map(r=>r.ms)); color: "#176078"; radius: 5 }
                        Label { anchors.fill: parent; anchors.margins: 12; text: modelData.label+" • "+(modelData.ms/60000).toFixed(1)+" min • "+modelData.plays+" escutas"; textFormat: Text.PlainText; elide: Text.ElideRight }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { drill.report=modelData;drill.open() } }
                    }
                    Label { anchors.centerIn: parent; visible: chart.count===0; text: "Seu histórico começa aqui\nOuça músicas para descobrir seus artistas e estilos mais ouvidos."; wrapMode: Text.WordWrap; width: parent.width-30; horizontalAlignment: Text.AlignHCenter; color:"#a5a8ad" }
                }
                Label { text: "Sugestões editoriais por estilo informado • toque para buscar"; color: "#b5c9d6" }
                Flow { Layout.fillWidth: true; spacing: 5; Repeater { model: panel.stats.recommendations || []; Button { required property string modelData; text: modelData; onClicked: {panel.discover(modelData);panel.close()} } } }
            }
            ColumnLayout {
                RowLayout {
                    Image { Layout.preferredWidth:110; Layout.preferredHeight:110; source:panel.controller.current.image || ""; fillMode:Image.PreserveAspectFit; asynchronous:true; sourceSize.width:220; sourceSize.height:220 }
                    ColumnLayout { Layout.fillWidth:true
                        Label { text:"TOCANDO AGORA"; color:"#a5a8ad"; font.pixelSize:11 }
                        Label { text:panel.controller.current.title || "Escolha uma música"; textFormat:Text.PlainText; font.pixelSize:26; font.bold:true; wrapMode:Text.Wrap; Layout.fillWidth:true }
                        Label { text:panel.controller.current.artist || ""; textFormat:Text.PlainText; color:"#c4c7cc" }
                        Label { text:(panel.controller.current.album || "")+" • "+(panel.controller.current.provider || ""); textFormat:Text.PlainText; color:"#a5a8ad"; Layout.fillWidth:true; elide:Text.ElideRight }
                    }
                }
                RowLayout {
                    ActionIcon { symbol:"prev";description:"Anterior"; enabled:!!panel.controller.current.title; onClicked:panel.previousRequested() }
                    ActionIcon { symbol:panel.playing?"pause":"play";description:panel.playing?"Pausar":"Reproduzir"; enabled:!!panel.controller.current.title; onClicked:panel.togglePlayback() }
                    ActionIcon { symbol:"next";description:"Próxima"; enabled:!!panel.controller.current.title; onClicked:panel.nextRequested() }
                    Slider { Layout.fillWidth:true; from:0; to:Math.max(1,panel.controller.current.durationMs || 0); value:panel.position; enabled:(panel.controller.current.durationMs || 0)>0; onMoved:panel.controller.seek(Math.round(value)) }
                }
                RowLayout {
                    visible:!!panel.mixController&&!!panel.transportPlayer&&panel.controller.current.provider==="Spotify"
                    ActionIcon{symbol:"shake";description:"Sortear outra ordem da fila";onClicked:panel.remixSong(true)}
                    ActionIcon{symbol:"random";description:"Reprodução aleatória / ordem da lista";filled:panel.randomPlayback;onClicked:{panel.randomPlayback=!panel.randomPlayback;panel.remixSong(false)}}
                    ActionIcon{symbol:"discover";description:"Descoberta 2+1 / desativar";filled:panel.discoveryPlayback;onClicked:{panel.discoveryPlayback=!panel.discoveryPlayback;panel.remixSong(false)}}
                    ActionIcon{objectName:"songQueueButton";symbol:"queue";description:"Ver fila de reprodução";onClicked:panel.openQueue()}
                }
                RowLayout {
                    ActionIcon { symbol:"refresh";description:"Atualizar letra"; enabled: !panel.controller.busy; onClicked: panel.controller.fetchLyrics() }
                    Label { text:"Idioma automático"; color:"#a5a8ad" }
                    ActionIcon { symbol:"translate";description:"Traduzir pt-BR"; enabled: !panel.controller.busy && panel.controller.lines.length>0; onClicked:{panel.translatedTrack="";panel.autoTranslation=true;panel.ensureLyrics()} }
                    ActionIcon { symbol:"font";description:"Fontes e cores";onClicked:appearance.open() }
                    ActionIcon { symbol:"expand";description:"Expandir letras em tela cheia";onClicked:fullscreen.present() }
                    Button { text: "Importar / editar"; onClicked: editDialog.open() }
                    CheckBox { text: "Sincronia"; checked:panel.controller.syncEnabled;onToggled:panel.controller.syncEnabled=checked }
                    Label{text:"Toque na linha que está ouvindo para alinhar";color:"#a5a8ad";font.pixelSize:12}
                    Button{text:"Exportar ajuste";onClicked:syncExport.open()}
                }
                Label { text: panel.controller.lyricStatus; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#b5c9d6" }
                Label { text: "Buscar envia título/artista ao LRCLIB; traduzir envia a letra ao Google Tradutor. Letra/versão e tradução podem não estar disponíveis."; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11; color: "#a3b8c8" }
                RowLayout {
                    ComboBox { model:stage.modes; Component.onCompleted: currentIndex=Math.max(0,model.indexOf(panel.controller.background)); onActivated: {panel.controller.background=currentText;panel.audioEngine.visualizerActive=panel.visible && tabs.currentIndex===2 && (currentText==="Ondas"||currentText==="Espectro");stage.requestPaint()} }
                    Button { text: "Escolher foto da banda"; onClicked: photoPicker.open() }
                    Label { text: "Ondas / espectro: PCM real antes do volume"; font.pixelSize: 11; color: "#a3b8c8" }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#070f18"; radius: 10; clip: true
                    Image { anchors.fill: parent; source: panel.controller.background==="Foto"?panel.controller.photo:panel.controller.background==="Capa"?(panel.controller.current.image || ""):""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 1400; sourceSize.height: 1000 }
                    LyricBackdrop {id:stage;anchors.fill:parent;mode:panel.controller.background;playing:panel.playing;active:panel.visible&&tabs.currentIndex===2&&!fullscreen.visible;metrics:panel.audioEngine ? panel.audioEngine.metrics : ({})}
                    Rectangle { anchors.fill: parent; color: "#99040a12" }
                    ListView {
                        id: lyrics; anchors.fill: parent; anchors.margins: 18; model: panel.controller.lines; clip: true; spacing: 20; ScrollBar.vertical: ScrollBar {}
                        header: Item { height: 24 }
                        footer: Item { height: 100 }
                        delegate: Column {
                            required property var modelData; required property int index
                            width: lyrics.width-18; spacing: 6
                            StyledVerse{width:parent.width;text:modelData.text;appearance:panel.controller.lyricStyle;playing:panel.playing&&panel.visible}
                            StyledVerse{width:parent.width;text:modelData.translation || "";visible:text.length>0;translation:true;appearance:panel.controller.lyricStyle;playing:panel.playing&&panel.visible}
                            MouseArea{anchors.fill:parent;onClicked:panel.controller.alignLyrics(index,panel.position)}

                        }
                    }
                }
            }
        }
    }
    Dialog{id:queueDialog;objectName:"songQueueDialog";title:panel.queueEdited?"Prévia da nova fila":"Fila de reprodução";modal:true;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(640,parent.width-32);height:Math.min(620,parent.height-48);standardButtons:Dialog.Close
        palette.window:"#141517";palette.windowText:"#f5f5f5";palette.text:"#f5f5f5";palette.button:"#263440";palette.buttonText:"#f5f5f5"
        background:Rectangle{color:"#171b24";radius:18;border.color:"#354052"}
        header:Label{text:queueDialog.title;color:"#f5f5f5";font.pixelSize:22;font.bold:true;padding:18}
        ColumnLayout{anchors.fill:parent
            Label{Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#c4c7cc";text:panel.queueEdited?"Confira a ordem e aplique para tocar. A faixa atual será retomada na posição atual.":"Agora e próximas músicas informadas pelo Spotify. A fila do serviço pode ser parcial."}
            Label{visible:!!panel.mixController;color:"#00b3ef";text:panel.mixController?panel.mixController.status:"";Layout.fillWidth:true;wrapMode:Text.Wrap}
            ListView{Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:panel.queuedRows;spacing:8;ScrollBar.vertical:ScrollBar{}
                delegate:Column{required property var modelData;required property int index;width:ListView.view.width;spacing:3
                    Label{width:parent.width;text:(index===0?"Agora • ":index+" • ")+(modelData.title||"");textFormat:Text.PlainText;color:modelData.mixExternal?"#00b3ef":"#f5f5f5";elide:Text.ElideRight;font.bold:true}
                    Label{width:parent.width;text:(modelData.artist||"")+(modelData.mixExternal?" • Descoberta":"");textFormat:Text.PlainText;color:"#a5a8ad";elide:Text.ElideRight}
                }
            }
            Button{text:"Aplicar e reproduzir fila";visible:panel.queueEdited;enabled:panel.mixController&&!panel.mixController.busy&&panel.queuedRows.length>0;onClicked:{if(panel.queueAnchor!==(panel.controller.current.uri||"")){panel.notice="A música mudou. Prepare a fila novamente.";queueDialog.close();return}panel.mixController.applyPreview(panel.position);queueDialog.close()}}
        }
    }
    FileDialog{id:syncExport;fileMode:FileDialog.SaveFile;nameFilters:["Ajuste Tonal (*.json)"];onAccepted:panel.controller.exportSyncFeedback(selectedFile)}
    StatisticsDetail{id:drill;controller:panel.controller}
    ClimateDetail{id:climate;controller:panel.controller;days:period.currentValue || 7}
    LyricStyleDialog{id:appearance;controller:panel.controller}
    LyricsFullscreen{id:fullscreen;controller:panel.controller;playing:panel.playing;position:panel.position;metrics:panel.audioEngine ? panel.audioEngine.metrics : ({})}
    Dialog { id: createDialog; anchors.centerIn: parent; title:"Criar playlist pessoal"; standardButtons: Dialog.Ok|Dialog.Cancel; TextField { id: newName; placeholderText:"Nome da playlist"; maximumLength:100 } onAccepted: {let id=panel.controller.createPlaylist(newName.text);if(id&&panel.controller.current.title)panel.controller.addToPlaylist(id,panel.controller.current);newName.clear()} }
    Dialog { id: renameDialog; anchors.centerIn: parent; title:"Renomear playlist"; standardButtons: Dialog.Ok|Dialog.Cancel; TextField { id: renameField; placeholderText:"Novo nome"; maximumLength:100 } onAccepted: panel.controller.renamePlaylist(panel.selectedPlaylist,renameField.text) }
    Dialog { id: deleteDialog; anchors.centerIn: parent; title:"Excluir esta playlist pessoal?"; standardButtons: Dialog.Yes|Dialog.No; onAccepted: {panel.controller.deletePlaylist(panel.selectedPlaylist);panel.selectedPlaylist="";panel.refresh()} }
    Dialog { id: clearDialog; anchors.centerIn: parent; title:"Apagar o histórico deste player?"; standardButtons: Dialog.Yes|Dialog.No; onAccepted: panel.controller.clearHistory() }
    Dialog {
        id: editDialog; anchors.centerIn: parent; title:"Dados e letra desta gravação"; width: Math.min(650,panel.width-40); height: Math.min(600,panel.height-40); standardButtons: Dialog.Close
        onOpened: {titleField.text=panel.controller.current.title || "";artistField.text=panel.controller.current.artist || "";genreField.text=panel.controller.current.genre || ""}
        ColumnLayout { anchors.fill: parent
            TextField { id:titleField; placeholderText:"Título"; Layout.fillWidth:true }
            TextField { id:artistField; placeholderText:"Banda / artista"; Layout.fillWidth:true }
            TextField { id:genreField; placeholderText:"Estilo (por exemplo grunge, punk, metal)"; Layout.fillWidth:true }
            Button { text:"Salvar dados"; onClicked: {panel.controller.editMetadata(titleField.text,artistField.text);panel.controller.setGenre(genreField.text)} }
            ScrollView { Layout.fillWidth:true;Layout.fillHeight:true; TextArea { id:lyricInput; placeholderText:"Cole letra TXT ou LRC; para tradução, use uma linha para cada linha da letra."; wrapMode:TextEdit.Wrap; textFormat:TextEdit.PlainText } }
            RowLayout { Button { text:"Usar como letra"; onClicked: {panel.controller.importLyrics(lyricInput.text,false);editDialog.close()} } Button { text:"Usar como tradução"; onClicked: {panel.controller.importLyrics(lyricInput.text,true);editDialog.close()} } }
        }
    }
    FileDialog { id:photoPicker; title:"Foto da banda"; nameFilters:["Imagens (*.jpg *.jpeg *.png *.webp)"]; onAccepted: panel.controller.photo=selectedFile.toString() }
    FileDialog { id:backupSave; title:"Salvar backup fora da pasta do aplicativo"; fileMode:FileDialog.SaveFile; defaultSuffix:"tonal-backup.json"; nameFilters:["Backup Tonal (*.json)"]; onAccepted: panel.controller.exportBackup(selectedFile) }
    Dialog { id:restoreConfirm; title:"Restaurar substitui a biblioteca e o histórico deste player. Faça um backup antes."; width:Math.min(550,panel.width-40); anchors.centerIn:parent; standardButtons:Dialog.Ok|Dialog.Cancel; onAccepted:backupOpen.open() }
    FileDialog { id:backupOpen; title:"Restaurar backup deste player"; nameFilters:["Backup Tonal (*.json)"]; onAccepted: panel.controller.restoreBackup(selectedFile) }
}
