import QtQuick
import QtQuick.Controls

// Página de artista: banner, populares e discografia.
Flickable {
    id: root

    property var route
    readonly property string artistId: route && route.params ? (route.params.id || "") : ""

    property string name
    property string imageUrl
    property string uri
    property string genres
    property var popular: []
    property var releases: []
    property var releaseNext: ({})   // próxima página de cada tipo ("album" | "single" | "compilation")
    property string releaseFilter: "album"
    property bool showAllPopular: false
    property bool showAllReleases: false
    property bool following: false
    property color tint: "#404040"
    property int serial: 0

    readonly property bool isCurrentContext: uri !== "" && App.player.contextUri === uri

    // O conteúdo começa abaixo do banner (content.y = banner.height): sem somá-lo, o fim da página
    // (a discografia) ficava fora da área de rolagem.
    contentHeight: banner.height + content.implicitHeight + 64
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    ScrollBar.vertical: ThinScrollBar { }

    // callLater: no onRouteChanged o artistId (derivado da rota) ainda é o do artista anterior;
    // adiando, load() roda uma vez só e já com a rota nova.
    onRouteChanged: Qt.callLater(load)

    function load() {
        // Rotas de outro tipo podem chegar aqui durante a troca de página (ver CollectionPage.load).
        if (!route || route.page !== "artist" || artistId === "") return
        const s = ++serial
        contentY = 0
        name = ""; imageUrl = ""; uri = ""; genres = ""; popular = []; releases = []; releaseNext = ({})
        showAllPopular = false; showAllReleases = false; following = false; tint = "#404040"
        releaseFilter = "album"   // o filtro do artista anterior não vale para este

        App.api.call("GET", "/artists/" + artistId, {}, null, function(status, a) {
            if (s !== root.serial || status !== 200) return
            root.name = a.name
            root.uri = a.uri
            root.imageUrl = Theme.bestImage(a.images, 640)
            root.genres = (a.genres || []).slice(0, 3).join(", ")
            if (root.imageUrl) App.dominantColor(Theme.bestImage(a.images, 64), function(c) { root.tint = c })
            root.loadPopular(s)
        })

        // Fev/2026: neste endpoint o limit vai até 10 ("Invalid limit" acima disso). Um pedido por tipo
        // traz os 10 mais recentes de cada um, sem que os álbuns ocupem o espaço dos singles.
        const groups = ["album", "single", "compilation"]
        let pending = groups.length
        for (const group of groups) {
            App.api.call("GET", "/artists/" + artistId + "/albums", { include_groups: group, limit: 10 }, null,
                         function(status, data) {
                if (s !== root.serial) return
                if (status === 200)
                    root.addReleases(group, data)
                // Sem álbuns, abre no primeiro tipo que tiver lançamentos (depois de todas as respostas).
                if (--pending === 0 && !root.releases.some(r => r.type === "album")) {
                    const other = root.releases.find(r => r.type !== "album")
                    if (other) root.releaseFilter = other.type
                }
            })
        }

        App.api.call("GET", "/me/library/contains", { uris: "spotify:artist:" + artistId }, null, function(status, data) {
            if (s === root.serial && status === 200 && data && data.length > 0) root.following = data[0]
        })
    }

    function addReleases(group, page) {
        root.releases = root.releases.concat(page.items.filter(al => al !== null).map(function(al) {
            return { kind: "album", id: al.id, uri: al.uri, title: al.name, type: group,
                     subtitle: (al.release_date || "").substring(0, 4) + " • " +
                               (al.album_type === "single" ? qsTr("Single") : al.album_type === "compilation" ? qsTr("Compilação") : qsTr("Álbum")),
                     imageUrl: Theme.bestImage(al.images, 300) }
        }))
        const next = Object.assign({}, root.releaseNext)
        next[group] = page.next || ""
        root.releaseNext = next
    }

    // "Mostrar tudo": busca o resto do tipo escolhido, uma página (10) por vez, até 50 lançamentos.
    // Só sob demanda, para não gastar a cota da API em toda visita.
    function loadMoreReleases() {
        const group = releaseFilter
        const url = releaseNext[group]
        if (!url || releases.filter(r => r.type === group).length >= 50) return
        const s = serial
        const n = Object.assign({}, releaseNext)
        n[group] = ""   // evita pedir a mesma página duas vezes
        releaseNext = n
        App.api.call("GET", url, {}, null, function(status, data) {
            if (s !== root.serial) return
            if (status !== 200) {   // devolve o link para dar para tentar de novo
                const back = Object.assign({}, root.releaseNext)
                back[group] = url
                root.releaseNext = back
                return
            }
            root.addReleases(group, data)
            if (root.showAllReleases && root.releaseFilter === group)
                root.loadMoreReleases()
        })
    }

    onReleaseFilterChanged: if (showAllReleases) loadMoreReleases()

    // O endpoint de "top tracks" saiu do modo de desenvolvimento em fev/2026.
    // A busca por artist:"Nome" devolve as faixas mais relevantes do artista, o que dá um resultado parecido.
    function loadPopular(s) {
        App.api.call("GET", "/search", { q: "artist:\"" + name + "\"", type: "track", limit: 10 }, null, function(status, data) {
            if (s !== root.serial || status !== 200) return
            root.popular = data.tracks.items
                .filter(t => t && t.artists.some(a => a.id === root.artistId))
                .map(t => ({ uri: t.uri, name: t.name, artists: t.artists, artistsText: Theme.joinNames(t.artists),
                             album: t.album.name, albumId: t.album.id, coverUrl: Theme.bestImage(t.album.images, 64),
                             durationMs: t.duration_ms, explicit: t.explicit }))
        })
    }

    function toggleFollow() {
        const newState = !following
        following = newState
        App.api.call(newState ? "PUT" : "DELETE", "/me/library", { uris: uri }, null, function(status) {
            if (status >= 300) { root.following = !newState; Nav.toast(qsTr("Não consegui atualizar")) }
        })
    }

    function playArtist() {
        if (isCurrentContext) App.player.togglePlay()
        else App.player.playContext(uri)
    }

    // ------------------------------------------------------------ banner
    Item {
        id: banner
        width: root.width
        height: 360

        Rectangle { anchors.fill: parent; color: root.tint }
        Cover {
            anchors.fill: parent
            source: root.imageUrl
            radius: 0
            placeholderIcon: Icons.personS
        }
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.3; color: "transparent" }
                GradientStop { position: 1; color: Qt.rgba(0, 0, 0, 0.6) }
            }
        }
        Column {
            x: 24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            width: parent.width - 48
            spacing: 4
            Label {
                text: root.genres !== "" ? root.genres : qsTr("Artista")
                color: Theme.textPrimary
                font.pixelSize: 14
            }
            Label {
                width: parent.width
                text: root.name
                color: Theme.textPrimary
                font.pixelSize: root.name.length < 16 && root.width > 800 ? 88 : 56
                font.weight: Font.Black
                font.letterSpacing: -1.5
                elide: Text.ElideRight
            }
        }
    }

    // ------------------------------------------------------------ conteúdo
    Rectangle {
        y: banner.height
        width: root.width
        height: 300
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.darker(root.tint, 2.4) }
            GradientStop { position: 1; color: Theme.panel }
        }
    }

    Column {
        id: content
        y: banner.height
        x: 24
        width: root.width - 48
        spacing: 24
        topPadding: 24

        Row {
            spacing: 24
            PlayButton {
                anchors.verticalCenter: parent.verticalCenter
                diameter: 56
                playing: root.isCurrentContext && App.player.isPlaying
                onClicked: root.playArtist()
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: followLabel.implicitWidth + 32
                height: 32
                radius: 16
                color: "transparent"
                border.width: 1
                border.color: followMouse.containsMouse ? Theme.textPrimary : Theme.textMuted
                scale: followMouse.containsMouse ? 1.04 : 1
                Label {
                    id: followLabel
                    anchors.centerIn: parent
                    text: root.following ? qsTr("Seguindo") : qsTr("Seguir")
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
                MouseArea {
                    id: followMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.toggleFollow()
                }
            }
        }

        // Populares
        Column {
            visible: root.popular.length > 0
            width: parent.width
            spacing: 8
            SectionHeader { width: parent.width; title: qsTr("Populares") }
            Repeater {
                model: root.showAllPopular ? root.popular : root.popular.slice(0, 5)
                TrackRow {
                    id: prow
                    required property var modelData
                    required property int index
                    width: parent.width
                    number: index + 1
                    title: modelData.name
                    artists: modelData.artistsText
                    album: modelData.album
                    coverUrl: modelData.coverUrl
                    durationMs: modelData.durationMs
                    explicitContent: !!modelData.explicit
                    showAlbum: false
                    isCurrent: App.player.trackUri === modelData.uri
                    isPlaying: App.player.isPlaying
                    onPlay: App.player.playUris(root.popular.map(t => t.uri), index)
                    onArtistClicked: if (modelData.artists[0].id !== root.artistId) Nav.go("artist", { id: modelData.artists[0].id })
                    onAlbumClicked: Nav.go("album", { id: modelData.albumId })
                    onMenuRequested: (mx, my) => Nav.trackMenuRequested(
                        { uri: modelData.uri, name: modelData.name, albumId: modelData.albumId, artists: modelData.artists }, prow, mx, my)
                }
            }
            Label {
                visible: root.popular.length > 5
                leftPadding: 16
                topPadding: 8
                text: root.showAllPopular ? qsTr("Mostrar menos") : qsTr("Ver mais")
                color: Theme.textSecondary
                font.pixelSize: 14
                font.weight: Font.Bold
                font.underline: moreMouse.containsMouse
                MouseArea {
                    id: moreMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.showAllPopular = !root.showAllPopular
                }
            }
        }

        // Discografia
        Column {
            visible: root.releases.length > 0
            width: parent.width
            spacing: 12
            SectionHeader { width: parent.width; title: qsTr("Discografia") }
            Row {
                spacing: 8
                Chip { text: qsTr("Álbuns"); checked: root.releaseFilter === "album"; onClicked: root.releaseFilter = "album" }
                Chip { text: qsTr("Singles e EPs"); checked: root.releaseFilter === "single"; onClicked: root.releaseFilter = "single" }
                Chip {
                    visible: root.releases.some(r => r.type === "compilation")
                    text: qsTr("Compilações"); checked: root.releaseFilter === "compilation"; onClicked: root.releaseFilter = "compilation"
                }
            }
            HorizontalShelf {
                width: parent.width
                title: ""
                showAll: root.showAllReleases
                model: root.releases.filter(r => r.type === root.releaseFilter)
                onShowAllClicked: { root.showAllReleases = true; root.loadMoreReleases() }
                onItemClicked: (c) => Nav.go("album", { id: c.id })
                onItemPlayClicked: (c) => App.player.playContext(c.uri)
            }
        }
    }
}
