import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: editor
    required property var player
    required property var tone
    property bool editing: false
    property bool applying: false
    property int markedCount: 0
    property int dragFrom: -1
    property int dragIndex: -1
    property real dragY: 0
    property string loadedToken: ""
    readonly property bool partial: !!(player && player.queuePartial)
    readonly property bool available: visible && !partial && loadedToken !== ""
    readonly property int rowHeight: 56
    // 0.18.1: public plays (CatalogStats) and Web API for artist metadata; both optional.
    property var catalog: null
    property var api: null
    property string sortMode: "original"
    property bool sortDescending: false
    property bool sorting: false
    property string sortStatus: ""
    property var beforeSort: null
    property var artistMeta: ({})
    property var waitingAlbums: []
    property var pendingSort: null
    readonly property var sortModes: [
        {id:"original",label:qsTr("Ordem anterior")},{id:"title",label:qsTr("Título A–Z")},
        {id:"duration",label:qsTr("Duração")},{id:"artist",label:qsTr("Artista A–Z")},
        {id:"album",label:qsTr("Álbum A–Z")},{id:"genre",label:qsTr("Gênero A–Z")},
        {id:"plays",label:qsTr("Mais tocadas (Spotify)")},{id:"followers",label:qsTr("Seguidores do artista")}
    ]
    function sortLabel(mode, descending) {
        const m = sortModes.find(x => x.id === mode)
        if (!m || mode === "original") return qsTr("Ordenar")
        const text = ["title","artist","album","genre"].indexOf(mode) >= 0
        return m.label.replace(" A–Z", "") + (text ? (descending ? " Z–A" : " A–Z") : (descending ? " ↓" : " ↑"))
    }
    function playsOf(uri) { return catalog ? catalog.plays(uri) : -1 }
    function sortValue(e, mode) {
        switch (mode) {
        case "title": return e.queueName
        case "artist": return e.artist
        case "album": return e.album
        case "genre": return (artistMeta[e.artistId] || {}).genre || ""
        case "duration": return e.durationMs > 0 ? e.durationMs : -1
        case "plays": return playsOf(e.uri)
        case "followers": { const f = (artistMeta[e.artistId] || {}).followers; return f === undefined ? -1 : f }
        }
        return 0
    }
    function compareEntries(a, b, mode, descending) {
        const av = sortValue(a, mode), bv = sortValue(b, mode)
        if (typeof av === "string") {
            const x = av.trim(), y = bv.trim()
            if (!x !== !y) return x ? -1 : 1
            const c = x.localeCompare(y, "pt-BR", {sensitivity: "base"})
            return (descending ? -c : c) || a.position - b.position
        }
        if ((av < 0) !== (bv < 0)) return av < 0 ? 1 : -1
        return (descending ? bv - av : av - bv) || a.position - b.position
    }
    function rows() {
        const out = []
        for (let i = 0; i < entries.count; ++i) out.push(Object.assign({position: i}, JSON.parse(JSON.stringify(entries.get(i)))))
        return out
    }
    function replaceRows(list) {
        entries.clear()
        for (const r of list) { delete r.position; entries.append(r) }
        saveOrder()
    }
    function chooseSort(mode) {
        if (!available || entries.count < 2 || sorting) return
        if (mode === "original") {
            if (!beforeSort) { sortStatus = qsTr("A fila já está na ordem anterior à ordenação."); return }
            const pool = rows(), restored = []
            for (const uri of beforeSort) { const k = pool.findIndex(r => r.uri === uri); if (k >= 0) restored.push(pool.splice(k, 1)[0]) }
            sortMode = "original"; beforeSort = null; sortStatus = ""
            replaceRows(restored.concat(pool))
            return
        }
        if (sortMode === mode) sortDescending = !sortDescending
        else { sortMode = mode; sortDescending = ["plays","followers"].indexOf(mode) >= 0 }
        pendingSort = {mode: sortMode, descending: sortDescending, token: loadedToken}
        sorting = true
        if (mode === "plays" && catalog) {
            const ids = [...new Set(rows().map(r => r.albumId).filter(id => id))]
            waitingAlbums = ids
            catalog.requestAlbums(ids)
            checkPlays()
        } else if ((mode === "genre" || mode === "followers") && api) {
            loadArtists([...new Set(rows().map(r => r.artistId).filter(id => id && !artistMeta[id]))], 0)
        } else finishSort()
    }
    function checkPlays() {
        if (!pendingSort || pendingSort.mode !== "plays") return
        let known = 0
        for (const id of waitingAlbums) if (catalog.albumKnown(id)) known++
        if (known >= waitingAlbums.length || catalog.pendingAlbums() === 0) finishSort()
        else sortStatus = qsTr("Carregando plays do Spotify: %1/%2 álbuns…").arg(known).arg(waitingAlbums.length)
    }
    function loadArtists(ids, offset) {
        if (offset >= ids.length) { finishSort(); return }
        sortStatus = qsTr("Carregando gêneros e seguidores…")
        const token = loadedToken
        api.call("GET", "/artists", {ids: ids.slice(offset, offset + 50).join(",")}, null, function(status, data) {
            if (token !== editor.loadedToken || !editor.pendingSort) return
            if (status === 200 && data) {
                const meta = Object.assign({}, editor.artistMeta)
                for (const a of data.artists || []) if (a) meta[a.id] = {genre: (a.genres || []).join(", "), followers: a.followers && a.followers.total !== undefined ? a.followers.total : -1}
                editor.artistMeta = meta
                editor.loadArtists(ids, offset + 50)
            } else { editor.sortStatus = qsTr("Dados de artistas incompletos; itens sem dado ficam no fim."); editor.finishSort() }
        })
    }
    function finishSort() {
        const job = pendingSort; pendingSort = null; sorting = false
        if (!job || job.token !== loadedToken || !available) { sortStatus = qsTr("A fila mudou durante a ordenação."); return }
        const list = rows()
        if (!beforeSort) beforeSort = list.map(r => r.uri)
        list.sort((a, b) => compareEntries(a, b, job.mode, job.descending))
        replaceRows(list)
        sortStatus = qsTr("Fila ordenada: %1").arg(sortLabel(job.mode, job.descending)) + (job.mode === "plays" ? qsTr(" · sem dado público no fim") : "")
    }
    Connections {
        target: editor.catalog
        ignoreUnknownSignals: true
        function onPlaysChanged() { editor.checkPlays() }
    }

    function reload() {
        dragFrom = dragIndex = -1
        entries.clear()
        loadedToken = player && player.queueEditToken || ""
        const tracks = player && player.queue || []
        for (let i = 0; i < tracks.length; ++i) {
            const t = tracks[i]
            entries.append({ordinal:i, marked:false, queueName:t.name || "", artist:t.artists || "",
                            queueCover:t.coverUrl || "", external:!!t.mixExternal, uri:t.uri || "",
                            album:t.album || "", albumId:t.albumId || "", artistId:t.artistId || "", durationMs:t.durationMs || 0})
        }
        if (catalog && tracks.length) catalog.requestAlbums([...new Set(tracks.map(t => t.albumId).filter(id => id))])
        markedCount = 0
        if (!tracks.length || partial) editing = false
    }

    function summary(tracks) {
        const albums = new Set()
        const artists = new Set()
        let ms = 0, missing = false
        for (const t of tracks) {
            const album = (t.album || "").trim().toLowerCase()
            const names = t.artistCredits && t.artistCredits.length ? t.artistCredits.map(a => a.name) : [t.artists]
            for (const name of names) {
                if (name && name.trim()) artists.add(name.trim().toLowerCase())
            }
            if (t.albumId || album) albums.add(t.albumId || album + "|" + (t.artists || "").toLowerCase())
            if (t.durationMs > 0) ms += t.durationMs
            else missing = true
        }
        const sec = Math.floor(ms / 1000)
        const min = Math.floor(sec / 60)
        const time = min >= 60 ? Math.floor(min / 60) + "h " + min % 60 + "min"
                               : min + "min " + sec % 60 + "s"
        return qsTr("%1 mús. · %2 álb. · %3 art. · %4").arg(tracks.length + (player && player.hasTrack ? 1 : 0)).arg(albums.size).arg(artists.size)
            .arg(missing ? (ms ? "≥ " + time : "—") : time)
    }

    function toggleMarked(index) {
        const on = !entries.get(index).marked
        entries.setProperty(index, "marked", on)
        markedCount += on ? 1 : -1
    }
    function stopEditing() {
        for (let i = 0; i < entries.count; ++i) {
            if (entries.get(i).marked) entries.setProperty(i, "marked", false)
        }
        markedCount = 0
        editing = false
    }
    function saveOrder() {
        const order = []
        for (let i = 0; i < entries.count; ++i) order.push(entries.get(i).ordinal)
        applying = true
        const accepted = player.editQueue(loadedToken, order)
        applying = false
        if (!accepted) {
            reload()
            return
        }
        loadedToken = player.queueEditToken
        for (let i = 0; i < entries.count; ++i) entries.setProperty(i, "ordinal", i)
        if (!entries.count) stopEditing()
    }
    function moveMarked(step) {
        let changed = false
        for (let i = step < 0 ? 1 : entries.count - 2; i >= 0 && i < entries.count; i -= step) {
            if (entries.get(i).marked && !entries.get(i + step).marked) {
                entries.move(i, i + step, 1)
                changed = true
            }
        }
        if (changed) saveOrder()
    }
    function removeMarked() {
        for (let i = entries.count - 1; i >= 0; --i) {
            if (entries.get(i).marked) entries.remove(i)
        }
        markedCount = 0
        saveOrder()
    }
    function dragTarget() {
        if (dragIndex < 0 || entries.count < 2) return
        const to = Math.max(0, Math.min(entries.count - 1,
            Math.floor((list.contentY - list.originY + dragY) / rowHeight)))
        if (to === dragIndex) return
        entries.move(dragIndex, to, 1)
        dragIndex = to
    }
    function finishDrag(canceled) {
        const from = dragFrom, to = dragIndex
        dragFrom = dragIndex = -1
        if (from < 0 || from === to) return
        if (canceled) entries.move(to, from, 1)
        else saveOrder()
    }

    onVisibleChanged: {
        if (visible) reload()
        else { finishDrag(true); stopEditing() }
    }
    Component.onCompleted: reload()
    Connections {
        target: editor.player
        ignoreUnknownSignals: true
        function onQueueChanged() { if (!editor.applying) editor.reload() }
        function onTrackChanged() { if (!editor.applying) editor.reload() }
        function onStateChanged() {
            if (!editor.applying && (editor.player.queueEditToken || "") !== editor.loadedToken)
                editor.reload()
        }
    }

    ListModel { id: entries }
    Timer {
        interval: 24
        repeat: true
        running: editor.dragIndex >= 0 && editor.visible
        onTriggered: {
            const edge = 38
            const step = editor.dragY < edge ? -10 : editor.dragY > list.height - edge ? 10 : 0
            if (!step) return
            list.contentY = Math.max(list.originY, Math.min(
                list.originY + Math.max(0, list.contentHeight - list.height), list.contentY + step))
            editor.dragTarget()
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: editor.width < 480 ? 3 : 4
        Label {
            objectName: "queueSummary"
            Layout.fillWidth: true
            Layout.columnSpan: parent.columns === 3 ? 3 : 1
            text: editor.summary(editor.player && editor.player.queue || [])
            color: editor.tone.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Button {
            text: qsTr("Limpar")
            Layout.fillWidth: parent.columns === 3
            enabled: editor.available && entries.count > 0 && editor.dragIndex < 0
            onClicked: {
                confirm.token = editor.loadedToken
                confirm.open()
            }
        }
        Button {
            text: editor.editing ? qsTr("Concluir") : qsTr("Editar")
            Layout.fillWidth: parent.columns === 3
            enabled: editor.available && entries.count > 0 && editor.dragIndex < 0
            onClicked: { if (editor.editing) editor.stopEditing(); else editor.editing = true }
        }
        Button {
            id: sortButton
            objectName: "queueSort"
            text: editor.sorting ? qsTr("↑↓ Ordenando…") : "↑↓ " + editor.sortLabel(editor.sortMode, editor.sortDescending)
            Layout.fillWidth: parent.columns === 3
            enabled: editor.available && entries.count > 1 && editor.dragIndex < 0 && !editor.editing && !editor.sorting
            onClicked: sortMenu.popup(sortButton, 0, sortButton.height)
            Menu {
                id: sortMenu
                Repeater {
                    model: editor.sortModes
                    MenuItem {
                        required property var modelData
                        text: modelData.id === editor.sortMode && modelData.id !== "original"
                              ? editor.sortLabel(modelData.id, editor.sortDescending) + qsTr("  (clique para inverter)") : modelData.label
                        checkable: true
                        checked: modelData.id === editor.sortMode
                        onTriggered: editor.chooseSort(modelData.id)
                    }
                }
            }
        }
    }
    Label {
        Layout.fillWidth: true
        visible: editor.sortStatus !== ""
        text: editor.sortStatus
        color: editor.tone.textSecondary
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    QueueTrack {
        Layout.fillWidth: true
        visible: !!editor.player && !!editor.player.hasTrack
        name: editor.player ? editor.player.trackName || "" : ""
        byline: editor.player ? editor.player.artistsText || "" : ""
        cover: editor.player ? editor.player.coverUrl || "" : ""
        highlighted: true
        current: true
        canPlay: editor.available && !editor.editing && editor.dragIndex < 0
        onPlayRequested: editor.player.playQueueEntry(editor.loadedToken, -1)
    }

    Label {
        Layout.fillWidth: true
        visible: editor.partial
        color: editor.tone.textSecondary
        text: qsTr("Fila parcial. A edição requer a ordem completa.")
        wrapMode: Text.WordWrap
    }

    Label {
        Layout.fillWidth: true
        visible: entries.count === 0
        color: editor.tone.textSecondary
        text: editor.available ? qsTr("Nenhuma próxima música.") : qsTr("Fila indisponível para edição.")
        wrapMode: Text.WordWrap
    }

    ListView {
        id: list
        objectName: "editableQueueList"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: entries
        moveDisplaced: Transition { NumberAnimation { property: "y"; duration: 145; easing.type: Easing.OutQuad } }
        move: Transition { NumberAnimation { property: "y"; duration: 95; easing.type: Easing.OutQuad } }
        removeDisplaced: Transition { NumberAnimation { property: "y"; duration: 145; easing.type: Easing.OutQuad } }
        ScrollBar.vertical: ScrollBar {}
        delegate: QueueTrack {
            id: row
            required property int index
            required property bool marked
            required property string artist
            required property string queueName
            required property string queueCover
            required property bool external
            required property string uri
            width: list.width
            name: queueName
            cover: queueCover
            byline: {
                const revision = editor.catalog ? editor.catalog.revision : 0
                const plays = editor.playsOf(uri)
                return artist + (external ? qsTr(" · Descoberta") : "") + (plays >= 0 ? " · " + editor.catalog.compact(plays) + qsTr(" plays") : "")
            }
            highlighted: marked || editor.dragIndex === index
            z: editor.dragIndex === index ? 1 : 0
            selectable: editor.editing
            selected: marked
            canDrag: editor.available && entries.count > 1
            canPlay: editor.available && !editor.editing && editor.dragIndex < 0
            onPlayRequested: editor.player.playQueueEntry(editor.loadedToken, index)
            onMark: editor.toggleMarked(index)
            onDragMove: (sceneX, sceneY) => {
                if (editor.dragIndex < 0) editor.dragFrom = editor.dragIndex = index
                editor.dragY = list.mapFromItem(null, sceneX, sceneY).y
                editor.dragTarget()
            }
            onDragFinish: canceled => editor.finishDrag(canceled)
        }
    }

    RowLayout {
        objectName: "queueBatchActions"
        visible: editor.editing
        Layout.fillWidth: true
        Button { Layout.fillWidth: true; text: qsTr("↑ Subir"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.moveMarked(-1) }
        Button { Layout.fillWidth: true; text: qsTr("↓ Descer"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.moveMarked(1) }
        Button { Layout.fillWidth: true; text: qsTr("Excluir"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.removeMarked() }
    }

    Dialog {
        id: confirm
        property string token
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Limpar fila?")
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label { text: qsTr("Remove todas as próximas músicas."); padding: 12 }
        onAccepted: {
            editor.player.editQueue(token, [])
            editor.reload()
        }
    }

    component QueueTrack: Rectangle {
        id: track
        property string name: ""
        property string byline: ""
        property string cover: ""
        property bool highlighted: false
        property bool current: false
        property bool selectable: false
        property bool selected: false
        property bool canDrag: false
        property bool canPlay: false
        signal playRequested()
        signal mark()
        signal dragMove(real x, real y)
        signal dragFinish(bool canceled)

        implicitHeight: editor.rowHeight
        height: implicitHeight
        radius: 6
        color: highlighted ? editor.tone.accentDeep : "transparent"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 10
            spacing: 8
            CheckBox {
                visible: track.selectable
                checked: track.selected
                Accessible.name: track.name
                onClicked: track.mark()
            }
            Rectangle {
                Layout.preferredWidth: 38
                Layout.preferredHeight: 38
                color: editor.tone.card
                radius: 4
                Label { anchors.centerIn: parent; text: "—"; color: editor.tone.textSecondary }
                Image { anchors.fill: parent; source: track.cover; asynchronous: true; fillMode: Image.PreserveAspectCrop; sourceSize: Qt.size(76,76) }
            }
            ColumnLayout {
                objectName: "queueTrackLabels"
                Layout.fillWidth: true
                spacing: 2
                Label {
                    Layout.fillWidth: true
                    text: (track.current ? qsTr("Agora · ") : "") + track.name
                    color: track.current ? editor.tone.accentText : editor.tone.text
                    font.bold: true
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
                Label { Layout.fillWidth: true; text: track.byline; color: editor.tone.textSecondary; elide: Text.ElideRight; textFormat: Text.PlainText }
                MouseArea {
                    anchors.fill: parent
                    enabled: track.canPlay
                    onDoubleClicked: track.playRequested()
                }
            }
            Label {
                visible: track.canDrag
                Layout.preferredWidth: 34
                Layout.fillHeight: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: "≡"
                color: editor.tone.textSecondary
                font.pixelSize: 22
                Accessible.name: qsTr("Arrastar %1").arg(track.name)
                MouseArea {
                    objectName: "queueDragHandle"
                    anchors.fill: parent
                    preventStealing: true
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    property real startY
                    property bool moved
                    onPressed: e => { startY = e.y; moved = false }
                    onPositionChanged: e => {
                        if (!pressed || (!moved && Math.abs(e.y - startY) < 5)) return
                        moved = true
                        const p = mapToItem(null, e.x, e.y)
                        track.dragMove(p.x, p.y)
                    }
                    onReleased: { if (moved) track.dragFinish(false) }
                    onCanceled: { if (moved) track.dragFinish(true) }
                }
            }
        }
    }
}
