import QtCore
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    width: 1280
    height: 820
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    color: Theme.windowBg
    title: App.player.hasTrack && App.player.isPlaying
           ? App.player.trackName + " • " + App.player.artistsText
           : "Tonal"

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
        active: !App.auth.authenticated
        sourceComponent: LoginPage { }
    }

    // ------------------------------------------------------------ app
    Item {
        id: shell
        anchors.fill: parent
        visible: App.auth.authenticated

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
                    switch (Nav.current.page) {
                    case "search": return searchPage
                    case "playlist":
                    case "album":
                    case "liked": return collectionPage
                    case "artist": return artistPage
                    case "lyrics": return lyricsPage
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

    // ------------------------------------------------------------ atalhos
    Shortcut { sequence: "Space"; enabled: App.auth.authenticated; onActivated: App.player.togglePlay() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: App.player.next() }
    Shortcut { sequence: "Ctrl+Left"; onActivated: App.player.previous() }
    Shortcut { sequence: "Ctrl+Up"; onActivated: App.player.setVolume(App.player.volume + 10) }
    Shortcut { sequence: "Ctrl+Down"; onActivated: App.player.setVolume(App.player.volume - 10) }
    Shortcut { sequences: ["Ctrl+L", "Ctrl+K"]; onActivated: topBar.focusSearch() }
    Shortcut { sequence: "Alt+Left"; onActivated: Nav.back() }
    Shortcut { sequence: "Alt+Right"; onActivated: Nav.forward() }

    // ------------------------------------------------------------ menu de faixa
    Menu {
        id: trackMenu
        property var track: ({})

        background: Rectangle {
            implicitWidth: 240
            color: "#282828"
            radius: 4
            border.color: Qt.rgba(1, 1, 1, 0.06)
        }

        DarkMenuItem {
            text: qsTr("Adicionar à fila")
            onTriggered: App.player.addToQueue(trackMenu.track.uri)
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

    component DarkMenuItem: MenuItem {
        id: mi
        implicitHeight: 40
        contentItem: Label {
            leftPadding: 4
            text: mi.text
            color: mi.enabled ? Theme.textPrimary : Theme.textMuted
            font.pixelSize: 14
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: mi.highlighted ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
            radius: 2
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
    Component.onCompleted: bootstrap()
}
