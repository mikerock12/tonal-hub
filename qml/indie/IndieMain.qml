import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "qrc:/companion/qml/companion" as Personal
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 1180; height: 820; minimumWidth: 800; minimumHeight: 600
    visible: true; title: hubMode ? "Tonal Hub" : "IndieTonal"
    color: "#101316"
    palette.window: "#171c22"; palette.base: "#202731"; palette.text: "#f4f6f8"
    palette.windowText: "#f4f6f8"; palette.button: "#263240"; palette.buttonText: "#f4f6f8"
    palette.highlight: "#11afd1"; palette.highlightedText: "#081318"
    palette.placeholderText: "#9aaebb"
    property string selectedProvider: "all"
    property string selectedKind: "all"
    property string notice: ""
    property var playerItem: ({})
    property var playQueue: []
    property int queueIndex: -1
    property bool songRequested: false
    property int musicRevision: 0
    function openMusic(song=false,section=0){songRequested=song;if(musicLoader.item){if(song)musicLoader.item.showSong();else{musicLoader.item.section=section;musicLoader.item.open()}}else musicLoader.setSource("qrc:/companion/qml/companion/MusicPanel.qml",{controller:music,audioEngine:audio,section:song?2:section})}
    Loader { id: musicLoader; onLoaded: {if(window.songRequested)item.showSong();else item.open()} }
    Binding { target: musicLoader.item; property:"position"; value:audio ? audio.positionMs : 0; when:!!musicLoader.item }
    Binding { target: musicLoader.item; property:"playing"; value:audio ? audio.localPlaying : false; when:!!musicLoader.item }
    Connections { target: musicLoader.item; function onDiscover(artist){search.text=artist;catalog.search(artist,selectedProvider)} function onTogglePlayback(){audio.toggleLocalPlay()} function onPreviousRequested(){playAt(queueIndex-1)} function onNextRequested(){playAt(queueIndex+1)} }
    Connections { target: music; function onChanged(){window.musicRevision++} function onQueueRequested(tracks,index){playQueue=tracks;playAt(index)} function onMessage(text){window.notice=text} }
    function kindMatches(item, kind) {
        return kind === "all" || item.kind === kind || (kind === "album" && (item.kind === "playlist" || item.kind === "release"))
    }
    function playAt(index) {
        if (index < 0 || index >= playQueue.length) return
        queueIndex = index; playerItem = playQueue[index]; music.selectTrack(playerItem)
        if(playerItem.localFile)audio.openFile(playerItem.localFile);else catalog.resolve(playerItem)
    }
    function playItem(item) {
        playQueue = catalog.items.filter(entry => entry.playable === true)
        playAt(playQueue.findIndex(entry => entry.provider === item.provider && entry.id === item.id))
    }
    function stopPlayback() { playQueue = []; queueIndex = -1; audio.returnToSpotify() }
    function showPanel(name) { if(name==="music")openMusic();else if(name==="song")openMusic(true);else if(name==="statistics")openMusic(false,1);else if (name === "connections") { config.populate(); config.open() } else if (name === "effects") effects.open(); else if (name === "shows") selectedKind = "show" }
    Connections { target: hub; function onError(message) { window.notice = message } }
    Connections {
        target: catalog
        function onSearchStarted(query, provider) {
            selectedKind = "all"
            search.text = query
            selectedProvider = provider
            sources.currentIndex = Math.max(0, sources.model.indexOf(provider))
        }
    }
    Connections {
        target: audio
        function onLocalChanged() {
            if (!audio.sourceFailed && !audio.localPlaying && audio.durationMs > 0 && audio.positionMs >= audio.durationMs && queueIndex + 1 < playQueue.length) playAt(queueIndex + 1)
        }
    }
    Component.onCompleted: if (!hubMode && initialQuery.length > 1) catalog.search(initialQuery)
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 24; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Label { text: hubMode ? "Tonal Hub" : "IndieTonal"; font.pixelSize: 28; font.bold: true; Layout.fillWidth: true }
            Button { text: "Tonalify"; onClicked: hub.spotify() }
            Button { text: hubMode ? "Abrir IndieTonal" : "Tonal Hub"; onClicked: hubMode ? hub.indie() : hub.hub() }
            Button { text: "Conexões"; visible: !hubMode; onClicked: { config.populate(); config.open() } }
            Button { text: "Áudio"; visible: !hubMode; onClicked: effects.open() }
            Button { text: "Sua música"; visible: !hubMode; onClicked: window.openMusic() }
            Button { text: "Arquivo"; visible: !hubMode; onClicked: filePicker.open() }
        }
        Label { visible: notice !== ""; text: notice; color: "#ffbf78"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ColumnLayout {
            visible: hubMode; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 22
            Label { text: "Sua música. Outros caminhos para descobrir."; font.pixelSize: 26 }
            Label { text: "Escolha um player. Cada um mantém suas conexões e seu processo."; color: "#9eaebc" }
            RowLayout {
                Layout.fillWidth: true; spacing: 20
                Rectangle {
                    color: "#183329"; radius: 16; Layout.fillWidth: true; height: 250
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 26; spacing: 14
                        Label { text: "Tonalify"; font.pixelSize: 30; font.bold: true }
                        Label { text: "Spotify"; font.pixelSize: 18; color: "#b2d0c1" }
                        Label { text: "Sua biblioteca, busca e reprodução atuais."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        Button { text: "Abrir Tonalify"; onClicked: hub.spotify() }
                    }
                }
                Rectangle {
                    color: "#153342"; radius: 16; Layout.fillWidth: true; height: 250
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 26; spacing: 14
                        Label { text: "IndieTonal"; font.pixelSize: 30; font.bold: true }
                        Label { text: "Fontes abertas e sua coleção"; font.pixelSize: 18; color: "#b2d0de" }
                        Label { text: "Audius, discografias, shows e bibliotecas próprias."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        Item { Layout.fillHeight: true }
                        Button { text: "Explorar IndieTonal"; onClicked: hub.indie() }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                TextField { id: hubQuery; Layout.fillWidth: true; placeholderText: "Uma banda para descobrir outras gravações"; onAccepted: hub.indie(text) }
                Button { text: "Explorar"; onClicked: hub.indie(hubQuery.text) }
            }
            Label { text: "O hub pode ficar aberto enquanto você usa os players. Reprodução e resultados mantêm a identificação da fonte."; color: "#9eaebc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Item { Layout.fillHeight: true }
        }
        RowLayout {
            visible: !hubMode; Layout.fillWidth: true
            TextField { id: search; objectName: "indieSearch"; text: initialQuery; Layout.fillWidth: true; placeholderText: "Artista, música ou show"; onAccepted: {music.rememberSearch(text);catalog.search(text, selectedProvider)} }
            ComboBox { id: sources; Layout.preferredWidth: 190; model: ["Todas as fontes", "Audius", "MusicBrainz", "Internet Archive", "Aadam Jacobs", "Live Music Archive", "Jamendo", "OpenSubsonic"]; onActivated: { selectedProvider = currentIndex === 0 ? "all" : currentText; catalog.search(search.text, selectedProvider) } }
            Button { text: "Buscar"; onClicked: {music.rememberSearch(search.text);catalog.search(search.text, selectedProvider)} }
        }
        RowLayout {visible:!hubMode&&!search.text.length;Layout.fillWidth:true
            Label{text:"Buscas recentes";font.bold:true}
            ComboBox{id:recent;Layout.fillWidth:true;model:music.searches;onActivated:{search.text=currentText;music.rememberSearch(currentText);catalog.search(currentText,selectedProvider)}}
            Button{text:"Limpar";onClicked:music.forgetSearch("")}
        }
        RowLayout {
            visible: !hubMode; Layout.fillWidth: true
            Label { text: catalog.heading; textFormat: Text.PlainText; font.pixelSize: 22; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
            BusyIndicator { running: catalog.busy; visible: running; Layout.preferredWidth: 30; Layout.preferredHeight: 30 }
            Button { text: "Tentar novamente"; objectName: "retryCatalog"; visible: catalog.canRetry; enabled: !catalog.busy; onClicked: catalog.retryFailed() }
            Button { text: "Voltar à busca"; onClicked: catalog.search(search.text, selectedProvider) }
        }
        Label { visible: !hubMode; text: "Resultados são candidatos: versões, remixes e artistas homônimos permanecem separados. MusicBrainz informa a discografia e não fornece áudio."; color: "#9eaebc"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        RowLayout {
            visible: !hubMode && !catalog.inDetail; Layout.fillWidth: true; spacing: 3
            Repeater {
                model: [{kind:"all",label:"Todos"},{kind:"artist",label:"Artistas"},{kind:"album",label:"Álbuns / playlists"},{kind:"track",label:"Músicas"},{kind:"show",label:"Shows"},{kind:"demo",label:"Demos"},{kind:"recording",label:"Gravações"}]
                Button {
                    required property var modelData
                    text: modelData.label + " (" + catalog.items.filter(entry => window.kindMatches(entry, modelData.kind)).length + ")"
                    checkable: true; checked: selectedKind === modelData.kind
                    onClicked: selectedKind = modelData.kind
                }
            }
            Item { Layout.fillWidth: true }
        }
        Label { visible: !hubMode && catalog.liveStatus.length > 0; text: catalog.liveStatus; color: "#9fd8df"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        Label { visible: !hubMode && catalog.messages.length > 0; text: catalog.messages.join("\n"); color: "#d8b87d"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ListView {
            id: results; objectName: "indieResults"; visible: !hubMode; Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; spacing: 6; model: catalog.items.filter(entry => catalog.inDetail || window.kindMatches(entry, selectedKind)); ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: results.width; height: 94; color: "#1b222a"; radius: 8
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 58; Layout.preferredHeight: 58; color: "#2a404f"; radius: 6
                        Image { id: coverImage; anchors.fill: parent; source: modelData.image || ""; sourceSize.width: 116; sourceSize.height: 116; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                        Label { visible: coverImage.status !== Image.Ready; anchors.centerIn: parent; text: modelData.kind === "show" ? "LIVE" : modelData.kind === "artist" ? "ART" : "♪"; color: "#9fd8df" }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 4
                        Label { text: modelData.title || "Sem título"; textFormat: Text.PlainText; elide: Text.ElideRight; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true }
                        Label { text: modelData.artist || ({artist:"Artista",album:"Álbum",track:"Música",show:"Show",playlist:"Playlist",release:"Edição"})[modelData.kind] || "Gravação"; textFormat: Text.PlainText; color: "#afbeca"; elide: Text.ElideRight; Layout.fillWidth: true }
                        Label { text: modelData.provider + " • " + (modelData.detail || ""); textFormat: Text.PlainText; color: "#67cddd"; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Button { visible: modelData.kind !== "track"; text: modelData.kind === "artist" ? "Ver músicas / álbuns" : "Ver faixas / edições"; onClicked: catalog.open(modelData) }
                    Button { visible: modelData.playable === true; text: "Reproduzir"; onClicked: window.playItem(modelData) }
                    Personal.ActionIcon {symbol:"heart";filled:{window.musicRevision;return music.liked(modelData)} description:filled?"Remover das favoritas":"Favoritar música";visible:modelData.playable===true; onClicked: music.favorite(modelData) }
                    Button { visible: !!modelData.url; text: "Fonte"; onClicked: hub.external(modelData.url) }
                }
            }
            Label {
                anchors.centerIn: parent; width: Math.min(600, parent.width - 32)
                horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                visible: results.count === 0
                text: catalog.busy ? "Consultando a fonte… Aguarde as tentativas automáticas."
                      : catalog.canRetry ? "Não foi possível carregar esta página. Toque em Tentar novamente ou volte à busca para escolher outra fonte."
                      : catalog.inDetail ? "A fonte não retornou itens para esta página. Volte à busca para explorar outros resultados."
                      : selectedKind === "show" ? (catalog.liveStatus || "Selecione Todas as fontes ou Live Music Archive para buscar shows.")
                      : catalog.query.length > 1 ? "Nenhum resultado encontrado nesta consulta. Tente outro nome ou outra fonte."
                      : "Busque um artista ou escolha outra fonte."
                color: "#9eaebc"
            }
        }
        RowLayout {
            visible: !hubMode; Layout.fillWidth: true
            Label { text: catalog.items.length + " resultados carregados • não é uma comparação de exclusividade"; color: "#9eaebc"; Layout.fillWidth: true }
            Button { text: "Carregar mais"; visible: catalog.hasMore; enabled: !catalog.busy && !catalog.canRetry; onClicked: catalog.more() }
        }
        Rectangle {
            visible: !hubMode; Layout.fillWidth: true; height: 94; color: "#202b34"; radius: 10
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 12; spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    Image { source:music.current.image || ""; Layout.preferredWidth:44; Layout.preferredHeight:44; fillMode:Image.PreserveAspectFit; asynchronous:true; sourceSize.width:88; sourceSize.height:88; MouseArea { anchors.fill:parent; onClicked:window.openMusic(true) } }
                    Label { text:music.current.title || "Escolha uma música"; elide:Text.ElideRight; Layout.fillWidth:true; MouseArea { anchors.fill:parent; cursorShape:Qt.PointingHandCursor; onClicked:window.openMusic(true) } }
                    Personal.ActionIcon {symbol:"heart";filled:{window.musicRevision;return music.liked(music.current)} description:filled?"Remover das favoritas":"Favoritar música"; enabled:!!music.current.title; onClicked:music.favorite(music.current) }
                    Button { text: "Anterior"; enabled: queueIndex > 0; onClicked: playAt(queueIndex - 1) }
                    Button { text: audio.sourceFailed ? "Tentar novamente" : audio.localPlaying ? "Pausar" : "Reproduzir"; enabled: audio.localActive; onClicked: { if (audio.sourceFailed) { if (queueIndex >= 0) catalog.resolve(playerItem); else filePicker.open() } else audio.toggleLocalPlay() } }
                    Button { text: "Próxima"; enabled: queueIndex >= 0 && queueIndex + 1 < playQueue.length; onClicked: playAt(queueIndex + 1) }
                    Button { text: "Parar"; enabled: audio.localActive; onClicked: stopPlayback() }
                    Slider { from: 0; to: 100; value: 60; Layout.preferredWidth: 110; onMoved: audio.setLocalVolume(Math.round(value)) }
                }
                Label { text: audio.diagnostics; color: "#9eaebc"; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                Slider { Layout.fillWidth: true; from: 0; to: Math.max(1,audio.durationMs); value: audio.positionMs; enabled: audio.localActive && audio.durationMs > 0; onMoved: audio.seekLocal(Math.round(value)) }
            }
        }
    }
    FileDialog {
        id: filePicker; title: "Abrir áudio local no IndieTonal"
        nameFilters: ["Áudio (*.mp3 *.flac *.wav *.aac *.m4a *.ogg *.opus *.aiff *.iamf)", "Todos os arquivos (*)"]
        onAccepted: { playerItem={provider:"Local",id:selectedFile.toString(),title:decodeURIComponent(selectedFile.toString().split('/').pop()),localFile:selectedFile.toString(),playable:true};playQueue=[playerItem];playAt(0) }
    }
    Dialog {
        id: config; title: "Conexões do IndieTonal"; anchors.centerIn: parent; width: Math.min(620,window.width-40); modal: true
        footer: RowLayout { Button { text: "Cancelar"; Layout.fillWidth: true; onClicked: config.reject() } Button { text: "Salvar"; Layout.fillWidth: true; onClicked: config.accept() } }
        function populate() { const c = catalog.configuration; jamendo.text = c.jamendoClientId; server.text = c.server; user.text = c.user; password.text = "" }
        onAccepted: { if (!catalog.configure({jamendoClientId:jamendo.text,server:server.text,user:user.text,password:password.text})) window.notice = "A conexão não foi salva. Confira os campos e os avisos da busca."; else { if (playerItem.provider === "OpenSubsonic") stopPlayback(); catalog.search(search.text, selectedProvider) } password.text = "" }
        onRejected: password.text = ""
        contentItem: ColumnLayout {
            spacing: 12
            Label { text: "Audius, MusicBrainz e Live Music Archive não precisam de login para estas consultas."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            TextField { id: jamendo; Layout.fillWidth: true; placeholderText: "Jamendo: client ID próprio ativo" }
            Label { text: "OpenSubsonic / Navidrome / coleção Bandcamp"; font.bold: true }
            TextField { id: server; Layout.fillWidth: true; placeholderText: "https://seu-servidor ou https://bandcamp.com/api/subsonic" }
            TextField { id: user; Layout.fillWidth: true; placeholderText: "Usuário da biblioteca" }
            TextField { id: password; Layout.fillWidth: true; placeholderText: "Senha / credencial Subsonic (vazio mantém a atual)"; echoMode: TextInput.Password }
            Label { text: "Credenciais ficam no cofre do Windows, separadas do Spotify. Use as credenciais Subsonic geradas nas configurações do Bandcamp, não a senha principal."; wrapMode: Text.WordWrap; color: "#afbeca"; Layout.fillWidth: true }
            Button { text: "Remover conexão da biblioteca"; onClicked: { if (playerItem.provider === "OpenSubsonic") stopPlayback(); catalog.clearLibraryCredentials(); config.populate(); catalog.search(search.text, selectedProvider) } }
        }
    }
    Dialog {
        id: effects; title: "Áudio do IndieTonal"; anchors.centerIn: parent; modal: true; width: 620; height: Math.min(680,window.height-40)
        footer: Button { text: "Fechar"; onClicked: effects.close() }
        contentItem: ScrollView {
            id: effectsScroll
            clip: true; contentWidth: availableWidth
            ColumnLayout {
                width: effectsScroll.availableWidth; spacing: 8
                CheckBox { text: "Equalizador de 10 bandas"; checked: audio.settings.eqEnabled; onToggled: audio.setSetting("eqEnabled",checked) }
                ComboBox { model: ["Flat","Rock","Pop","Clássica","Eletrônica","Vocal"]; onActivated: audio.applyPreset(currentText) }
                Repeater {
                    model: [31,62,125,250,500,1000,2000,4000,8000,16000]
                    RowLayout {
                        required property int index
                        required property int modelData
                        Layout.fillWidth: true
                        Label { text: modelData + " Hz"; Layout.preferredWidth: 70 }
                        Slider { from: -12; to: 12; stepSize: 0.5; value: audio.settings.eq[index]; Layout.fillWidth: true; onMoved: audio.setEqBand(index,value) }
                        Label { text: Number(audio.settings.eq[index]).toFixed(1) + " dB"; Layout.preferredWidth: 65 }
                    }
                }
                Label { text: "Graves" }
                Slider { from: 0; to: 12; stepSize: 0.5; value: audio.settings.bass; Layout.fillWidth: true; onMoved: audio.setSetting("bass",value) }
                CheckBox { text: "Compressor"; checked: audio.settings.compressor; onToggled: audio.setSetting("compressor",checked) }
                CheckBox { text: "Normalização (ReplayGain / análise de arquivos locais)"; checked: audio.settings.normalization; onToggled: audio.setSetting("normalization",checked) }
                Label { text: "Alvo de loudness para arquivos locais: " + Number(audio.settings.targetLufs).toFixed(1) + " LUFS" }
                Slider { from: -24; to: -10; stepSize: 0.5; value: audio.settings.targetLufs; Layout.fillWidth: true; onMoved: audio.setSetting("targetLufs",value) }
                Label { text: "Limitador de picos de amostra: " + Number(audio.settings.ceilingDb).toFixed(1) + " dBFS" }
                Slider { from: -6; to: -0.1; stepSize: 0.1; value: audio.settings.ceilingDb; Layout.fillWidth: true; onMoved: audio.setSetting("ceilingDb",value) }
                CheckBox { text: "HRTF para fones"; checked: audio.settings.spatial === "hrtf"; onToggled: audio.setSetting("spatial",checked?"hrtf":"original") }
                Label { text: audio.sourceIamf ? "IAMF: renderização binaural ativa" : "IAMF binaural disponível para arquivos locais compatíveis"; visible: audio.iamfAvailable; color: "#afbeca" }
                Label { text: "Streams usam buffer de leitura e o mesmo DSP nativo. A análise completa de loudness fica disponível para arquivos locais; não se analisa toda a música antes de iniciar um stream."; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Button { text: "Restaurar áudio"; onClicked: audio.reset() }
            }
        }
    }
}
