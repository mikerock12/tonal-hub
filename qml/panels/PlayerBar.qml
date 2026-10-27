import QtQuick
import QtQuick.Controls
import "qrc:/companion/qml/companion" as Personal

// Barra do player: faixa atual (esquerda), controles (centro), fila/dispositivo/volume (direita).
Item {
    id: root

    readonly property var player: App.player
    readonly property bool playingElsewhere: player.activeDeviceId !== "" && !player.playingHere
    property int libraryRevision: 0
    readonly property bool trackLiked: {libraryRevision;return player.localPlayback ? App.music.liked(App.music.current) : player.liked}
    Connections { target: App.music; function onChanged(){root.libraryRevision++} }

    // Shake, Aleatório e Descoberta 2+1 aplicados direto à fila atual, a partir
    // da música que toca (ela continua). A janela Fila mantém prévia e Aplicar.
    readonly property var mix: App.listeningMix
    readonly property bool mixAvailable: player.hasTrack && !player.localPlayback && player.trackUri.startsWith("spotify:track:")
    property var pendingMode: null
    property bool mixRequested: false
    function currentRow() {
        const c = App.music.current
        if (c && c.uri === player.trackUri) return c
        const a = player.artists
        return {provider:"Spotify", id:player.trackUri, uri:player.trackUri, title:player.trackName, artist:player.artistsText,
                artistId:a.length ? (a[0].id || "") : "", album:player.albumName, image:player.coverUrl, durationMs:player.durationMs}
    }
    function queueRow(t) {
        return {provider:"Spotify", id:t.uri, uri:t.uri, title:t.name, artist:t.artists, artistId:t.artistId || "", album:t.album || "",
                albumId:t.albumId || "", year:t.year || 0, image:t.coverUrl, durationMs:t.durationMs, mixExternal:!!t.mixExternal}
    }
    function applyMode(mode) {
        if (!mixAvailable) return
        let random = mix.randomEnabled, discovery = mix.discoveryEnabled
        if (mode === "random") random = !random
        if (mode === "discovery") discovery = !discovery
        const request = {shake: mode === "shake", random: random, discovery: discovery, anchor: player.trackUri}
        const committed = mix.queueFor(player.trackUri)
        mixRequested = true
        if (committed.length > 1) { mix.applyModes(committed, request.shake, random, discovery); return }
        pendingMode = request
        player.refreshQueue()
    }
    Connections {
        target: root.player
        function onQueueRefreshFinished(status) {
            if (!root.pendingMode) return
            const m = root.pendingMode
            root.pendingMode = null
            if (m.anchor !== root.player.trackUri) { root.mixRequested = false; return }
            if (status !== 200) { root.mixRequested = false; Nav.toast(qsTr("Não foi possível carregar a fila (Spotify HTTP %1).").arg(status)); return }
            const rows = [root.currentRow()].concat(root.player.queue.map(root.queueRow))
            if (rows.length < 2) { root.mixRequested = false; Nav.toast(qsTr("A fila só tem a música atual. Abra um álbum ou playlist e toque a seleção.")); return }
            root.mix.applyModes(rows, m.shake, m.random, m.discovery)
        }
    }
    Connections {
        target: root.mix
        function onPreviewReady() { if (root.mixRequested) { root.mixRequested = false; Nav.toast(root.mix.status) } }
    }

    // ---------------------------------------------------- esquerda: faixa atual
    Item {
        id: nowPlaying
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.playingElsewhere ? -6 : 0
        width: Math.min(root.width * 0.3, 420)
        height: 56
        visible: root.player.hasTrack

        Cover {
            id: cover
            width: 56; height: 56
            source: root.player.coverUrl
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Nav.go("lyrics")
            }
        }

        Column {
            anchors.left: cover.right
            anchors.leftMargin: 14
            anchors.right: likeButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Label {
                width: Math.min(parent.width, implicitWidth)
                text: root.player.trackName
                color: Theme.textPrimary
                font.pixelSize: 14
                font.underline: titleMouse.containsMouse
                elide: Text.ElideRight
                MouseArea {
                    id: titleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Nav.go("lyrics")
                }
            }
            Label {
                width: Math.min(parent.width, implicitWidth)
                text: root.player.artistsText
                color: artistMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: 12
                font.underline: artistMouse.containsMouse
                elide: Text.ElideRight
                MouseArea {
                    id: artistMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const a = root.player.artists
                        if (a.length > 0 && a[0].id) Nav.go("artist", { id: a[0].id })
                    }
                }
            }
        }

        IconButton {
            id: likeButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 18
            fillPath: root.trackLiked ? Icons.heart : ""
            strokePath: root.trackLiked ? "" : Icons.heart
            strokeWidth: 1.8
            active: root.trackLiked
            visible: root.player.hasTrack
            tooltip: root.trackLiked ? qsTr("Remover das favoritas") : qsTr("Favoritar música")
            onClicked: root.player.localPlayback ? App.music.favorite(App.music.current) : root.player.toggleLike()
        }
    }

    // ---------------------------------------------------- centro: controles
    Column {
        id: controls
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.playingElsewhere ? -6 : 0
        width: Math.min(root.width * 0.4, 722)
        spacing: 4

        Row {
            objectName: "transportControls"
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            Personal.ActionIcon {
                objectName: "shakeButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 36; height: 36
                symbol: "shake"
                accentInk: Theme.accent
                enabled: root.mixAvailable && !root.mix.busy
                description: qsTr("Shake: sortear outra ordem da fila agora")
                onClicked: root.applyMode("shake")
            }
            Personal.ActionIcon {
                objectName: "randomButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 36; height: 36
                symbol: "random"
                accentInk: Theme.accent
                filled: root.mix.randomEnabled
                enabled: root.mixAvailable && !root.mix.busy
                description: root.mix.randomEnabled ? qsTr("Aleatório ligado: voltar à ordem") : qsTr("Aleatório: tocar a fila em ordem aleatória")
                onClicked: root.applyMode("random")
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconSize: 18
                fillPath: Icons.prevF
                tooltip: qsTr("Voltar")
                onClicked: root.player.previous()
            }
            PlayButton {
                anchors.verticalCenter: parent.verticalCenter
                diameter: 34
                color: Theme.textPrimary
                hoverColor: Theme.textPrimary
                iconColor: "black"
                playing: root.player.isPlaying
                onClicked: root.player.togglePlay()
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconSize: 18
                fillPath: Icons.nextF
                tooltip: qsTr("Avançar")
                onClicked: root.player.next()
            }
            Personal.ActionIcon {
                objectName: "repeatButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 36; height: 36
                symbol: root.player.repeatMode === "track" ? "repeat1" : "repeat"
                accentInk: Theme.accent
                filled: root.player.repeatMode !== "off"
                enabled: root.player.hasTrack && !root.player.localPlayback
                description: root.player.repeatMode === "off" ? qsTr("Repetir a fila")
                       : root.player.repeatMode === "context" ? qsTr("Repetindo a fila: repetir só esta música")
                       : qsTr("Repetindo esta música: desligar repetição")
                onClicked: root.player.cycleRepeat()
            }
            Personal.ActionIcon {
                objectName: "discoveryButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 36; height: 36
                symbol: "discover"
                accentInk: Theme.accent
                filled: root.mix.discoveryEnabled
                enabled: root.mixAvailable && !root.mix.busy
                opacity: root.mix.busy ? 0.55 : 1
                description: root.mix.busy ? root.mix.status
                       : root.mix.discoveryEnabled ? qsTr("Descoberta 2+1 ligada: tirar as próximas externas")
                       : qsTr("Descoberta 2+1: duas da fila, uma descoberta")
                onClicked: root.applyMode("discovery")
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Label {
                width: 40
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: Theme.formatTime(seek.dragging ? seek.dragValue * root.player.durationMs : root.player.positionMs)
                color: Theme.textSecondary
                font.pixelSize: 12
                font.features: ({ "tnum": 1 })
            }
            SeekBar {
                id: seek
                width: parent.width - 96
                anchors.verticalCenter: parent.verticalCenter
                enabled: root.player.hasTrack
                value: root.player.durationMs > 0 ? root.player.positionMs / root.player.durationMs : 0
                onMoved: (v) => root.player.seek(Math.round(v * root.player.durationMs))
            }
            Label {
                width: 40
                anchors.verticalCenter: parent.verticalCenter
                text: Theme.formatTime(root.player.durationMs)
                color: Theme.textSecondary
                font.pixelSize: 12
                font.features: ({ "tnum": 1 })
            }
        }
    }

    // ---------------------------------------------------- direita: fila, dispositivos, volume
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: root.playingElsewhere ? -6 : 0
        spacing: 2

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 17
            strokePath: Icons.lyricsS
            strokeWidth: 1.8
            enabled: root.player.hasTrack
            active: Nav.current.page === "lyrics"
            showActiveDot: true
            tooltip: qsTr("Letra")
            onClicked: Nav.current.page === "lyrics" ? Nav.back() : Nav.go("lyrics")
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 17
            strokePath: Icons.nowPlayingS
            strokeWidth: 1.8
            active: Nav.rightPanel === "nowplaying"
            showActiveDot: true
            tooltip: qsTr("Tela Tocando agora")
            onClicked: Nav.rightPanel = Nav.rightPanel === "nowplaying" ? "" : "nowplaying"
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 17
            strokePath: Icons.queueS
            active: Nav.rightPanel === "queue"
            showActiveDot: true
            tooltip: qsTr("Fila")
            onClicked: {
                Nav.rightPanel = Nav.rightPanel === "queue" ? "" : "queue"
                if (Nav.rightPanel === "queue") root.player.refreshQueue()
            }
        }
        IconButton {
            id: devicesButton
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 17
            strokePath: Icons.devicesS
            strokeWidth: 1.8
            active: root.playingElsewhere || devicePopup.opened
            tooltip: qsTr("Conectar a um dispositivo")
            onClicked: {
                root.player.refreshDevices()
                devicePopup.open()
            }
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 17
            fillPath: Icons.speakerF
            objectName: "muteButton"
            strokePath: root.player.volume === 0 ? Icons.muteS
                      : root.player.volume < 50 ? Icons.volLowS : Icons.volHighS
            strokeWidth: 1.8
            tooltip: root.player.volume === 0 ? qsTr("Ativar som") : qsTr("Sem som")
            // O volume anterior ao mudo fica salvo no Player (sobrevive a reinícios).
            onClicked: root.player.toggleMute()
        }
        SeekBar {
            objectName: "volumeSlider"
            width: 96
            anchors.verticalCenter: parent.verticalCenter
            wheelEnabled: true
            value: root.player.volume / 100
            onMoving: (v) => root.player.setVolume(Math.round(v * 100))
            onMoved: (v) => root.player.setVolume(Math.round(v * 100))
            ToolTip.visible: hovered || dragging
            ToolTip.text: Math.round(shownValue * 100) + "%"
        }
    }

    DevicePopup {
        id: devicePopup
        parent: devicesButton
        x: devicesButton.width / 2 - width / 2 - 100
        y: -height - 12
    }

    // Faixa "Tocando em <dispositivo>"
    Rectangle {
        visible: root.playingElsewhere
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gap
        anchors.bottomMargin: 2
        height: 20
        radius: 4
        color: Theme.accent
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Icon {
                size: 14
                strokePath: Icons.devicesS
                strokeWidth: 2
                color: Theme.textOnAccent
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                text: qsTr("Tocando em %1").arg(root.player.activeDeviceName)
                color: Theme.textOnAccent
                font.pixelSize: 12
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
