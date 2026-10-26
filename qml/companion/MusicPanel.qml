import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models

Dialog {
    id: panel
    required property var controller
    required property var audioEngine
    property var mixController:null
    // Tema do player (App.theme.current no Tonalify). Sem tema, usa as cores originais.
    property var themeTokens:null
    readonly property var tone:themeTokens&&themeTokens.seek?themeTokens:({panel:"#141517",card:"#20252d",text:"#f5f5f5",textSecondary:"#a5a8ad",accent:"#009bd6",accentText:"#00b3ef",accentDeep:"#263440",divider:"#303237",seek:{height:4,activeHeight:6,track:"#404040",fill:["#009bd6","#009bd6"],activeFill:["#00b3ef","#00b3ef"],thumb:"#f5f5f5",thumbBorder:"#f5f5f5",thumbSize:14,thumbShape:"circle",thumbAlways:true,glow:false}})
    property var transportPlayer:null
    property var songQueueSource:[]
    property var queuedRows:[]
    property bool queueEdited:false
    property string queueAnchor:""
    property bool queueLoading:false
    property bool pendingRemix:false
    property bool pendingShake:false
    property string queueNotice:""
    function queueRows(){return transportPlayer ? transportPlayer.queue.map(t=>({provider:"Spotify",id:t.uri,uri:t.uri,title:t.name,artist:t.artists,artistId:t.artistId||"",album:t.album||"",albumId:t.albumId||"",year:t.year||0,image:t.coverUrl,durationMs:t.durationMs})) : []}
    function committedQueue(){return mixController&&typeof mixController.queueFor==="function"?mixController.queueFor(controller.current.uri||""):[]}
    function syncQueueModes(){if(mixController&&typeof mixController.discoveryEnabled!=="undefined"){discoveryPlayback=mixController.discoveryEnabled;randomPlayback=mixController.randomEnabled}}
    function changeSongMode(mode){if(!queueDialog.visible){syncQueueModes();queueEdited=false;songQueueSource=[];queuedRows=[]}if(mode===1)randomPlayback=!randomPlayback;if(mode===2)discoveryPlayback=!discoveryPlayback;remixSong(mode===0)}
    function openQueue(){syncQueueModes();pendingRemix=false;queueEdited=false;queueNotice="";queueAnchor=controller.current.uri || "";let committed=committedQueue();songQueueSource=committed.length?committed:[controller.current].concat(queueRows());queuedRows=songQueueSource;queueDialog.open();if(transportPlayer&&!committed.length){queueLoading=true;transportPlayer.refreshQueue()}}
    function remixSong(shake){
        if(!mixController)return
        mixController.cancel();queueNotice="";queueDialog.open()
        if(!queueEdited||!songQueueSource.length||queueAnchor!==(controller.current.uri || "")){
            queueAnchor=controller.current.uri || "";queueEdited=true;queuedRows=[]
            let committed=committedQueue();if(committed.length){songQueueSource=committed;prepareSongMix(shake);return}
            if(transportPlayer){pendingRemix=true;pendingShake=shake;queueLoading=true;transportPlayer.refreshQueue();return}
            songQueueSource=[controller.current]
        }
        prepareSongMix(shake)
    }
    readonly property bool quickModes:!!mixController&&!!transportPlayer&&controller.current.provider==="Spotify"
    function formatClock(ms){const s=Math.max(0,Math.floor((ms||0)/1000));return Math.floor(s/60)+":"+(s%60<10?"0":"")+(s%60)}
    // Botões ao lado da barra: aplicam o modo direto na fila atual, a partir da
    // música que toca. A fila vem do mix aplicado ou do Spotify.
    property var pendingQuick:null
    function applySongMode(mode){
        if(!quickModes)return
        let random=mixController.randomEnabled,discovery=mixController.discoveryEnabled
        if(mode===1)random=!random
        if(mode===2)discovery=!discovery
        notice=""
        const committed=committedQueue()
        if(committed.length>1){mixController.applyModes(committed,mode===0,random,discovery);return}
        pendingQuick={shake:mode===0,random:random,discovery:discovery,anchor:controller.current.uri||""}
        transportPlayer.refreshQueue()
    }
    function prepareSongMix(shake){
        let source=(queuedRows.length?queuedRows:songQueueSource).slice();if(shake&&mixController&&source.length>2){let rest=source.slice(1);source=[source[0]].concat(mixController.order(rest.length,Math.floor(Math.random()*2147483647)).map(i=>rest[i]))}
        queueEdited=true;mixController.preview(source,0,randomPlayback,discoveryPlayback,false)
    }
    Connections{target:panel.transportPlayer;ignoreUnknownSignals:true;function onQueueRefreshFinished(status){
        if(panel.pendingQuick){
            const m=panel.pendingQuick;panel.pendingQuick=null
            if(m.anchor!==(panel.controller.current.uri||"")){panel.notice="A música mudou. Use o modo de novo.";return}
            if(status!==200){panel.notice="Não foi possível carregar a fila (Spotify HTTP "+status+").";return}
            const rows=[panel.controller.current].concat(panel.queueRows())
            if(rows.length<2){panel.notice="A fila só tem a música atual. Abra um álbum ou playlist e toque a seleção.";return}
            panel.mixController.applyModes(rows,m.shake,m.random,m.discovery);return
        }
        if(!panel.queueLoading)return
        panel.queueLoading=false
        if(!queueDialog.visible||panel.queueAnchor!==(panel.controller.current.uri||"")){panel.pendingRemix=false;panel.queueNotice="A música mudou. Prepare a fila novamente.";return}
        if(status!==200){panel.pendingRemix=false;panel.queueNotice="Não foi possível carregar a fila (Spotify HTTP "+status+"). Tente novamente.";panel.queueEdited=false;return}
        panel.songQueueSource=[panel.controller.current].concat(panel.queueRows())
        if(panel.pendingRemix){panel.pendingRemix=false;panel.prepareSongMix(panel.pendingShake)}else panel.queuedRows=panel.songQueueSource
    }}
    onClosed: {pendingRemix=false;queueLoading=false;if(typeof controller.cancelArtistNotes==="function")controller.cancelArtistNotes()}
    Connections{target:panel.controller;function onCurrentChanged(){if(panel.queueLoading&&panel.queueAnchor!==(panel.controller.current.uri||"")){panel.pendingRemix=false;panel.queueLoading=false;panel.queueNotice="A música mudou. Prepare a fila novamente."}}}
    Connections{target:panel.mixController;ignoreUnknownSignals:true;function onPreviewReady(){if(panel.queueEdited&&!panel.mixController.busy)panel.queuedRows=panel.mixController.previewQueue}}

    property bool listShaken:false
    property bool randomPlayback:false
    property bool discoveryPlayback:false
    property int mixSeed:Math.floor(Math.random()*2147483647)
    property bool textPlaylistAvailable:false
    signal textPlaylistRequested()
    signal homeRequested()
    signal jamRequested()
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
    function showBackup(){backupMenu.open()}
    function showSong(){tabs.currentIndex=2;songPages.positionViewAtIndex(0,ListView.Beginning);open();ensureLyrics()}
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
    palette.window:tone.panel;palette.windowText:tone.text;palette.text:tone.text;palette.base:tone.card;palette.button:tone.accentDeep;palette.buttonText:tone.text;palette.highlight:tone.accent
    standardButtons: Dialog.NoButton
    background: Rectangle { color: panel.tone.panel; radius: 18; border.color: panel.tone.divider }
    function playSelection(index){if(mixController)mixController.play(displayTracks,index,randomPlayback,discoveryPlayback,false);else controller.play(displayTracks,index)}
    function refresh() {
        revision++
        if(typeof controller.requestStatistics==="function"){controller.requestStatistics(period.currentIndex<0?7:period.currentValue,dimension.currentValue || "artists");stats=controller.statisticsReport}
        else stats=controller.statistics(period.currentIndex<0?7:period.currentValue,dimension.currentValue || "artists")
        displayTracks=drillTracks!==null ? drillTracks : selectedPlaylist ? controller.playlistTracks(selectedPlaylist) : controller.favorites
        if(listShaken&&mixController){let original=displayTracks;displayTracks=mixController.order(original.length,mixSeed).map(i=>original[i])}
    }
    onOpened: {syncQueueModes();refresh();controller.refreshMetadata();ensureLyrics()}
    onPositionChanged: {
        const found=controller.lineAt(position)
        highlighted=found
        if(followLyrics && found>=0) lyrics.positionViewAtIndex(found,ListView.Center)
    }
    Connections {
        target: panel.controller
        function onStatisticsChanged(){if(panel.visible)panel.stats=panel.controller.statisticsReport}
        function onChanged(){if(panel.visible){panel.refresh();if(panel.audioEngine)panel.audioEngine.visualizerActive=tabs.currentIndex===2&&songPages.currentIndex===1&&(panel.controller.background==="Ondas"||panel.controller.background==="Espectro")}}
        function onLyricsChanged(){lyrics.model=panel.controller.lines;Qt.callLater(panel.ensureLyrics)}
        function onCurrentChanged(){Qt.callLater(panel.ensureLyrics);if(panel.visible&&songPages.currentIndex===2&&typeof panel.controller.loadArtistNotes==="function")panel.controller.loadArtistNotes()}
        function onMessage(text){panel.notice=text}
    }
    onVisibleChanged: if(audioEngine) audioEngine.visualizerActive=visible && tabs.currentIndex===2 && songPages.currentIndex===1 && (controller.background==="Ondas" || controller.background==="Espectro")
    Rectangle {
        objectName:"songPanelBody";anchors.fill:parent;color:panel.tone.panel
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: panel.controller.current.title || "Escolha uma música no player"; textFormat: Text.PlainText; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
            ActionIcon { accentInk:panel.tone.accent;symbol:"heart";filled:{panel.revision;return panel.controller.liked(panel.controller.current)} description:filled?"Remover das favoritas":"Favoritar música"; enabled: !!panel.controller.current.title; onClicked: panel.controller.favorite(panel.controller.current) }
            ActionIcon { accentInk:panel.tone.accent;symbol:"plus";description:"Adicionar à playlist"; enabled: !!panel.controller.current.title; onClicked: addMenu.open() }
            ActionIcon {accentInk:panel.tone.accent;symbol:"share";description:"Compartilhar música";visible:panel.controller.current.provider==="Spotify";onClicked:shareSong.present(panel.controller.current)}

            ActionIcon {accentInk:panel.tone.accent;symbol:tabs.currentIndex===2?"home":"close";description:tabs.currentIndex===2?"Ir para o início":"Fechar";onClicked:{if(tabs.currentIndex===2)panel.homeRequested();panel.close()}}
            Menu { id: backupMenu; MenuItem { text:"Exportar backup…"; onTriggered: backupSave.open() } MenuItem { text:"Restaurar backup…"; onTriggered: restoreConfirm.open() } }
            Menu { id: addMenu; Repeater { model: panel.controller.playlists; MenuItem { required property var modelData; text: modelData.name; onTriggered: panel.controller.addToPlaylist(modelData.id,panel.controller.current) } } MenuItem { text: "Criar playlist…"; onTriggered: createDialog.open() } MenuItem {text:"Criar com Text to Tonalify…";visible:panel.textPlaylistAvailable;onTriggered:panel.textPlaylistRequested()} }
        }
        Label { visible: panel.notice.length>0; text: panel.notice; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#ffcc8d"; Layout.fillWidth: true }
        TabBar {
            id: tabs; Layout.fillWidth: true
            visible:currentIndex!==2
            onCurrentIndexChanged: {if(panel.audioEngine)panel.audioEngine.visualizerActive=panel.visible && currentIndex===2 && songPages.currentIndex===1 && (panel.controller.background==="Ondas" || panel.controller.background==="Espectro");Qt.callLater(panel.ensureLyrics)}
            TabButton { Accessible.name:"Biblioteca";contentItem:ActionIcon{accentInk:panel.tone.accent;symbol:"library";description:"Biblioteca";filled:tabs.currentIndex===0;onClicked:tabs.currentIndex=0} }
            TabButton { visible:false;width:0;implicitWidth:0;Accessible.name:"Estatísticas";contentItem:ActionIcon{accentInk:panel.tone.accent;symbol:"chart";description:"Estatísticas";filled:tabs.currentIndex===1;onClicked:tabs.currentIndex=1} }
            TabButton { Accessible.name:"Letras";contentItem:ActionIcon{accentInk:panel.tone.accent;symbol:"lyrics";description:"Letras";filled:tabs.currentIndex===2;onClicked:{tabs.currentIndex=2;songPages.positionViewAtIndex(1,ListView.Beginning)}} }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: tabs.currentIndex
            ColumnLayout {
                RowLayout {visible:!!panel.mixController
                    ActionIcon{accentInk:panel.tone.accent;symbol:"shake";description:"Shake: sortear ordem; segure para restaurar";filled:panel.listShaken;onClicked:{panel.mixController.cancel();panel.listShaken=true;panel.mixSeed=Math.floor(Math.random()*2147483647);panel.refresh()}onPressAndHold:{panel.listShaken=false;panel.refresh()}}
                    ActionIcon{accentInk:panel.tone.accent;symbol:"random";description:"Reprodução aleatória";filled:panel.randomPlayback;onClicked:{panel.mixController.cancel();panel.randomPlayback=!panel.randomPlayback}}
                    ActionIcon{accentInk:panel.tone.accent;symbol:"discover";description:"Descoberta 2+1";filled:panel.discoveryPlayback;onClicked:{panel.mixController.cancel();panel.discoveryPlayback=!panel.discoveryPlayback}}
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
                Label { visible:!!panel.stats.historyError;Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#ffce76";text:"Não foi possível consultar todo o histórico. Os dados podem estar incompletos." }
                Label { text:"Seu ritmo"; font.pixelSize:28; font.bold:true }
                Label { text:"Artistas, músicas e estilos que marcaram seus dias"; color:"#a5a8ad" }
                RowLayout {
                    ComboBox { id: period;objectName:"statisticsPeriod"; textRole: "text"; valueRole: "value"; model:[{text:"1 semana",value:7},{text:"1 mês",value:30},{text:"3 meses",value:90},{text:"6 meses",value:180},{text:"1 ano",value:365},{text:"Desde o começo",value:0}]; onActivated: panel.refresh() }
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
                ListView {
                    id:songPages;objectName:"songPages";Layout.fillWidth:true;Layout.fillHeight:true;orientation:ListView.Vertical
                    clip:true;snapMode:ListView.SnapOneItem;boundsBehavior:Flickable.StopAtBounds;highlightRangeMode:ListView.StrictlyEnforceRange
                    preferredHighlightBegin:0;preferredHighlightEnd:0;highlightMoveDuration:260;maximumFlickVelocity:1500
                    onCurrentIndexChanged:{if(currentIndex===2&&typeof panel.controller.loadArtistNotes==="function")panel.controller.loadArtistNotes();if(panel.audioEngine)panel.audioEngine.visualizerActive=panel.visible&&currentIndex===1&&(panel.controller.background==="Ondas"||panel.controller.background==="Espectro")}
                    model:ObjectModel{
                        ColumnLayout {width:songPages.width;height:songPages.height;spacing:8
                Item { Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:60
                    Rectangle{anchors.centerIn:parent;width:Math.min(parent.width,parent.height);height:width;radius:16;color:panel.tone.card
                        Rectangle{anchors.centerIn:parent;width:parent.width*.56;height:width;radius:width/2;color:panel.tone.accentDeep;border.color:panel.tone.accent
                            Rectangle{anchors.centerIn:parent;width:parent.width*.18;height:width;radius:width/2;color:panel.tone.panel}}
                        Label{anchors.bottom:parent.bottom;anchors.bottomMargin:18;anchors.horizontalCenter:parent.horizontalCenter;text:"Capa indisponível";color:panel.tone.textSecondary}
                        visible:songArt.status!==Image.Ready
                    }
                    Image {id:songArt;anchors.fill:parent;source:panel.controller.current.image||"";fillMode:Image.PreserveAspectFit;asynchronous:true;sourceSize.width:800;sourceSize.height:800}
                }
                Label {text:panel.controller.current.title||"Escolha uma música";textFormat:Text.PlainText;font.pixelSize:28;font.bold:true;Layout.fillWidth:true;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight}
                Label {text:(panel.controller.current.artist||"")+" · "+(panel.controller.current.album||"");textFormat:Text.PlainText;color:panel.tone.textSecondary;Layout.fillWidth:true;elide:Text.ElideRight}
                // Todos os controles juntos, perto da barra: Shake, Aleatório, anterior,
                // tocar, próxima, Repetir, Descoberta 2+1 e Fila. Os modos aplicam direto;
                // a janela Fila guarda a prévia com Aplicar.
                RowLayout {
                    objectName:"songControls"
                    Layout.alignment:Qt.AlignHCenter
                    spacing:4
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songShake";visible:panel.quickModes;enabled:!panel.mixController||!panel.mixController.busy;symbol:"shake";description:"Shake: sortear outra ordem da fila agora";onClicked:panel.applySongMode(0)}
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songRandom";visible:panel.quickModes;enabled:!panel.mixController||!panel.mixController.busy;symbol:"random";description:"Aleatório: tocar a fila em ordem aleatória / voltar à ordem";filled:!!panel.mixController&&panel.mixController.randomEnabled;onClicked:panel.applySongMode(1)}
                    ActionIcon { accentInk:panel.tone.accent;symbol:"prev";description:"Anterior"; enabled:!!panel.controller.current.title; onClicked:panel.previousRequested() }
                    ActionIcon { implicitWidth:64;implicitHeight:64;background:Rectangle{radius:32;color:panel.tone.accent}accentInk:panel.tone.accent;symbol:panel.playing?"pause":"play";description:panel.playing?"Pausar":"Reproduzir"; enabled:!!panel.controller.current.title; onClicked:panel.togglePlayback() }
                    ActionIcon { accentInk:panel.tone.accent;symbol:"next";description:"Próxima"; enabled:!!panel.controller.current.title; onClicked:panel.nextRequested() }
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songRepeat";visible:!!panel.transportPlayer&&typeof panel.transportPlayer.cycleRepeat==="function";symbol:panel.transportPlayer&&panel.transportPlayer.repeatMode==="track"?"repeat1":"repeat";filled:!!panel.transportPlayer&&panel.transportPlayer.repeatMode!=="off";description:!panel.transportPlayer||panel.transportPlayer.repeatMode==="off"?"Repetir a fila":panel.transportPlayer.repeatMode==="context"?"Repetindo a fila: repetir só esta música":"Repetindo esta música: desligar repetição";onClicked:panel.transportPlayer.cycleRepeat()}
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songDiscovery";visible:panel.quickModes;enabled:!panel.mixController||!panel.mixController.busy;symbol:"discover";description:panel.mixController&&panel.mixController.busy?panel.mixController.status:"Descoberta 2+1: duas da fila, uma descoberta / desligar";filled:!!panel.mixController&&panel.mixController.discoveryEnabled;onClicked:panel.applySongMode(2)}
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songQueueButton";visible:panel.quickModes;symbol:"queue";description:"Fila: ver, preparar com prévia e aplicar";onClicked:panel.openQueue()}
                    ActionIcon{accentInk:panel.tone.accent;objectName:"songJamButton";visible:panel.quickModes;symbol:"jam";description:"Jam com amigos";onClicked:panel.jamRequested()}
                }
                RowLayout {
                    Label { text:panel.formatClock(songSeek.pressed?songSeek.value:panel.position); color:panel.tone.textSecondary; font.pixelSize:12; font.features:({"tnum":1}) }
                    Slider { id:songSeek; objectName:"songSeek"; Layout.fillWidth:true; from:0; to:Math.max(1,panel.controller.current.durationMs || 0); enabled:(panel.controller.current.durationMs || 0)>0
                        property real dragTarget:-1
                        readonly property var look:panel.tone.seek
                        readonly property bool active:hovered||pressed
                        Binding on value { value:panel.position; when:!songSeek.pressed&&songSeek.dragTarget<0; restoreMode:Binding.RestoreNone }
                        onMoved:{if(pressed)dragTarget=value;else panel.controller.seek(Math.round(value))}
                        onPressedChanged:if(!pressed&&dragTarget>=0){panel.controller.seek(Math.round(dragTarget));dragTarget=-1}
                        background:Rectangle{x:songSeek.leftPadding;y:songSeek.topPadding+songSeek.availableHeight/2-height/2;width:songSeek.availableWidth;height:songSeek.active?songSeek.look.activeHeight:songSeek.look.height;radius:height/2;color:songSeek.look.track
                            Rectangle{width:Math.max(parent.height,songSeek.visualPosition*parent.width);height:parent.height;radius:height/2;gradient:Gradient{orientation:Gradient.Horizontal;GradientStop{position:0;color:(songSeek.active?songSeek.look.activeFill:songSeek.look.fill)[0]}GradientStop{position:1;color:(songSeek.active?songSeek.look.activeFill:songSeek.look.fill)[1]}}}}
                        handle:Item{x:songSeek.leftPadding+songSeek.visualPosition*songSeek.availableWidth-width/2;y:songSeek.topPadding+songSeek.availableHeight/2-height/2;readonly property int size:songSeek.look.thumbSize+(songSeek.pressed?2:0);width:songSeek.look.thumbShape==="pill"?Math.round(size*1.6):size;height:songSeek.look.thumbShape==="pill"?Math.round(size*0.8):size;visible:songSeek.enabled&&(songSeek.look.thumbAlways||songSeek.active)
                            Rectangle{visible:songSeek.look.glow;anchors.centerIn:parent;width:parent.width+10;height:parent.height+10;radius:height/2;color:songSeek.look.thumbBorder;opacity:songSeek.pressed?0.45:0.28}
                            Rectangle{anchors.centerIn:parent;width:songSeek.look.thumbShape==="diamond"?Math.round(parent.width*0.75):parent.width;height:songSeek.look.thumbShape==="diamond"?Math.round(parent.height*0.75):parent.height;rotation:songSeek.look.thumbShape==="diamond"?45:0;radius:songSeek.look.thumbShape==="diamond"?2:height/2;color:songSeek.look.thumb;border.width:songSeek.look.thumbShape==="ring"?3:(songSeek.look.thumbBorder!==songSeek.look.thumb?2:0);border.color:songSeek.look.thumbBorder}}
                    }
                    Label { text:panel.formatClock(panel.controller.current.durationMs||0); color:panel.tone.textSecondary; font.pixelSize:12; font.features:({"tnum":1}) }
                }

                        }
                        ColumnLayout {width:songPages.width;height:songPages.height;spacing:8
                RowLayout {
                    ActionIcon { accentInk:panel.tone.accent;symbol:"refresh";description:"Atualizar letra"; enabled: !panel.controller.busy; onClicked:{panel.translatedTrack="";if(typeof panel.controller.reloadLyrics==="function")panel.controller.reloadLyrics();else panel.controller.fetchLyrics()} }
                    Label { text:"Idioma automático"; color:"#a5a8ad" }
                    ActionIcon { accentInk:panel.tone.accent;symbol:"translate";description:"Traduzir pt-BR"; enabled: !panel.controller.busy && panel.controller.lines.length>0; onClicked:{panel.translatedTrack="";panel.autoTranslation=true;panel.ensureLyrics()} }
                    ActionIcon { accentInk:panel.tone.accent;symbol:"font";description:"Fontes e cores";onClicked:appearance.open() }
                    ActionIcon { accentInk:panel.tone.accent;symbol:"expand";description:"Expandir letras em tela cheia";onClicked:fullscreen.present() }
                    ActionIcon {symbol:"plus";description:"Importar / editar letra";onClicked:editDialog.open()}
                    CheckBox { text: "Sincronia"; checked:panel.controller.syncEnabled;onToggled:panel.controller.syncEnabled=checked }

                    ActionIcon{symbol:"save";description:"Exportar ajuste de sincronia";onClicked:syncExport.open()}
                }
                Label { text: panel.controller.lyricStatus; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#b5c9d6" }
                Label { text: "Buscar envia título/artista ao LRCLIB; traduzir envia a letra ao Google Tradutor. Letra/versão e tradução podem não estar disponíveis."; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11; color: "#a3b8c8" }
                RowLayout {
                    ComboBox { model:stage.modes; Component.onCompleted: currentIndex=Math.max(0,model.indexOf(panel.controller.background)); onActivated: {panel.controller.background=currentText;panel.audioEngine.visualizerActive=panel.visible && tabs.currentIndex===2 && songPages.currentIndex===1 && (currentText==="Ondas"||currentText==="Espectro");stage.requestPaint()} }
                    Button { text: "Escolher foto da banda"; onClicked: photoPicker.open() }
                    Label { visible:songPages.width>800;text: "Ondas / espectro: PCM real antes do volume"; font.pixelSize: 11; color: "#a3b8c8" }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#070f18"; radius: 10; clip: true
                    Image { anchors.fill: parent; source: panel.controller.background==="Foto"?panel.controller.photo:panel.controller.background==="Capa"?(panel.controller.current.image || ""):""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 1400; sourceSize.height: 1000 }
                    LyricBackdrop {id:stage;anchors.fill:parent;mode:panel.controller.background;playing:panel.playing;active:panel.visible&&tabs.currentIndex===2&&songPages.currentIndex===1&&!fullscreen.visible;metrics:panel.audioEngine ? panel.audioEngine.metrics : ({})}
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
                        ScrollView {width:songPages.width;height:songPages.height;clip:true;contentWidth:availableWidth
                            ColumnLayout {width:songPages.width-24;spacing:20
                                Label {text:panel.controller.current.artist||"Sobre o artista";textFormat:Text.PlainText;font.pixelSize:32;font.bold:true;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                Label {text:"Bastidores do artista";font.pixelSize:22}
                                Label {text:(panel.controller.artistNotes||{}).message||"Curiosidades e trajetória · MusicBrainz";Layout.fillWidth:true;wrapMode:Text.Wrap;color:panel.tone.textSecondary}
                                Repeater{model:(panel.controller.artistNotes||{}).facts||[];Label{required property string modelData;text:modelData;textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:18}}
                                Button{visible:!!(panel.controller.artistNotes||{}).url;text:"Fonte: MusicBrainz · dados colaborativos";onClicked:Qt.openUrlExternally(panel.controller.artistNotes.url)}
                                Label{text:"Continue a descoberta";font.pixelSize:22;font.bold:true}
                                Label{text:"Sugestões editoriais por estilo; não são uma lista oficial de artistas relacionados.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:panel.tone.textSecondary}
                                Flow{Layout.fillWidth:true;spacing:8;Repeater{model:(panel.controller.artistNotes||{}).related||[];Button{required property string modelData;text:modelData;onClicked:{panel.discover(modelData);panel.close()}}}}
                                Button{text:"Mais deste artista";onClicked:{panel.discover(panel.controller.current.artist||"");panel.close()}}
                            }
                        }
                    }
                }

            }
        }
    }
    }
    Dialog{id:queueDialog;objectName:"songQueueDialog";title:panel.queueEdited?"Prévia da nova fila":"Fila de reprodução";modal:true;parent:Overlay.overlay;anchors.centerIn:parent;width:Math.min(640,parent.width-32);height:Math.min(620,parent.height-48);standardButtons:Dialog.Close
        onClosed:{panel.pendingRemix=false;panel.queueLoading=false;panel.queueEdited=false;panel.songQueueSource=[]}
        palette.window:panel.tone.panel;palette.windowText:panel.tone.text;palette.text:panel.tone.text;palette.button:panel.tone.accentDeep;palette.buttonText:panel.tone.text
        background:Rectangle{color:panel.tone.panel;radius:18;border.color:panel.tone.divider}
        header:Label{text:queueDialog.title;color:"#f5f5f5";font.pixelSize:22;font.bold:true;padding:18}
        ColumnLayout{anchors.fill:parent
            RowLayout{visible:panel.quickModes
                Label{text:"Prévia:";color:panel.tone.textSecondary}
                ActionIcon{accentInk:panel.tone.accent;objectName:"previewShake";symbol:"shake";description:"Prévia com outra ordem";onClicked:panel.changeSongMode(0)}
                ActionIcon{accentInk:panel.tone.accent;objectName:"previewRandom";symbol:"random";description:"Prévia com reprodução aleatória / ordem da lista";filled:panel.randomPlayback;onClicked:panel.changeSongMode(1)}
                ActionIcon{accentInk:panel.tone.accent;objectName:"previewDiscovery";symbol:"discover";description:"Prévia com Descoberta 2+1 / desativar";filled:panel.discoveryPlayback;onClicked:panel.changeSongMode(2)}
            }
            Label{Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#c4c7cc";text:panel.queueEdited?"Confira a ordem e aplique para tocar. A faixa atual será retomada na posição atual.":"Agora e próximas músicas informadas pelo Spotify. A fila do serviço pode ser parcial."}
            Label{visible:!!panel.mixController;color:panel.tone.accentText;text:panel.queueLoading?"Carregando a fila atual do Spotify…":panel.queueNotice|| (panel.mixController?panel.mixController.status:"");Layout.fillWidth:true;wrapMode:Text.Wrap}
            CheckBox{text:"Analisar letras das candidatas em segundo plano";visible:panel.mixController&&typeof panel.mixController.enrichmentEnabled!=="undefined";checked:panel.mixController?panel.mixController.enrichmentEnabled:false;onToggled:if(panel.mixController)panel.mixController.enrichmentEnabled=checked}
            Label{text:"Envia título, artista, álbum e duração ao LRCLIB; guarda apenas os temas. A fila confirmada permanece igual.";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#a5a8ad";font.pixelSize:11}
            Label{text:panel.mixController&&typeof panel.mixController.enrichmentStatus!=="undefined"?panel.mixController.enrichmentStatus:"";Layout.fillWidth:true;wrapMode:Text.Wrap;color:"#9bcfe1"}
            ListView{Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:panel.queuedRows;spacing:8;ScrollBar.vertical:ScrollBar{}
                delegate:Column{required property var modelData;required property int index;width:ListView.view.width;spacing:3
                    Label{width:parent.width;text:(index===0?"Agora • ":index+" • ")+(modelData.title||"");textFormat:Text.PlainText;color:modelData.mixExternal?panel.tone.accentText:panel.tone.text;elide:Text.ElideRight;font.bold:true}
                    Label{width:parent.width;text:(modelData.artist||"")+(modelData.mixExternal?" • Descoberta":"");textFormat:Text.PlainText;color:"#a5a8ad";elide:Text.ElideRight}
                }
            }
            Button{text:"Aplicar e reproduzir fila";visible:panel.queueEdited;enabled:!panel.queueLoading&&!panel.pendingRemix&&!panel.queueNotice&&panel.mixController&&!panel.mixController.busy&&panel.queuedRows.length>0;onClicked:{if(panel.queueAnchor!==(panel.controller.current.uri||"")){panel.notice="A música mudou. Prepare a fila novamente.";queueDialog.close();return}panel.mixController.applyPreview(panel.transportPlayer&&typeof panel.transportPlayer.positionMs==="number"?panel.transportPlayer.positionMs:panel.position);queueDialog.close()}}
        }
    }
    FileDialog{id:syncExport;fileMode:FileDialog.SaveFile;nameFilters:["Ajuste Tonal (*.json)"];onAccepted:panel.controller.exportSyncFeedback(selectedFile)}
    StatisticsDetail{id:drill;controller:panel.controller}
    footer:Label {padding:8;font.pixelSize:11;color:panel.tone.textSecondary;wrapMode:Text.WordWrap;text:{panel.revision;const info=panel.controller.storageInfo || ({});const kb=n=>n>=1048576?(n/1048576).toFixed(2)+" MiB":(n/1024).toFixed(1)+" KiB";return "Biblioteca no aparelho: "+kb(info.databaseBytes || 0)+(info.lastBackupBytes?" • Último backup exportado: "+kb(info.lastBackupBytes):" • Nenhum backup exportado neste aparelho")}}
    ClimateDetail{id:climate;controller:panel.controller;days:period.currentIndex<0?7:period.currentValue}
    LyricStyleDialog{id:appearance;controller:panel.controller}
    LyricsFullscreen{id:fullscreen;onRefreshRequested:panel.translatedTrack="";controller:panel.controller;playing:panel.playing;position:panel.position;metrics:panel.audioEngine ? panel.audioEngine.metrics : ({})}
    SongShareDialog{id:shareSong;objectName:"songShareDialog"}
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
    FileDialog { id:backupSave; title:"Salvar backup fora da pasta do aplicativo"; fileMode:FileDialog.SaveFile; defaultSuffix:"tonal-backup.json"; nameFilters:["Backup Tonal (*.json)"]; onAccepted: (typeof panel.controller.exportFullBackup==="function"?panel.controller.exportFullBackup(selectedFile):panel.controller.exportBackup(selectedFile)) }
    Dialog { id:restoreConfirm; title:"Restaurar substitui a biblioteca e o histórico deste player. Faça um backup antes."; width:Math.min(550,panel.width-40); anchors.centerIn:parent; standardButtons:Dialog.Ok|Dialog.Cancel; onAccepted:backupOpen.open() }
    FileDialog { id:backupOpen; title:"Restaurar backup deste player"; nameFilters:["Backup Tonal (*.json)"]; onAccepted: (typeof panel.controller.restoreFullBackup==="function"?panel.controller.restoreFullBackup(selectedFile):panel.controller.restoreBackup(selectedFile)) }
}
