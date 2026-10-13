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
    property bool owned: false          // aceita músicas: sua ou colaborativa
    property bool isOwner: false        // só o dono muda nome e descrição e exclui
    property string snapshotId: ""

    // Arraste de faixa (playlist sua): a linha muda de lugar no modelo enquanto anda, e só ao soltar vai
    // um PUT com a posição final. Mover no modelo mantém a linha arrastada na tela: a lista recicla as linhas
    // que saem dela (reuseItems) e a linha perderia o mouse.
    property int dragFrom: -1        // posição original
    property int dragIndex: -1       // posição atual
    property real dragY: 0           // ponteiro, em coordenadas da lista
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
        isOwner = false; snapshotId = ""
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
            root.isOwner = !!(Nav.me && p.owner && p.owner.id === Nav.me.id)
            root.owned = root.isOwner || !!p.collaborative
            root.snapshotId = p.snapshot_id || ""
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

    // ------------------------------------------------------------ edição (playlists suas)
    function saveDetails(name, description) {
        const id = itemId
        Playlists.update(id, { name: name, description: description }, function(done) {
            if (!done || id !== root.itemId) return
            root.title = name
            root.description = description
        })
    }

    function deletePlaylist() {
        Playlists.remove(uri, function(done) {
            if (!done) return
            if (Nav.canBack) Nav.back()
            else Nav.go("home")
        })
    }

    // A API tira todas as ocorrências da faixa; a lista faz o mesmo, sem recarregar a página.
    function removeTrack(trackUri) {
        const id = itemId
        Playlists.removeTrack(id, trackUri, snapshotId, function(done, snapshot) {
            if (!done || id !== root.itemId) return
            if (snapshot) root.snapshotId = snapshot
            let removed = 0
            for (let i = trackModel.count - 1; i >= 0; --i) {
                const row = trackModel.get(i)
                if (row.uri !== trackUri) continue
                root.totalDurationMs -= row.durationMs
                trackModel.remove(i)
                ++removed
            }
            if (root.totalTracks >= removed) root.totalTracks -= removed
            list.currentIndex = -1
        })
    }

    function dragStart(index) {
        dragFrom = index
        dragIndex = index
        list.currentIndex = -1
    }

    function dragMove(sceneX, sceneY) {
        dragY = list.mapFromItem(null, sceneX, sceneY).y
        updateDragTarget()
    }

    function updateDragTarget() {
        if (dragIndex < 0) return
        const y = list.contentY + dragY
        let target = list.indexAt(list.width / 2, y)
        if (target < 0) {
            // Fora das linhas: abaixo da última (rodapé) ou acima da primeira (cabeçalho).
            const last = list.itemAtIndex(trackModel.count - 1)
            target = last && y > last.y ? trackModel.count - 1 : 0
        }
        if (target === dragIndex) return
        trackModel.move(dragIndex, target, 1)
        dragIndex = target
    }

    function dragEnd(canceled) {
        const from = dragFrom
        const to = dragIndex
        dragFrom = -1
        dragIndex = -1
        if (from < 0 || to === from) return
        if (canceled) {
            trackModel.move(to, from, 1)
            return
        }
        const id = itemId
        // insert_before conta as posições de antes da mudança: descendo, é a posição seguinte à de destino.
        Playlists.moveTrack(id, from, to > from ? to + 1 : to, snapshotId, function(done, snapshot) {
            if (id !== root.itemId) return
            if (!done) root.load() // a ordem local não vale mais: recarrega a do Spotify
            else if (snapshot) root.snapshotId = snapshot
        })
    }

    // Rola a lista enquanto o ponteiro arrastado está perto da borda de cima ou de baixo.
    Timer {
        interval: 16
        repeat: true
        running: root.dragIndex >= 0
        onTriggered: {
            const top = stickyBar.opacity > 0.5 ? 144 : 48   // a barra fixa e as colunas cobrem o topo
            const bottom = list.height - 48
            let dy = 0
            if (root.dragY < top) dy = -Math.ceil((top - root.dragY) / 4)
            else if (root.dragY > bottom) dy = Math.ceil((root.dragY - bottom) / 4)
            if (dy === 0) return
            const maxY = list.originY + list.contentHeight - list.height
            list.contentY = Math.max(list.originY, Math.min(maxY, list.contentY + dy))
            root.updateDragTarget()
        }
    }

    Connections {
        target: Playlists
        // Música adicionada a esta playlist pelo menu de faixa, de qualquer tela.
        function onContentChanged(playlistId) {
            if (root.kind === "playlist" && playlistId === root.itemId) root.load()
        }
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
        moveDisplaced: Transition { NumberAnimation { property: "y"; duration: 120; easing.type: Easing.OutQuad } }

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
                        MouseArea {
                            anchors.fill: parent
                            enabled: root.isOwner
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: detailsDialog.openFor(root.title, root.description)
                        }
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
                        id: moreButton
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.uri !== ""
                        iconSize: 28
                        fillPath: Icons.dotsF
                        tooltip: qsTr("Mais opções")
                        onClicked: pageMenu.popup(moreButton, 0, moreButton.height)
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
            draggable: root.kind === "playlist" && root.owned
            dragging: root.dragIndex === index
            onDragStarted: root.dragStart(index)
            onDragMoved: (sx, sy) => root.dragMove(sx, sy)
            onDragEnded: (canceled) => root.dragEnd(canceled)
            onPlay: root.playFrom(index)
            onArtistClicked: if (artistId) Nav.go("artist", { id: artistId })
            onAlbumClicked: if (albumId) Nav.go("album", { id: albumId })
            onMenuRequested: (mx, my) => {
                const track = { uri: uri, name: name, albumId: albumId, artists: JSON.parse(artistsJson) }
                if (root.kind === "playlist" && root.owned) {
                    const trackUri = uri
                    track.removeFromPlaylist = function() { root.removeTrack(trackUri) }
                }
                Nav.trackMenuRequested(track, row, mx, my)
            }
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
                visible: root.kind === "playlist" && root.owned && root.contentsAvailable && !root.loading
                         && trackModel.count === 0
                x: 24
                width: parent.width - 48
                wrapMode: Text.WordWrap
                text: qsTr("Sua playlist está vazia. Para pôr músicas nela, use “Adicionar à playlist” no menu (⋯) de qualquer música.")
                color: Theme.textSecondary
                font.pixelSize: 14
            }
            Label {
                visible: root.kind === "album" && root.releaseDate !== ""
                x: 24
                text: {
                    const d = new Date(root.releaseDate + "T12:00:00")
                    if (isNaN(d.getTime()) || root.releaseDate.length < 10) return root.releaseDate
                    return Theme.formatDate(d, true)
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

    // ------------------------------------------------------------ menu ⋯ e diálogos
    DarkMenu {
        id: pageMenu

        DarkMenuItem {
            text: qsTr("Editar detalhes")
            visible: root.isOwner
            height: visible ? implicitHeight : 0
            onTriggered: detailsDialog.openFor(root.title, root.description)
        }
        DarkMenuItem {
            text: qsTr("Excluir")
            visible: root.isOwner
            height: visible ? implicitHeight : 0
            onTriggered: deleteDialog.open()
        }
        DarkMenuItem {
            text: qsTr("Copiar link")
            onTriggered: {
                App.copyToClipboard("https://open.spotify.com/" + root.kind + "/" + root.itemId)
                Nav.toast(qsTr("Link copiado"))
            }
        }
    }

    ModalDialog {
        id: detailsDialog
        title: qsTr("Editar detalhes")

        function openFor(name, description) {
            nameField.text = name
            descriptionField.text = description
            open()
            nameField.forceActiveFocus()
            nameField.selectAll()
        }
        function save() {
            const name = nameField.text.trim()
            if (name === "") return
            root.saveDetails(name, descriptionField.text.trim())
            close()
        }

        TextField {
            id: nameField
            width: parent.width
            height: 44
            leftPadding: 12
            placeholderText: qsTr("Nome da playlist")
            placeholderTextColor: Theme.textMuted
            color: Theme.textPrimary
            font.pixelSize: 15
            maximumLength: 100
            selectByMouse: true
            onAccepted: detailsDialog.save()
            background: Rectangle {
                radius: 4
                color: Qt.rgba(1, 1, 1, 0.07)
                border.width: nameField.activeFocus ? 1 : 0
                border.color: Theme.textMuted
            }
        }
        TextArea {
            id: descriptionField
            width: parent.width
            height: 112
            padding: 12
            placeholderText: qsTr("Adicione uma descrição (opcional)")
            placeholderTextColor: Theme.textMuted
            color: Theme.textPrimary
            font.pixelSize: 14
            wrapMode: TextArea.Wrap
            selectByMouse: true
            background: Rectangle {
                radius: 4
                color: Qt.rgba(1, 1, 1, 0.07)
                border.width: descriptionField.activeFocus ? 1 : 0
                border.color: Theme.textMuted
            }
        }

        buttons: [
            PillButton {
                primary: false
                text: qsTr("Cancelar")
                onClicked: detailsDialog.close()
            },
            PillButton {
                text: qsTr("Salvar")
                enabled: nameField.text.trim() !== ""
                onClicked: detailsDialog.save()
            }
        ]
    }

    ModalDialog {
        id: deleteDialog
        title: qsTr("Excluir de Sua Biblioteca?")

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            textFormat: Text.StyledText
            color: Theme.textSecondary
            font.pixelSize: 14
            text: qsTr("Isso vai excluir <b>%1</b> de Sua Biblioteca.").arg(root.title.replace(/&/g, "&amp;").replace(/</g, "&lt;"))
        }

        buttons: [
            PillButton {
                primary: false
                text: qsTr("Cancelar")
                onClicked: deleteDialog.close()
            },
            PillButton {
                text: qsTr("Excluir")
                onClicked: {
                    deleteDialog.close()
                    root.deletePlaylist()
                }
            }
        ]
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
