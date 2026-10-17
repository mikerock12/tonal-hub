import QtCore
import QtQuick
import QtQuick.Controls

// Busca: melhor resultado, músicas, artistas, álbuns e playlists.
Flickable {
    id: root

    property var route
    readonly property string query: route && route.params ? (route.params.query || "") : ""
    property string filter: "all"   // all | track | artist | album | playlist
    property var tracks: []
    property var artists: []
    property var albums: []
    property var playlists: []
    property var totals: ({})
    property bool loading: false
    property int requestSerial: 0

    contentHeight: content.implicitHeight + 48
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    ScrollBar.vertical: ThinScrollBar { }

    Settings {
        id: history
        category: "search"
        // guardado como JSON: listas no registro do Windows voltam com tipos inconsistentes
        property string recentJson: "[]"
    }
    property var recentSearches: App.music.searches

    onQueryChanged: { filter = "all"; search(); discoveryDelay.restart() }
    Timer {
        id: discoveryDelay
        interval: 600; repeat: false
        onTriggered: App.discovery.search(root.query, "extras")
    }
    Component.onCompleted: { if(!App.music.searches.length){try{JSON.parse(history.recentJson).slice().reverse().forEach(q=>App.music.rememberSearch(q))}catch(e){}} search(); discoveryDelay.restart() }

    function rememberQuery() {
        if (query === "") return
        App.music.rememberSearch(query)
    }

    function mapTrack(t) {
        return { uri: t.uri, name: t.name, artists: t.artists, artistsText: Theme.joinNames(t.artists),
                 album: t.album.name, albumId: t.album.id, coverUrl: Theme.bestImage(t.album.images, 64),
                 durationMs: t.duration_ms, explicit: t.explicit }
    }
    function mapCard(kind, o) {
        if (kind === "artist")
            return { kind: kind, id: o.id, uri: o.uri, title: o.name, subtitle: qsTr("Artista"),
                     imageUrl: Theme.bestImage(o.images, 300), round: true }
        if (kind === "album")
            return { kind: kind, id: o.id, uri: o.uri, title: o.name,
                     subtitle: (o.release_date || "").substring(0, 4) + " • " + Theme.joinNames(o.artists),
                     imageUrl: Theme.bestImage(o.images, 300) }
        return { kind: kind, id: o.id, uri: o.uri, title: o.name,
                 subtitle: qsTr("De %1").arg(o.owner ? (o.owner.display_name || o.owner.id) : ""),
                 imageUrl: Theme.bestImage(o.images, 300) }
    }

    function search() {
        tracks = []; artists = []; albums = []; playlists = []; totals = ({})
        if (query === "") return
        const serial = ++requestSerial
        loading = true
        // Em modo de desenvolvimento a API devolve no máximo 10 itens por tipo.
        App.api.call("GET", "/search", { q: query, type: "track,artist,album,playlist", limit: 10 }, null,
                     function(status, data) {
            if (serial !== root.requestSerial) return   // resposta de uma busca antiga
            root.loading = false
            if (status !== 200) return
            root.rememberQuery()
            const valid = x => x !== null
            root.tracks = data.tracks ? data.tracks.items.filter(valid).map(root.mapTrack) : []
            root.artists = data.artists ? data.artists.items.filter(valid).map(a => root.mapCard("artist", a)) : []
            root.albums = data.albums ? data.albums.items.filter(valid).map(a => root.mapCard("album", a)) : []
            root.playlists = data.playlists ? data.playlists.items.filter(valid).map(p => root.mapCard("playlist", p)) : []
            root.totals = {
                track: data.tracks ? data.tracks.total : 0,
                artist: data.artists ? data.artists.total : 0,
                album: data.albums ? data.albums.total : 0,
                playlist: data.playlists ? data.playlists.total : 0
            }
            root.contentY = 0
        })
    }

    function loadMore(kind) {
        const current = kind === "track" ? tracks : kind === "artist" ? artists : kind === "album" ? albums : playlists
        const serial = requestSerial
        App.api.call("GET", "/search", { q: query, type: kind, limit: 10, offset: current.length }, null,
                     function(status, data) {
            if (serial !== root.requestSerial || status !== 200) return
            const page = data[kind + "s"].items.filter(x => x !== null)
            if (kind === "track") root.tracks = root.tracks.concat(page.map(root.mapTrack))
            else if (kind === "artist") root.artists = root.artists.concat(page.map(a => root.mapCard("artist", a)))
            else if (kind === "album") root.albums = root.albums.concat(page.map(a => root.mapCard("album", a)))
            else root.playlists = root.playlists.concat(page.map(p => root.mapCard("playlist", p)))
        })
    }

    function topResult() {
        const q = query.toLowerCase()
        if (artists.length > 0 && artists[0].title.toLowerCase().indexOf(q) === 0) return artists[0]
        if (tracks.length > 0) {
            const t = tracks[0]
            return { kind: "track", uri: t.uri, title: t.name, subtitle: t.artistsText, imageUrl: t.coverUrl, albumId: t.albumId }
        }
        if (artists.length > 0) return artists[0]
        if (albums.length > 0) return albums[0]
        return playlists.length > 0 ? playlists[0] : null
    }

    function openCard(c) {
        rememberQuery()
        if (c.kind === "track") Nav.go("album", { id: c.albumId })
        else Nav.go(c.kind, { id: c.id })
    }

    function playTracks(index) {
        rememberQuery()
        App.player.playUris(tracks.map(t => t.uri), index)
    }

    Column {
        id: content
        x: 24
        y: 16
        width: root.width - 48
        spacing: 24

        // ------------------------------------------------ sem busca: buscas recentes
        Column {
            visible: root.query === ""
            width: parent.width
            spacing: 16
            topPadding: 16

            Label {
                text: root.recentSearches.length > 0 ? qsTr("Buscas recentes") : qsTr("Busque artistas, músicas, álbuns ou playlists")
                color: Theme.textPrimary
                font.pixelSize: 24
                font.weight: Font.Bold
            }
            Flow {
                width: parent.width
                spacing: 8
                Repeater {
                    model: root.recentSearches
                    Chip {
                        required property var modelData
                        text: modelData
                        onClicked: Nav.go("search", { query: modelData })
                    }
                }
            }
            Chip {
                visible: root.recentSearches.length > 0
                text: qsTr("Limpar buscas recentes")
                onClicked: App.music.forgetSearch("")
            }
        }

        // ------------------------------------------------ filtros
        Rectangle {
            visible: root.query.length > 1
            width: parent.width
            height: discoveryColumn.implicitHeight + 28
            color: "#152b37"; radius: 10
            Column {
                id: discoveryColumn
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top; anchors.margins: 14; spacing: 8
                Row {
                    spacing: 12
                    Label { text: "Explore também no IndieTonal"; font.pixelSize: 17; font.bold: true; color: "#a4e1ee" }
                    Label { text: App.discovery.busy ? "Consultando fontes abertas…" : App.discovery.items.length + " candidatos carregados"; color: "#9ab7c5" }
                }
                Label { text: "Busca independente pelo texto digitado. Resultados podem ser versões diferentes ou homônimos; não indicam exclusividade."; color: "#afc1cc"; wrapMode: Text.WordWrap; width: parent.width; font.pixelSize: 12 }
                Repeater {
                    model: App.discovery.query === root.query ? App.discovery.items.filter(item => item.kind !== "artist").slice(0, 5) : []
                    Label {
                        required property var modelData
                        width: discoveryColumn.width; elide: Text.ElideRight; color: Theme.textPrimary
                        textFormat: Text.PlainText
                        text: modelData.title + " • " + (modelData.artist || modelData.kind) + " • " + modelData.provider
                    }
                }
                Button { text: "Abrir esta busca no IndieTonal"; onClicked: { if (App.player.isPlaying) App.player.togglePlay(); App.hub.indie(root.query) } }
            }
        }

        Row {
            visible: root.query !== ""
            spacing: 8
            Repeater {
                model: [
                    { key: "all", label: qsTr("Tudo") },
                    { key: "track", label: qsTr("Músicas") },
                    { key: "artist", label: qsTr("Artistas") },
                    { key: "album", label: qsTr("Álbuns") },
                    { key: "playlist", label: qsTr("Playlists") }
                ]
                Chip {
                    required property var modelData
                    text: modelData.label
                    checked: root.filter === modelData.key
                    onClicked: { root.filter = modelData.key; root.contentY = 0 }
                }
            }
        }

        BusyIndicator {
            visible: root.loading
            running: visible
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Label {
            visible: root.query !== "" && !root.loading && root.tracks.length + root.artists.length
                     + root.albums.length + root.playlists.length === 0
            text: qsTr("Nenhum resultado para \"%1\"").arg(root.query)
            color: Theme.textPrimary
            font.pixelSize: 20
            font.weight: Font.Bold
        }

        // ------------------------------------------------ Tudo: melhor resultado + músicas
        Row {
            id: topRow
            visible: root.query !== "" && root.filter === "all" && root.topResult() !== null
            width: parent.width
            spacing: 24
            // "top" não serve: é uma âncora FINAL do Item
            readonly property var best: root.topResult()

            Column {
                width: Math.min(440, parent.width * 0.42)
                spacing: 8
                SectionHeader { width: parent.width; title: qsTr("Melhor resultado") }

                Rectangle {
                    id: topCard
                    width: parent.width
                    height: 236
                    radius: 8
                    color: topHover.hovered ? Theme.elevatedHover : Theme.elevated
                    HoverHandler { id: topHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: if (topRow.best) root.openCard(topRow.best) }

                    Cover {
                        x: 20; y: 20
                        width: 92; height: 92
                        source: topRow.best ? topRow.best.imageUrl : ""
                        round: topRow.best ? !!topRow.best.round : false
                    }
                    Label {
                        x: 20; y: 132
                        width: parent.width - 40
                        text: topRow.best ? topRow.best.title : ""
                        color: Theme.textPrimary
                        font.pixelSize: 32
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Row {
                        x: 20; y: 180
                        spacing: 8
                        Label {
                            text: topRow.best ? topRow.best.subtitle : ""
                            color: Theme.textSecondary
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            visible: !!topRow.best && topRow.best.kind === "track"
                            width: kindLabel.implicitWidth + 16; height: 24; radius: 12
                            color: Qt.rgba(0, 0, 0, 0.3)
                            Label { id: kindLabel; anchors.centerIn: parent; text: qsTr("Música"); color: Theme.textPrimary; font.pixelSize: 12; font.weight: Font.Bold }
                        }
                    }
                    PlayButton {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 20
                        diameter: 48
                        opacity: topHover.hovered ? 1 : 0
                        visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 150 } }
                        onClicked: {
                            const t = topRow.best
                            if (t.kind === "track") root.playTracks(0)
                            else { root.rememberQuery(); App.player.playContext(t.uri) }
                        }
                    }
                }
            }

            Column {
                width: parent.width - Math.min(440, parent.width * 0.42) - 24
                spacing: 8
                visible: root.tracks.length > 0
                SectionHeader { width: parent.width; title: qsTr("Músicas") }
                Repeater {
                    model: root.tracks.slice(0, 4)
                    SearchTrackRow { required property var modelData; required property int index; track: modelData; trackIndex: index; showAlbum: false }
                }
            }
        }

        // ------------------------------------------------ Músicas (filtro)
        Column {
            visible: root.filter === "track" && root.tracks.length > 0
            width: parent.width
            Repeater {
                model: root.filter === "track" ? root.tracks : []
                SearchTrackRow { required property var modelData; required property int index; track: modelData; trackIndex: index }
            }
            LoadMore { kind: "track"; count: root.tracks.length }
        }

        // ------------------------------------------------ prateleiras
        HorizontalShelf {
            width: parent.width
            title: qsTr("Artistas")
            model: root.filter === "all" ? root.artists : []
            onItemClicked: (c) => root.openCard(c)
            onItemPlayClicked: (c) => { root.rememberQuery(); App.player.playContext(c.uri) }
        }
        HorizontalShelf {
            width: parent.width
            title: qsTr("Álbuns")
            model: root.filter === "all" ? root.albums : []
            onItemClicked: (c) => root.openCard(c)
            onItemPlayClicked: (c) => { root.rememberQuery(); App.player.playContext(c.uri) }
        }
        HorizontalShelf {
            width: parent.width
            title: qsTr("Playlists")
            model: root.filter === "all" ? root.playlists : []
            onItemClicked: (c) => root.openCard(c)
            onItemPlayClicked: (c) => { root.rememberQuery(); App.player.playContext(c.uri) }
        }

        // ------------------------------------------------ grade (filtro artista/álbum/playlist)
        Column {
            visible: root.filter === "artist" || root.filter === "album" || root.filter === "playlist"
            width: parent.width
            spacing: 8
            HorizontalShelf {
                width: parent.width
                title: ""
                showAll: true
                model: root.filter === "artist" ? root.artists : root.filter === "album" ? root.albums
                     : root.filter === "playlist" ? root.playlists : []
                onItemClicked: (c) => root.openCard(c)
                onItemPlayClicked: (c) => { root.rememberQuery(); App.player.playContext(c.uri) }
            }
            LoadMore {
                kind: root.filter
                count: root.filter === "artist" ? root.artists.length : root.filter === "album" ? root.albums.length : root.playlists.length
            }
        }
    }

    component SearchTrackRow: TrackRow {
        id: str
        property var track
        property int trackIndex: 0
        width: content.width
        number: trackIndex + 1
        showNumber: root.filter === "track"
        title: track.name
        artists: track.artistsText
        album: track.album
        coverUrl: track.coverUrl
        durationMs: track.durationMs
        explicitContent: !!track.explicit
        isCurrent: App.player.trackUri === track.uri
        isPlaying: App.player.isPlaying
        onPlay: root.playTracks(trackIndex)
        onArtistClicked: { root.rememberQuery(); if (track.artists.length > 0) Nav.go("artist", { id: track.artists[0].id }) }
        onAlbumClicked: { root.rememberQuery(); Nav.go("album", { id: track.albumId }) }
        onMenuRequested: (mx, my) => Nav.trackMenuRequested({ uri: track.uri, name: track.name, albumId: track.albumId, artists: track.artists }, str, mx, my)
    }

    component LoadMore: Item {
        property string kind
        property int count: 0
        width: parent ? parent.width : 0
        height: visible ? 56 : 0
        visible: count > 0 && count < (root.totals[kind] || 0) && count < 100
        Chip {
            anchors.centerIn: parent
            text: qsTr("Carregar mais")
            onClicked: root.loadMore(parent.kind)
        }
    }
}
