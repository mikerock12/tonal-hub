import QtQuick
import QtQuick.Controls

// Barra do player: faixa atual (esquerda), controles (centro), fila/dispositivo/volume (direita).
Item {
    id: root

    readonly property var player: App.player
    readonly property bool playingElsewhere: player.activeDeviceId !== "" && !player.playingHere

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
                onClicked: Nav.rightPanel = Nav.rightPanel === "nowplaying" ? "" : "nowplaying"
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
                    onClicked: Nav.openUri(root.player.albumUri)
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
            fillPath: root.player.liked ? Icons.heart : ""
            strokePath: root.player.liked ? "" : Icons.heart
            strokeWidth: 1.8
            active: root.player.liked
            visible: root.player.trackUri.indexOf("spotify:track:") === 0
            tooltip: root.player.liked ? qsTr("Remover das Músicas Curtidas") : qsTr("Adicionar às Músicas Curtidas")
            onClicked: root.player.toggleLike()
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
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconSize: 18
                strokePath: Icons.shuffleS
                active: root.player.shuffle
                showActiveDot: true
                tooltip: root.player.shuffle ? qsTr("Desativar aleatório") : qsTr("Ativar aleatório")
                onClicked: root.player.toggleShuffle()
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
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                iconSize: 18
                strokePath: root.player.repeatMode === "track" ? Icons.repeatOneS : Icons.repeatS
                active: root.player.repeatMode !== "off"
                showActiveDot: true
                tooltip: root.player.repeatMode === "off" ? qsTr("Ativar repetição")
                       : root.player.repeatMode === "context" ? qsTr("Ativar repetição de uma faixa")
                       : qsTr("Desativar repetição")
                onClicked: root.player.cycleRepeat()
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
            strokePath: root.player.volume === 0 ? Icons.muteS
                      : root.player.volume < 50 ? Icons.volLowS : Icons.volHighS
            strokeWidth: 1.8
            tooltip: root.player.volume === 0 ? qsTr("Ativar som") : qsTr("Sem som")
            property int lastVolume: 50
            onClicked: {
                if (root.player.volume > 0) {
                    lastVolume = root.player.volume
                    root.player.setVolume(0)
                } else {
                    root.player.setVolume(lastVolume)
                }
            }
        }
        SeekBar {
            width: 96
            anchors.verticalCenter: parent.verticalCenter
            wheelEnabled: true
            value: root.player.volume / 100
            onMoving: (v) => root.player.setVolume(Math.round(v * 100))
            onMoved: (v) => root.player.setVolume(Math.round(v * 100))
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
