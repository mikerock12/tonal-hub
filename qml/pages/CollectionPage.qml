import QtQuick
import QtQuick.Controls

// Página de Playlist, Álbum ou Músicas Curtidas.
Item {
    id: root

    property var route
    readonly property string kind: route ? route.page : ""          // playlist | album | liked
    readonly property string itemId: route && route.params ? (route.params.id || "") : ""

    // metadados do cabeçalho
    property string typeLabel
    property string title
    property string description
    property string imageUrl
    property string uri
    property string ownerName
    property string ownerId
    property string artistId
    property string year
    property string releaseDate
    property string copyright
    property int totalTracks: -1
    property real totalDurationMs: 0
    property bool contentsAvailable: true
    property bool owned: false
    property bool saved: false
    property bool loading: false
    property string nextUrl: ""
    property color tint: "#535353"
    property int serial: 0

    readonly property string playUri: kind === "liked" ? Nav.likedSongsUri() : uri
    readonly property bool isCurrentContext: playUri !== "" && App.player.contextUri === playUri

    // callLater: no onRouteChanged os bindings derivados (kind, itemId) ainda têm o valor da rota
    // anterior; adiando, load() roda uma vez só e já com a rota nova.
    onRouteChanged: Qt.callLater(load)

    ListModel { id: trackModel }

    // ------------------------------------------------------------ dados
    function reset() {
        serial++
        trackModel.clear()
        // Depois de clear() + append() o ListView põe currentIndex em 0 sozinho, e a primeira
        // faixa aparecia selecionada sem ninguém clicar nela.
        list.currentIndex = -1
        typeLabel = ""; title = ""; description = ""; imageUrl = ""; uri = ""
        ownerName = ""; ownerId = ""; artistId = ""; year = ""; releaseDate = ""; copyright = ""
        totalTracks = -1; totalDurationMs = 0; contentsAvailable = true; owned = false; saved = false
        nextUrl = ""; tint = "#535353"
        list.positionViewAtBeginning()
    }

    function load() {
        // Durante a troca de página o Binding do Main.qml pode entregar a rota nova a esta página
        // que está saindo (ex.: playlist → artista). Rotas de outro tipo são ignoradas.
        if (!route || ["playlist", "album", "liked"].indexOf(route.page) < 0) return
        reset()
        const s = serial
        loading = true

        if (kind === "liked") {
            typeLabel = qsTr("Playlist")
            title = qsTr("Músicas Curtidas")
            tint = Theme.accent
            ownerName = Nav.me ? (Nav.me.display_name || Nav.me.id) : ""
            fetchPage("/me/tracks?limit=50", s)
            return
        }
        if (itemId === "") return

        if (kind === "album") {
            App.api.call("GET", "/albums/" + itemId, {}, null, function(status, a) {
                if (s !== root.serial) return
                root.loading = false
                if (status !== 200) { Nav.toast(qsTr("Não consegui abrir o álbum")); return }
                root.typeLabel = a.album_type === "single" ? qsTr("Single") : a.album_type === "compilation" ? qsTr("Compilação") : qsTr("Álbum")
                root.title = a.name
                root.uri = a.uri
                root.imageUrl = Theme.bestImage(a.images, 300)
                root.ownerName = Theme.joinNames(a.artists)
                root.artistId = a.artists.length > 0 ? a.artists[0].id : ""
                root.year = (a.release_date || "").substring(0, 4)
                root.releaseDate = a.release_date || ""
                root.totalTracks = a.total_tracks
                root.copyright = a.copyrights && a.copyrights.length > 0 ? a.copyrights.map(c => (c.type === "P" ? "℗ " : "© ") + c.text.replace(/^[©℗]\s*/, "")).join("\n") : ""
                root.appendTracks(a.tracks.items, a)
                root.nextUrl = a.tracks.next || ""
                root.updateTint()
                root.checkSaved()
            })
            return
        }

        // playlist
        App.api.call("GET", "/playlists/" + itemId, {}, null, function(status, p) {
            if (s !== root.serial) return
            root.loading = false
            if (status !== 200) { Nav.toast(qsTr("Não consegui abrir a playlist")); return }
            // "public" é palavra reservada no modo estrito: o parser do QML não aceita p.public
            root.typeLabel = p["public"] === false ? qsTr("Playlist privada") : qsTr("Playlist")
            root.title = p.name
            root.uri = p.uri
            root.description = (p.description || "").replace(/<[^>]*>/g, "").replace(/&amp;/g, "&").replace(/&#x27;/g, "'").replace(/&quot;/g, "\"")
            root.imageUrl = Theme.bestImage(p.images, 300)
            root.ownerName = p.owner ? (p.owner.display_name || p.owner.id) : ""
            root.ownerId = p.owner ? p.owner.id : ""
            root.owned = !!(Nav.me && p.owner && p.owner.id === Nav.me.id) || !!p.collaborative
            // Fev/2026: o conteúdo vem em "items" (antes "tracks") e só para playlists suas/colaborativas.
            const page = p.items || p.tracks
            if (page && page.items) {
                root.totalTracks = page.total
                root.appendPlaylistEntries(page.items)
                root.nextUrl = page.next || ""
            } else {
                root.contentsAvailable = false
                if (page && page.total !== undefined) root.totalTracks = page.total
            }
            root.updateTint()
            if (!root.owned) root.checkSaved()
        })
    }

    function fetchPage(url, s) {
        loading = true
        App.api.call("GET", url, {}, null, function(status, data) {
            if (s !== root.serial) return
            root.loading = false
            if (status !== 200) return
            if (root.kind === "album") {
                root.appendTracks(data.items, null)
            } else {
                if (root.totalTracks < 0 || root.kind === "liked") root.totalTracks = data.total
                root.appendPlaylistEntries(data.items)
            }
            root.nextUrl = data.next || ""
        })
    }

    function loadMore() {
        if (loading || nextUrl === "") return
        fetchPage(nextUrl, serial)
    }

    // Entradas de playlist / curtidas: { added_at, item|track }
    function appendPlaylistEntries(entries) {
        for (let i = 0; i < entries.length; ++i) {
            const e = entries[i]
            const t = e.item || e.track
            if (!t || !t.uri) continue
            addRow(t, t.album, e.added_at || "")
        }
    }

    // Faixas de álbum (sem objeto album embutido)
    function appendTracks(items, album) {
        for (let i = 0; i < items.length; ++i)
            addRow(items[i], items[i].album || album, "")
    }

    function addRow(t, album, addedAt) {
        const artists = t.artists || (t.show ? [{ name: t.show.name, id: "" }] : [])
        trackModel.append({
            uri: t.uri,
            name: t.name || "",
            artistsText: Theme.joinNames(artists),
            artistId: artists.length > 0 && artists[0].id ? artists[0].id : "",
            artistsJson: JSON.stringify(artists.map(a => ({ id: a.id || "", name: a.name }))),
            album: album ? album.name : "",
            albumId: album ? album.id : "",
            coverUrl: album && album.images ? Theme.bestImage(album.images, 64) : "",
            durationMs: t.duration_ms || 0,
            addedAt: addedAt,
            explicit: !!t.explicit
        })
        totalDurationMs += t.duration_ms || 0
    }

    function updateTint() {
        if (imageUrl !== "")
            App.dominantColor(imageUrl, function(c) { root.tint = c })
    }

    function checkSaved() {
        if (uri === "") return
        App.api.call("GET", "/me/library/contains", { uris: uri }, null, function(status, data) {
            if (status === 200 && data && data.length > 0) root.saved = data[0]
        })
    }

    function toggleSaved() {
        const newState = !saved
        saved = newState
        App.api.call(newState ? "PUT" : "DELETE", "/me/library", { uris: uri }, null, function(status) {
            if (status >= 300) { root.saved = !newState; Nav.toast(qsTr("Não consegui atualizar Sua Biblioteca")) }
            else Nav.toast(newState ? qsTr("Salvo em Sua Biblioteca") : qsTr("Removido de Sua Biblioteca"))
        })
    }

    function playFrom(index) {
        const row = trackModel.get(index)
        if (kind === "liked" && playUri === "") {
            // perfil ainda não carregou: toca a lista carregada
            const uris = []
            for (let i = 0; i < trackModel.count; ++i) uris.push(trackModel.get(i).uri)
            App.player.playUris(uris, index)
            return
        }
        // playlist: posição (aguenta faixas repetidas); álbum/curtidas: URI da faixa
        if (kind === "playlist")
            App.player.playContext(playUri, "", index)
        else
            App.player.playContext(playUri, row ? row.uri : "", row ? -1 : 0)
    }

    function playAll() {
        if (isCurrentContext) { App.player.togglePlay(); return }
        if (trackModel.count > 0) playFrom(0)
        else if (playUri !== "") App.player.playContext(playUri)
    }

    function titleSize() {
        const len = title.length
        const w = list.width
        if (len < 14 && w > 900) return 88
        if (len < 22 && w > 800) return 64
        if (len < 40) return 44
        return 32
    }

    // ------------------------------------------------------------ fundo
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        y: -Math.min(list.contentY - list.originY, 600) * 0.3
        height: 560
        gradient: Gradient {
            GradientStop { position: 0; color: root.tint }
            GradientStop { position: 0.55; color: Qt.darker(root.tint, 2.2) }
            GradientStop { position: 1; color: Theme.panel }
        }
    }

    // ------------------------------------------------------------ lista
    ListView {
        id: list
        anchors.fill: parent
        model: trackModel
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        cacheBuffer: 800
        ScrollBar.vertical: ThinScrollBar { }

        onContentYChanged: {
            if (contentY + height > contentHeight + originY - 1200)
                root.loadMore()
        }

        header: Column {
            width: list.width

            // Capa + título
            Item {
                width: parent.width
                height: 340

                Item {
                    id: coverBox
                    x: 24
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 24
                    width: Math.min(232, list.width * 0.22)
                    height: width

                    Rectangle {   // sombra simples
                        anchors.fill: parent
                        anchors.topMargin: 6
                        anchors.leftMargin: 3
                        anchors.rightMargin: -3
                        anchors.bottomMargin: -10
                        radius: 6
                        color: Qt.rgba(0, 0, 0, 0.45)
                    }
                    Cover {
                        anchors.fill: parent
                        visible: root.kind !== "liked"
                        source: root.imageUrl
                    }
                    LikedArt {
                        anchors.fill: parent
                        visible: root.kind === "liked"
                    }
                }

                Column {
                    anchors.left: coverBox.right
                    anchors.leftMargin: 24
                    anchors.right: parent.right
                    anchors.rightMargin: 24
                    anchors.bottom: coverBox.bottom
                    spacing: 8

                    Label {
                        text: root.typeLabel
                        color: Theme.textPrimary
                        font.pixelSize: 14
                    }
                    Label {
                        width: parent.width
                        text: root.title
                        color: Theme.textPrimary
                        font.pixelSize: root.titleSize()
                        font.weight: Font.Black
                        font.letterSpacing: -1
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        lineHeight: 0.95
                    }
                    Label {
                        visible: root.description !== ""
                        width: parent.width
                        text: root.description
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Label {
                        width: parent.width
                        textFormat: Text.StyledText
                        elide: Text.ElideRight
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.pixelSize: 14
                        text: {
                            const parts = []
                            if (root.ownerName) parts.push("<b><font color='#ffffff'>" + root.ownerName + "</font></b>")
                            if (root.year) parts.push(root.year)
                            if (root.totalTracks >= 0) parts.push(root.totalTracks + (root.totalTracks === 1 ? qsTr(" música") : qsTr(" músicas")))
                            if (root.totalDurationMs > 0 && (root.kind === "album" || trackModel.count >= root.totalTracks))
                                parts.push(Theme.formatLongDuration(root.totalDurationMs))
                            return parts.join(" • ")
                        }
                        MouseArea {
                            anchors.fill: parent
                            enabled: root.artistId !== ""
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: Nav.go("artist", { id: root.artistId })
                        }
                    }
                }
            }

            // Barra de ações
            Item {
                width: parent.width
                height: 104

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: Qt.rgba(0, 0, 0, 0.2) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                }

                Row {
                    x: 24
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 24

                    PlayButton {
                        anchors.verticalCenter: parent.verticalCenter
                        diameter: 56
                        playing: root.isCurrentContext && App.player.isPlaying
                        onClicked: root.playAll()
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        iconSize: 30
                        strokePath: Icons.shuffleS
                        strokeWidth: 1.8
                        active: App.player.shuffle
                        showActiveDot: true
                        tooltip: qsTr("Tocar aleatoriamente")
                        onClicked: {
                            if (!App.player.shuffle) App.player.toggleShuffle()
                            if (!root.isCurrentContext) root.playAll()
                        }
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.kind === "album" || (root.kind === "playlist" && !root.owned)
                        iconSize: 30
                        fillPath: root.saved ? Icons.heart : ""
                        strokePath: root.saved ? "" : Icons.heart
                        strokeWidth: 1.6
                        active: root.saved
                        tooltip: root.saved ? qsTr("Remover de Sua Biblioteca") : qsTr("Salvar em Sua Biblioteca")
                        onClicked: root.toggleSaved()
                    }
                    IconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.uri !== ""
                        iconSize: 28
                        fillPath: Icons.dotsF
                        tooltip: qsTr("Copiar link")
                        onClicked: {
                            App.copyToClipboard("https://open.spotify.com/" + root.kind + "/" + root.itemId)
                            Nav.toast(qsTr("Link copiado"))
                        }
                    }
                }
            }

            // Aviso de playlist sem conteúdo disponível
            Rectangle {
                visible: !root.contentsAvailable
                x: 24
                width: parent.width - 48
                height: visible ? unavailable.implicitHeight + 32 : 0
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.06)
                Label {
                    id: unavailable
                    anchors.fill: parent
                    anchors.margins: 16
                    wrapMode: Text.WordWrap
                    color: Theme.textSecondary
                    font.pixelSize: 14
                    text: qsTr("O Spotify só libera a lista de faixas de playlists suas ou colaborativas para apps em modo de desenvolvimento. Você ainda pode tocar esta playlist com o botão acima, e as faixas aparecem na fila e no player.")
                }
            }

            // Cabeçalho das colunas
            ColumnHeader {
                visible: root.contentsAvailable
                x: 24
                width: parent.width - 48
            }
        }

        delegate: TrackRow {
            id: row
            required property int index
            required property string uri
            required property string name
            required property string artistsText
            required property string artistId
            required property string artistsJson
            required property string albumId
            required property bool explicit
            // propriedades que já existem no TrackRow: só marcamos como "required"
            required album
            required coverUrl
            required durationMs
            required addedAt

            x: 24
            width: list.width - 48
            number: index + 1
            title: name
            artists: artistsText
            explicitContent: explicit
            showCover: root.kind !== "album"
            showAlbum: root.kind !== "album" && width > 640
            showAdded: root.kind !== "album"
            isCurrent: App.player.trackUri === uri
            isPlaying: App.player.isPlaying
            selected: list.currentIndex === index
            onSelectRequested: list.currentIndex = index
            onPlay: root.playFrom(index)
            onArtistClicked: if (artistId) Nav.go("artist", { id: artistId })
            onAlbumClicked: if (albumId) Nav.go("album", { id: albumId })
            onMenuRequested: (mx, my) => Nav.trackMenuRequested(
                { uri: uri, name: name, albumId: albumId, artists: JSON.parse(artistsJson) }, row, mx, my)
        }

        footer: Column {
            width: list.width
            topPadding: 16
            bottomPadding: 48
            spacing: 4

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: root.loading
                visible: running
            }
            Label {
                visible: root.kind === "album" && root.releaseDate !== ""
                x: 24
                text: {
                    const d = new Date(root.releaseDate + "T12:00:00")
                    if (isNaN(d.getTime()) || root.releaseDate.length < 10) return root.releaseDate
                    const months = ["janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho",
                                    "agosto", "setembro", "outubro", "novembro", "dezembro"]
                    return d.getDate() + " de " + months[d.getMonth()] + " de " + d.getFullYear()
                }
                color: Theme.textSecondary
                font.pixelSize: 14
            }
            Label {
                visible: root.kind === "album" && root.copyright !== ""
                x: 24
                width: parent.width - 48
                text: root.copyright
                color: Theme.textSecondary
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }

    // ------------------------------------------------------------ barra fixa ao rolar
    Rectangle {
        id: stickyBar
        readonly property real scrolled: list.contentY - list.originY
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64
        color: Qt.darker(root.tint, 1.6)
        opacity: Math.max(0, Math.min(1, (scrolled - 280) / 60))
        visible: opacity > 0

        Row {
            x: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12
            PlayButton {
                diameter: 44
                anchors.verticalCenter: parent.verticalCenter
                playing: root.isCurrentContext && App.player.isPlaying
                onClicked: root.playAll()
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: 24
                font.weight: Font.Bold
                elide: Text.ElideRight
                width: Math.min(implicitWidth, stickyBar.width - 120)
            }
        }

        // cabeçalho de colunas fixo
        ColumnHeader {
            visible: root.contentsAvailable && stickyBar.scrolled > 440
            anchors.top: parent.bottom
            x: 24
            width: parent.width - 48
            Rectangle { anchors.fill: parent; anchors.leftMargin: -24; anchors.rightMargin: -24; z: -1; color: Theme.panel }
        }
    }

    component ColumnHeader: Item {
        height: 40
        readonly property bool showAlbum: root.kind !== "album" && width > 640
        readonly property bool showAdded: root.kind !== "album" && width > 820
        readonly property real albumWidth: showAlbum ? Math.max(0, (width - 48 - 76 - (root.kind !== "album" ? 150 : 0)) * 0.38) : 0

        Label { x: 0; width: 48; horizontalAlignment: Text.AlignHCenter; anchors.verticalCenter: parent.verticalCenter; text: "#"; color: Theme.textSecondary; font.pixelSize: 14 }
        Label { x: 48; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Título"); color: Theme.textSecondary; font.pixelSize: 14 }
        Label {
            visible: parent.showAlbum
            x: parent.width - 76 - (parent.showAdded ? 150 : 0) - parent.albumWidth
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Álbum"); color: Theme.textSecondary; font.pixelSize: 14
        }
        Label {
            visible: parent.showAdded
            x: parent.width - 76 - 150
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Adicionada em"); color: Theme.textSecondary; font.pixelSize: 14
        }
        Icon {
            anchors.right: parent.right
            anchors.rightMargin: 48
            anchors.verticalCenter: parent.verticalCenter
            size: 16
            strokePath: Icons.clockS
            strokeWidth: 1.8
            color: Theme.textSecondary
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
    }
}
