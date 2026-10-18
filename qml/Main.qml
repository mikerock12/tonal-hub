import QtCore
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window
    property alias audioEffectsController: audioPanel
    function openMusic(){if(musicLoader.item)musicLoader.item.open();else musicLoader.setSource("qrc:/companion/qml/companion/MusicPanel.qml",{controller:App.music,audioEngine:App.audio,textPlaylistAvailable:true})}
    property string pendingTextPlaylistId:""
    function openTextPlaylist(id){pendingTextPlaylistId=id || "";if(!textPlaylistLoader.active)textPlaylistLoader.active=true;else if(textPlaylistLoader.item){if(pendingTextPlaylistId)textPlaylistLoader.item.resume(pendingTextPlaylistId);else textPlaylistLoader.item.fresh()}}
    Loader {id:textPlaylistLoader;active:false;sourceComponent:Component {TextPlaylistDialog {controller:App.textPlaylist}} onLoaded:{if(window.pendingTextPlaylistId)item.resume(window.pendingTextPlaylistId);else item.fresh()}}
    Connections {target:textPlaylistLoader.item;function onAuthorizationRequested(){App.auth.login()}function onLocalPlaylistRequested(id){window.openMusic();if(musicLoader.item){musicLoader.item.section=0;musicLoader.item.selectedPlaylist=id;musicLoader.item.refresh()}}}
    Connections {target:App.textPlaylist;function onSpotifyChanged(){Playlists.listChanged()}}
    Loader { id: musicLoader; onLoaded: item.open() }
    Binding { target: musicLoader.item; property: "position"; value: App.player.positionMs; when: !!musicLoader.item }
    Binding { target: musicLoader.item; property: "playing"; value: App.player.isPlaying && App.player.playingHere; when: !!musicLoader.item }
    Connections { target: musicLoader.item; function onTextPlaylistRequested(){musicLoader.item.close();window.openTextPlaylist("")} function onTextPlaylistResume(id){musicLoader.item.close();window.openTextPlaylist(id)} function onDiscover(artist){Nav.go("search",{query:artist})} function onTogglePlayback(){App.player.togglePlay()} function onPreviousRequested(){App.player.previous()} function onNextRequested(){App.player.next()} }

    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    color: Theme.windowBg
    title: App.player.hasTrack && App.player.isPlaying
           ? App.player.trackName + " • " + App.player.artistsText
           : "Tonalify"

    palette.toolTipBase: "#282828"
    palette.toolTipText: Theme.textPrimary
    palette.dark: "#282828"
    palette.window: Theme.panel
    palette.windowText: Theme.textPrimary
    palette.text: Theme.textPrimary
    palette.base: Theme.elevated
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.textOnAccent
    palette.placeholderText: Theme.textMuted

    Settings {
        id: settings
        category: "window"
        property alias x: window.x
        property alias y: window.y
        property alias width: window.width
        property alias height: window.height
        property real libraryWidth: 320
    }

    // ------------------------------------------------------------ login
    Loader {
        anchors.fill: parent
        active: !App.auth.authenticated && !App.audio.localActive
        sourceComponent: LoginPage { }
    }

    // ------------------------------------------------------------ app
    Item {
        id: shell
        anchors.fill: parent
        visible: App.auth.authenticated || App.audio.localActive

        // Botões laterais do mouse (voltar/avançar). Fica por baixo de tudo:
        // os outros MouseAreas só aceitam o botão esquerdo, então esses cliques chegam aqui.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.BackButton | Qt.ForwardButton
            onClicked: (e) => {
                if (e.button === Qt.BackButton) Nav.back()
                else Nav.forward()
            }
        }

        TopBar {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Theme.topBarHeight
            onAudioSettingsRequested: audioPanel.open()
            onMusicRequested: window.openMusic()
        }

        LibraryPanel {
            id: library
            anchors.left: parent.left
            anchors.leftMargin: Theme.gap
            anchors.top: topBar.bottom
            anchors.bottom: playerBar.top
            width: collapsed ? 72 : Math.max(280, Math.min(480, settings.libraryWidth))
            collapsed: settings.libraryWidth < 200
            onToggleCollapse: settings.libraryWidth = collapsed ? 320 : 72
        }

        // Alça para redimensionar a biblioteca
        MouseArea {
            id: libraryHandle
            x: library.x + library.width
            width: Theme.gap
            anchors.top: library.top
            anchors.bottom: library.bottom
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            property real startX: 0
            property real startWidth: 0
            onPressed: (e) => { startX = mapToItem(shell, e.x, 0).x; startWidth = library.width }
            onPositionChanged: (e) => {
                if (!pressed) return
                const dx = mapToItem(shell, e.x, 0).x - startX
                settings.libraryWidth = startWidth + dx
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 1
                height: parent.height
                color: Theme.textMuted
                opacity: parent.containsMouse || parent.pressed ? 0.6 : 0
            }
        }

        Rectangle {
            id: mainPanel
            anchors.left: libraryHandle.right
            anchors.right: rightPanel.visible ? rightPanel.left : parent.right
            anchors.rightMargin: Theme.gap
            anchors.top: topBar.bottom
            anchors.bottom: playerBar.top
            radius: Theme.radiusPanel
            color: Theme.panel
            clip: true

            Loader {
                id: pageLoader
                anchors.fill: parent
                sourceComponent: {
                    if(!App.auth.authenticated && App.audio.localActive) return localAudioPage
                    switch (Nav.current.page) {
                    case "search": return searchPage
                    case "playlist":
                    case "album":
                    case "liked": return collectionPage
                    case "artist": return artistPage
                    case "lyrics": return lyricsPage
                    case "setlists": return setlistsPage
                    default: return homePage
                    }
                }
            }

            Binding {
                target: pageLoader.item
                property: "route"
                value: Nav.current
                when: pageLoader.status === Loader.Ready
            }
        }

        RightPanel {
            id: rightPanel
            visible: Nav.rightPanel !== ""
            anchors.right: parent.right
            anchors.rightMargin: Theme.gap
            anchors.top: topBar.bottom
            anchors.bottom: playerBar.top
            width: Math.min(380, Math.max(300, window.width * 0.24))
        }

        PlayerBar {
            id: playerBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Theme.playerBarHeight
        }

    }

    Component { id: homePage; HomePage { } }
    Component { id: searchPage; SearchPage { } }
    Component { id: collectionPage; CollectionPage { } }
    Component { id: artistPage; ArtistPage { } }
    Component { id: lyricsPage; LyricsPage { } }
    Component { id: setlistsPage; SetlistsPage { } }
    Component {
        id: localAudioPage
        Item {
            property var route
            Label {
                anchors.centerIn: parent
                width: parent.width - 48
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: App.audio.localName + "\n\n" + qsTr("Use a barra de reprodução. Ctrl+E abre os efeitos e arquivos locais.")
                color: Theme.textPrimary
                font.pixelSize: 18
            }
        }
    }

    AudioEffectsPanel { id: audioPanel }
    Button {
        visible: !App.auth.authenticated && !App.audio.localActive
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        text: qsTr("Áudio e arquivos locais…")
        onClicked: audioPanel.open()
    }

    // ------------------------------------------------------------ atalhos
    Shortcut { sequence: "Ctrl+E"; onActivated: audioPanel.open() }
    Shortcut { sequence: "Space"; enabled: App.auth.authenticated || App.audio.localActive; onActivated: App.player.togglePlay() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: App.player.next() }
    Shortcut { sequence: "Ctrl+Left"; onActivated: App.player.previous() }
    Shortcut { sequence: "Ctrl+Up"; onActivated: App.player.setVolume(App.player.volume + 10) }
    Shortcut { sequence: "Ctrl+Down"; onActivated: App.player.setVolume(App.player.volume - 10) }
    Shortcut { sequences: ["Ctrl+L", "Ctrl+K"]; onActivated: topBar.focusSearch() }
    Shortcut { sequence: "Alt+Left"; onActivated: Nav.back() }
    Shortcut { sequence: "Alt+Right"; onActivated: Nav.forward() }

    // ------------------------------------------------------------ menu de faixa
    DarkMenu {
        id: trackMenu
        // { uri, name, albumId, artists: [{id, name}], removeFromPlaylist?: function }
        property var track: ({})

        DarkMenuItem {
            text: qsTr("Adicionar à fila")
            onTriggered: App.player.addToQueue(trackMenu.track.uri)
        }
        DarkMenu {
            id: addToPlaylistMenu
            title: qsTr("Adicionar à playlist")

            DarkMenuItem {
                text: qsTr("Nova playlist")
                onTriggered: Playlists.createWithTracks([trackMenu.track.uri])
            }
            MenuSeparator {
                contentItem: Rectangle { implicitHeight: 1; color: Theme.divider }
            }
            // As playlists entram depois dos dois itens fixos acima.
            Instantiator {
                model: Playlists.editable
                delegate: DarkMenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: Playlists.addTracks(modelData, [trackMenu.track.uri])
                }
                onObjectAdded: (index, object) => addToPlaylistMenu.insertItem(index + 2, object)
                onObjectRemoved: (index, object) => addToPlaylistMenu.removeItem(object)
            }
        }
        DarkMenuItem {
            text: qsTr("Remover desta playlist")
            visible: !!trackMenu.track.removeFromPlaylist
            height: visible ? implicitHeight : 0
            onTriggered: trackMenu.track.removeFromPlaylist()
        }
        DarkMenuItem {
            text: qsTr("Ir para o artista")
            enabled: !!(trackMenu.track.artists && trackMenu.track.artists.length > 0 && trackMenu.track.artists[0].id)
            onTriggered: Nav.go("artist", { id: trackMenu.track.artists[0].id })
        }
        DarkMenuItem {
            text: qsTr("Ir para o álbum")
            enabled: !!trackMenu.track.albumId
            onTriggered: Nav.go("album", { id: trackMenu.track.albumId })
        }
        DarkMenuItem {
            text: qsTr("Copiar link da música")
            onTriggered: {
                App.copyToClipboard("https://open.spotify.com/track/" + Theme.idFromUri(trackMenu.track.uri))
                Nav.toast(qsTr("Link copiado"))
            }
        }
    }

    // ------------------------------------------------------------ permissão para editar playlists
    // Quem entrou antes de o Tonal pedir playlist-modify-private autoriza de novo, sem sair da conta.
    ModalDialog {
        id: permissionDialog
        title: qsTr("Autorizar a edição de playlists")
        onClosed: {
            if (App.auth.busy) App.auth.cancelLogin()
            Playlists.pending = null
        }

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.textSecondary
            font.pixelSize: 14
            text: App.auth.busy
                  ? qsTr("Confirme no navegador. Esta janela fecha sozinha quando o Spotify responder.")
                  : qsTr("Para criar e editar playlists, o Spotify precisa que você autorize o Tonal de novo, agora com acesso às suas playlists privadas. O navegador vai abrir na página de autorização: é só confirmar.")
        }
        Label {
            visible: App.auth.lastError !== ""
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.danger
            font.pixelSize: 13
            text: App.auth.lastError
        }

        buttons: [
            PillButton {
                primary: false
                text: App.auth.busy ? qsTr("Cancelar") : qsTr("Agora não")
                onClicked: permissionDialog.close()
            },
            PillButton {
                visible: !App.auth.busy
                text: qsTr("Autorizar")
                onClicked: App.auth.login()
            }
        ]
    }

    Connections {
        target: Playlists
        function onPermissionNeeded() { permissionDialog.open() }
        function onTextImportRequested() {window.openTextPlaylist("")}
    }
    Connections {
        target: App.auth
        function onGrantedScopesChanged() {
            if (!App.auth.canEditPlaylists || !permissionDialog.opened) return
            Playlists.runPending()
            permissionDialog.close()
        }
    }

    Connections {
        target: Nav
        function onTrackMenuRequested(track, item, x, y) {
            trackMenu.track = track
            trackMenu.popup(item, x, y)
        }
        function onToast(text) { toast.show(text) }
    }

    // ------------------------------------------------------------ toast
    Connections {
        target: App.player
        function onMessage(text) { toast.show(text) }
    }

    Rectangle {
        id: toast
        property string text
        function show(t) { text = t; opacity = 1; toastTimer.restart() }
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.playerBarHeight + 24
        width: toastLabel.implicitWidth + 32
        height: 40
        radius: 8
        color: Theme.textPrimary
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Label {
            id: toastLabel
            anchors.centerIn: parent
            text: toast.text
            color: "black"
            font.pixelSize: 14
        }
        Timer { id: toastTimer; interval: 2600; onTriggered: toast.opacity = 0 }
    }

    // ------------------------------------------------------------ boot
    function bootstrap() {
        if (App.auth.authenticated)
            Nav.loadMe()
    }
    Connections {
        target: App.auth
        function onAuthenticatedChanged() { window.bootstrap() }
    }
    Component.onCompleted: {
        bootstrap()
        if(Qt.application.arguments.indexOf("--audio-settings") >= 0) audioPanel.open()
    }
}
