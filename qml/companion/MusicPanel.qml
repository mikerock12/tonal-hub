import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: panel
    required property var controller
    required property var audioEngine
    property int position: 0
    property bool playing: false
    property int revision: 0
    property string selectedPlaylist: ""
    property var displayTracks: []
    property var drillTracks: null
    property var stats: ({rows:[],recommendations:[]})
    property string notice: ""
    property bool followLyrics: true
    property int highlighted: -1
    property real phase: 0
    signal discover(string artist)
    parent: Overlay.overlay
    width: Math.min(1120,parent ? parent.width-32 : 1000)
    height: Math.min(800,parent ? parent.height-32 : 700)
    anchors.centerIn: parent
    modal: true
    title: "Sua música • biblioteca, estatísticas e letras"
    standardButtons: Dialog.Close
    background: Rectangle { color: "#121b24"; radius: 14; border.color: "#385065" }
    function refresh() {
        revision++
        stats=controller.statistics(period.currentValue || 7,dimension.currentValue || "artists")
        displayTracks=drillTracks!==null ? drillTracks : selectedPlaylist ? controller.playlistTracks(selectedPlaylist) : controller.favorites
    }
    onOpened: refresh()
    onPositionChanged: {
        let found=-1
        for(let i=0;i<controller.lines.length;i++) if(controller.lines[i].time>=0 && controller.lines[i].time<=position) found=i
        highlighted=found
        if(followLyrics && found>=0) lyrics.positionViewAtIndex(found,ListView.Center)
    }
    Connections {
        target: panel.controller
        function onChanged(){if(panel.visible)panel.refresh()}
        function onLyricsChanged(){lyrics.model=panel.controller.lines}
        function onMessage(text){panel.notice=text}
    }
    Timer {
        interval: 50; repeat: true
        running: panel.visible && tabs.currentIndex===2 && panel.playing && ["Aurora","Ondas","Espectro"].indexOf(panel.controller.background)>=0
        onTriggered:{panel.phase+=0.045;stage.requestPaint()}
    }
    onVisibleChanged: if(audioEngine) audioEngine.visualizerActive=visible && tabs.currentIndex===2 && (controller.background==="Ondas" || controller.background==="Espectro")
    ColumnLayout {
        anchors.fill: parent; spacing: 10
        RowLayout {
            Layout.fillWidth: true
            Label { text: panel.controller.current.title || "Escolha uma música no player"; textFormat: Text.PlainText; font.pixelSize: 20; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
            Button { text: {panel.revision;return panel.controller.liked(panel.controller.current)?"♥ Favorita":"♡ Favoritar"} enabled: !!panel.controller.current.title; onClicked: panel.controller.favorite(panel.controller.current) }
            Button { text: "+ Playlist"; enabled: !!panel.controller.current.title; onClicked: addMenu.open() }
            Button { text: "Backup"; onClicked: backupMenu.open() }
            Menu { id: backupMenu; MenuItem { text:"Exportar backup…"; onTriggered: backupSave.open() } MenuItem { text:"Restaurar backup…"; onTriggered: restoreConfirm.open() } }
            Menu { id: addMenu; Repeater { model: panel.controller.playlists; MenuItem { required property var modelData; text: modelData.name; onTriggered: panel.controller.addToPlaylist(modelData.id,panel.controller.current) } } MenuItem { text: "Criar playlist…"; onTriggered: createDialog.open() } }
        }
        Label { visible: panel.notice.length>0; text: panel.notice; textFormat: Text.PlainText; wrapMode: Text.WordWrap; color: "#ffcc8d"; Layout.fillWidth: true }
        TabBar {
            id: tabs; Layout.fillWidth: true
            onCurrentIndexChanged: if(panel.audioEngine)panel.audioEngine.visualizerActive=panel.visible && currentIndex===2 && (panel.controller.background==="Ondas" || panel.controller.background==="Espectro")
            TabButton { text: "Favoritas e playlists" }
            TabButton { text: "Estatísticas" }
            TabButton { text: "Letras e cenário" }
        }
        StackLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; currentIndex: tabs.currentIndex
            ColumnLayout {
                RowLayout {
                    ComboBox { Layout.fillWidth: true; textRole: "name"; valueRole: "id"; model: [{name:"Músicas favoritas",id:""}].concat(panel.controller.playlists); onActivated: {panel.drillTracks=null;panel.selectedPlaylist=currentValue;panel.refresh()} }
                    Button { text: "Criar"; onClicked: createDialog.open() }
                    Button { text: "Renomear"; enabled: !!panel.selectedPlaylist; onClicked: renameDialog.open() }
                    Button { text: "Excluir playlist"; enabled: !!panel.selectedPlaylist; onClicked: deleteDialog.open() }
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
                            Button { text: "▶"; onClicked: panel.controller.play(panel.displayTracks,index) }
                            Button { text: panel.selectedPlaylist?"Remover":"♥"; onClicked: panel.selectedPlaylist?panel.controller.removeFromPlaylist(panel.selectedPlaylist,index):panel.controller.favorite(modelData) }
                        }
                    }
                    Label { anchors.centerIn: parent; visible: saved.count===0; text: "Marque uma música com ♥ ou adicione faixas a uma playlist."; wrapMode: Text.WordWrap; width: parent.width-30; horizontalAlignment: Text.AlignHCenter }
                }
            }
            ColumnLayout {
                RowLayout {
                    ComboBox { id: period; textRole: "text"; valueRole: "value"; model:[{text:"7 dias",value:7},{text:"30 dias",value:30},{text:"90 dias",value:90},{text:"180 dias",value:180}]; onActivated: panel.refresh() }
                    ComboBox { id: dimension; textRole: "text"; valueRole: "value"; model:[{text:"Bandas / artistas",value:"artists"},{text:"Músicas",value:"tracks"},{text:"Estilos",value:"genres"},{text:"Por dia",value:"days"}]; onActivated: panel.refresh() }
                    CheckBox { text: "Registrar escutas locais"; checked: panel.controller.historyEnabled; onToggled: panel.controller.historyEnabled=checked }
                    Button { text: "Apagar histórico"; onClicked: clearDialog.open() }
                }
                Label { text: ((panel.stats.ms || 0)/60000).toFixed(1)+" min • "+(panel.stats.plays || 0)+" escutas qualificadas • "+(panel.stats.vibe || "Dados insuficientes"); font.pixelSize: 18 }
                Label { text: panel.stats.explanation || ""; wrapMode: Text.WordWrap; color: "#b5c9d6"; Layout.fillWidth: true }
                ListView {
                    id: chart; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 5; model: panel.stats.rows || []; ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        required property var modelData
                        width: chart.width; height: 54; color: "#1b2935"; radius: 5
                        Rectangle { anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom; width: parent.width*modelData.ms/Math.max(1,...panel.stats.rows.map(r=>r.ms)); color: "#176078"; radius: 5 }
                        Label { anchors.fill: parent; anchors.margins: 12; text: modelData.label+" • "+(modelData.ms/60000).toFixed(1)+" min • "+modelData.plays+" escutas"; textFormat: Text.PlainText; elide: Text.ElideRight }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { panel.drillTracks=modelData.tracks;panel.displayTracks=modelData.tracks;panel.selectedPlaylist="";tabs.currentIndex=0 } }
                    }
                    Label { anchors.centerIn: parent; visible: chart.count===0; text: "O histórico começa com esta versão. Ouça músicas para preencher o gráfico."; wrapMode: Text.WordWrap; width: parent.width-30; horizontalAlignment: Text.AlignHCenter }
                }
                Label { text: "Sugestões editoriais por estilo informado • toque para buscar"; color: "#b5c9d6" }
                Flow { Layout.fillWidth: true; spacing: 5; Repeater { model: panel.stats.recommendations || []; Button { required property string modelData; text: modelData; onClicked: {panel.discover(modelData);panel.close()} } } }
            }
            ColumnLayout {
                RowLayout {
                    Button { text: "Buscar letra"; enabled: !panel.controller.busy; onClicked: panel.controller.fetchLyrics() }
                    ComboBox { id: language; textRole:"text"; valueRole:"value"; model:[{text:"Inglês",value:"en"},{text:"Espanhol",value:"es"},{text:"Francês",value:"fr"},{text:"Alemão",value:"de"},{text:"Italiano",value:"it"},{text:"Japonês",value:"ja"},{text:"Coreano",value:"ko"},{text:"Português",value:"pt"}] }
                    Button { text: "Traduzir pt-BR"; enabled: !panel.controller.busy && panel.controller.lines.length>0; onClicked: panel.controller.translate(language.currentValue) }
                    Button { text: "Importar / editar"; onClicked: editDialog.open() }
                    CheckBox { text: "Acompanhar"; checked: panel.followLyrics; onToggled: panel.followLyrics=checked }
                }
                Label { text: panel.controller.lyricStatus; textFormat: Text.PlainText; wrapMode: Text.WordWrap; Layout.fillWidth: true; color: "#b5c9d6" }
                Label { text: "Buscar envia título/artista ao LRCLIB; traduzir envia a letra ao Google Tradutor. Letra/versão e tradução podem não estar disponíveis."; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11; color: "#a3b8c8" }
                RowLayout {
                    ComboBox { model:["Aurora","Ondas","Espectro","Capa","Foto","Sem animação"]; Component.onCompleted: currentIndex=Math.max(0,model.indexOf(panel.controller.background)); onActivated: {panel.controller.background=currentText;panel.audioEngine.visualizerActive=panel.visible && tabs.currentIndex===2 && (currentText==="Ondas"||currentText==="Espectro");stage.requestPaint()} }
                    Button { text: "Escolher foto da banda"; onClicked: photoPicker.open() }
                    Label { text: "Ondas / espectro: PCM real antes do volume"; font.pixelSize: 11; color: "#a3b8c8" }
                }
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true; color: "#070f18"; radius: 10; clip: true
                    Image { anchors.fill: parent; source: panel.controller.background==="Foto"?panel.controller.photo:panel.controller.background==="Capa"?(panel.controller.current.image || ""):""; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize.width: 1400; sourceSize.height: 1000 }
                    Canvas {
                        id: stage; anchors.fill: parent
                        onPaint: {
                            let c=getContext("2d");c.reset();let w=width,h=height,mode=panel.controller.background
                            if(mode==="Aurora") {for(let i=0;i<4;i++){let x=w*(0.5+0.4*Math.sin(panel.phase*0.4+i)),y=h*(0.5+0.3*Math.cos(panel.phase*0.3+i*2)),g=c.createRadialGradient(x,y,0,x,y,w*0.65);g.addColorStop(0,["#124c65","#243858","#244a40","#292443"][i]);g.addColorStop(1,"transparent");c.fillStyle=g;c.fillRect(0,0,w,h)}}
                            let data=mode==="Ondas"?panel.audioEngine.metrics.waveform:panel.audioEngine.metrics.spectrum
                            if((mode==="Ondas"||mode==="Espectro") && data && data.length && panel.playing) {
                                c.strokeStyle="#54e0d1";c.fillStyle="#338a9c";c.lineWidth=3;c.beginPath()
                                for(let i=0;i<data.length;i++){if(mode==="Ondas"){let y=h*0.5+data[i]*h*0.32;i?c.lineTo(i*w/(data.length-1),y):c.moveTo(0,y)}else c.fillRect(i*w/data.length,h*(1-data[i]*0.8),w/data.length-3,h*data[i]*0.8)}c.stroke()
                            }
                        }
                    }
                    Rectangle { anchors.fill: parent; color: "#99040a12" }
                    ListView {
                        id: lyrics; anchors.fill: parent; anchors.margins: 18; model: panel.controller.lines; clip: true; spacing: 20; ScrollBar.vertical: ScrollBar {}
                        header: Item { height: 24 }
                        footer: Item { height: 100 }
                        delegate: Column {
                            required property var modelData; required property int index
                            width: lyrics.width-18; spacing: 6
                            Label { width: parent.width; text: modelData.text; textFormat: Text.PlainText; wrapMode: Text.WordWrap; font.pixelSize: panel.highlighted===index?30:25; font.bold: true; color: panel.highlighted===index?"#8bf3e0":"#f2f5fa" }
                            Label { width: parent.width; text: modelData.translation || ""; textFormat: Text.PlainText; visible: text.length>0; wrapMode: Text.WordWrap; font.pixelSize: 21; color: "#d1e6ed" }
                            MouseArea { anchors.fill: parent; enabled: modelData.time>=0; onClicked: panel.controller.seek(modelData.time) }
                        }
                    }
                }
            }
        }
    }
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
