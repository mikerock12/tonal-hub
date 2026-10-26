import QtQuick
import QtQuick.Controls

// Início: atalhos, tocadas recentemente, artistas e músicas mais ouvidas, playlists e álbuns.
Flickable {
    id: root

    property var route
    property var shortcuts: []      // { kind, id, uri, title, imageUrl }
    property var serviceRecent: []
    property var localAlbums: App.music.homeAlbums
    property var recent: mergeAlbums(localAlbums,serviceRecent).slice(0,12)
    property var favoriteAlbums: localAlbums.slice().sort((a,b)=>(b.listenedMs||0)-(a.listenedMs||0)).slice(0,12)
    property var suggestions: []
    property var topArtists: []
    property var topTracks: []
    property var myPlaylists: []
    property var serviceAlbums: []
    property var savedAlbums: mergeAlbums(localAlbums,serviceAlbums)
    property string expanded: ""    // prateleira aberta em "Mostrar tudo"
    property color tint: Theme.accentDeep

    contentHeight: content.implicitHeight + 48
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    ScrollBar.vertical: ThinScrollBar { }

    function greeting() {
        const h = new Date().getHours()
        if (h < 5) return qsTr("Boa noite")
        if (h < 12) return qsTr("Bom dia")
        if (h < 18) return qsTr("Boa tarde")
        return qsTr("Boa noite")
    }

    function cardFor(kind, obj) {
        switch (kind) {
        case "album":
            return { kind: "album", id: obj.id, uri: obj.uri, title: obj.name,
                     subtitle: (obj.release_date ? obj.release_date.substring(0, 4) + " • " : "") + Theme.joinNames(obj.artists),
                     imageUrl: Theme.bestImage(obj.images, 300) }
        case "playlist":
            return { kind: "playlist", id: obj.id, uri: obj.uri, title: obj.name,
                     subtitle: obj.description ? obj.description.replace(/<[^>]*>/g, "")
                                               : qsTr("De %1").arg(obj.owner ? (obj.owner.display_name || obj.owner.id) : ""),
                     imageUrl: Theme.bestImage(obj.images, 300) }
        case "artist":
            return { kind: "artist", id: obj.id, uri: obj.uri, title: obj.name, subtitle: qsTr("Artista"),
                     imageUrl: Theme.bestImage(obj.images, 300), round: true }
        case "track":
            return { kind: "track", id: obj.id, uri: obj.uri, title: obj.name,
                     subtitle: Theme.joinNames(obj.artists), imageUrl: Theme.bestImage(obj.album.images, 300),
                     albumId: obj.album.id }
        }
        return null
    }

    function open(item) {
        if (item.kind === "liked") Nav.go("liked")
        else if (item.kind === "track") Nav.go("album", { id: item.albumId })
        else Nav.go(item.kind, { id: item.id })
    }

    function play(item, list) {
        if (item.kind === "liked") App.player.playContext(Nav.likedSongsUri())
        else if (item.kind === "track") {
            const uris = list.map(t => t.uri)
            App.player.playUris(uris, uris.indexOf(item.uri))
        } else App.player.playContext(item.uri)
    }

    function mergeAlbums(first,second){const seen={};return first.concat(second).filter(c=>{if(!c||!c.uri||!c.title||seen[c.uri])return false;seen[c.uri]=true;return true})}
    function load() {
        App.music.requestHomeAlbums()
        // Tocadas recentemente -> contextos únicos (playlist/álbum/artista)
        App.api.call("GET", "/me/player/recently-played", { limit: 50 }, null, function(status, data) {
            if (status !== 200) return
            const seen = {}
            const pending = []
            for (let i = 0; i < data.items.length && pending.length < 12; ++i) {
                const it = data.items[i]
                const track=it.track||it.item; if(!track||!track.album)continue
                const ctx = it.context
                let uri = ctx && ctx.uri ? ctx.uri : track.album.uri
                if (uri.endsWith(":collection")) uri = "liked"
                if (seen[uri]) continue
                seen[uri] = true
                if (uri === "liked")
                    pending.push({ kind: "liked", id: "", uri: Nav.likedSongsUri(), title: qsTr("Músicas Curtidas"),
                                   subtitle: qsTr("Playlist"), imageUrl: "" })
                else if (uri.indexOf("spotify:album:") === 0)
                    pending.push(root.cardFor("album", track.album))
                else
                    pending.push({ kind: uri.split(":")[1], id: Theme.idFromUri(uri), uri: uri, title: "", imageUrl: "" })
            }
            root.serviceRecent = pending
            root.buildShortcuts()
            // Playlists e artistas do histórico vêm sem nome: busca os metadados (poucas chamadas).
            for (let j = 0; j < pending.length; ++j) {
                const p = pending[j]
                if (p.title !== "") continue
                const path = p.kind === "playlist" ? "/playlists/" + p.id : "/artists/" + p.id
                const q = p.kind === "playlist" ? { fields: "id,uri,name,description,images,owner(display_name,id)" } : {}
                App.api.call("GET", path, q, null, function(s, obj) {
                    const copy = root.serviceRecent.slice()
                    const idx = copy.findIndex(c => c.uri === p.uri)
                    if (idx < 0) return
                    if (s === 200) copy[idx] = root.cardFor(p.kind, obj)
                    else copy.splice(idx, 1)
                    root.serviceRecent = copy
                    root.buildShortcuts()
                })
            }
        })

        App.api.call("GET", "/me/top/artists", { limit: 10, time_range: "short_term" }, null, function(status, data) {
            if(status===200){root.topArtists=data.items.map(a=>root.cardFor("artist",a));for(const artist of data.items.slice(0,3))App.api.call("GET","/artists/"+artist.id+"/albums",{include_groups:"album,single",limit:5},null,function(code,result){if(code===200){const heard={};for(const a of root.localAlbums)heard[a.uri]=true;root.suggestions=root.mergeAlbums(root.suggestions,result.items.map(a=>root.cardFor("album",a))).filter(a=>!heard[a.uri]).slice(0,12)}})}
        })
        App.api.call("GET", "/me/top/tracks", { limit: 10, time_range: "short_term" }, null, function(status, data) {
            if (status === 200) root.topTracks = data.items.map(t => root.cardFor("track", t))
        })
        App.api.call("GET", "/me/playlists", { limit: 20 }, null, function(status, data) {
            if (status === 200) {
                root.myPlaylists = data.items.filter(p => p !== null).map(p => root.cardFor("playlist", p))
                root.buildShortcuts()
            }
        })
        App.api.call("GET", "/me/albums", { limit: 20 }, null, function(status, data) {
            if (status === 200) root.serviceAlbums = data.items.map(it => root.cardFor("album", it.album))
        })
    }

    function buildShortcuts() {
        const out = [{ kind: "liked", id: "", uri: Nav.likedSongsUri(), title: qsTr("Músicas Curtidas"), imageUrl: "" }]
        const pool = recent.concat(myPlaylists)
        const used = { liked: true }
        for (let i = 0; i < pool.length && out.length < 8; ++i) {
            const c = pool[i]
            if (!c || !c.title || c.kind === "liked" || used[c.uri]) continue
            used[c.uri] = true
            out.push(c)
        }
        shortcuts = out
        if (out.length > 1 && out[1].imageUrl)
            App.dominantColor(out[1].imageUrl, function(c) { root.tint = c })
    }

    Component.onCompleted: load()
    onLocalAlbumsChanged: buildShortcuts()
    onVisibleChanged: if(visible)App.music.requestHomeAlbums()

    // Degradê do topo, tingido pela capa do primeiro atalho
    Rectangle {
        width: root.width
        height: 340
        y: 0
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.darker(root.tint, 1.3) }
            GradientStop { position: 1; color: Theme.panel }
        }
    }

    Column {
        id: content
        x: 24
        y: 24
        width: root.width - 48
        spacing: 32

        Label {
            text: root.greeting()
            color: Theme.textPrimary
            font.pixelSize: 30
            font.weight: Font.Bold
        }

        // Grade de atalhos
        Grid {
            id: grid
            width: parent.width
            columns: width > 900 ? 4 : 2
            columnSpacing: 8
            rowSpacing: 8
            visible: root.expanded === ""
            Repeater {
                model: root.shortcuts
                ShortcutTile {
                    required property var modelData
                    width: (grid.width - (grid.columns - 1) * 8) / grid.columns
                    title: modelData.title
                    imageUrl: modelData.imageUrl || ""
                    likedSongs: modelData.kind === "liked"
                    isCurrent: modelData.uri !== "" && App.player.contextUri === modelData.uri
                    isPlaying: App.player.isPlaying
                    onClicked: root.open(modelData)
                    onPlayClicked: {
                        if (isCurrent) App.player.togglePlay()
                        else root.play(modelData, [])
                    }
                }
            }
        }

        Shelf { key: "recent"; title: qsTr("Tocadas recentemente"); model: root.recent.filter(c => c && c.title) }
        Label { visible:!!App.music.homeHistoryError;text:qsTr("Não foi possível consultar todo o histórico local. Os álbuns abaixo podem refletir apenas as escutas recentes.");color:Theme.textSecondary;width:parent.width;wrapMode:Text.Wrap }
        Shelf { key: "topAlbums"; title: qsTr("Seus álbuns mais ouvidos"); model: root.favoriteAlbums }
        Shelf { key: "newAlbums"; title: qsTr("Álbuns dos seus artistas para explorar"); model: root.suggestions }
        Shelf { key: "topArtists"; title: qsTr("Seus artistas mais ouvidos"); model: root.topArtists }
        Shelf { key: "topTracks"; title: qsTr("Suas músicas do momento"); model: root.topTracks }
        Shelf { key: "playlists"; title: qsTr("Suas playlists"); model: root.myPlaylists }
        Shelf { key: "albums"; title: qsTr("Seus álbuns"); model: root.savedAlbums }

        Label {
            visible: root.expanded !== ""
            text: qsTr("‹ Voltar ao Início")
            color: Theme.textSecondary
            font.pixelSize: 14
            font.underline: backMouse.containsMouse
            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { root.expanded = ""; root.contentY = 0 }
            }
        }
    }

    component Shelf: HorizontalShelf {
        property string key
        width: content.width
        visible: model.length > 0 && (root.expanded === "" || root.expanded === key)
        showAll: root.expanded === key
        onShowAllClicked: { root.expanded = key; root.contentY = 0 }
        onItemClicked: (item) => root.open(item)
        onItemPlayClicked: (item) => {
            if (item.uri === App.player.contextUri || item.uri === App.player.trackUri) App.player.togglePlay()
            else root.play(item, model)
        }
    }
}
