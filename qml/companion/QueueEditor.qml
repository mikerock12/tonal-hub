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
    readonly property bool available: visible && loadedToken !== ""
    readonly property int rowHeight: 56

    function reload() {
        dragFrom = dragIndex = -1
        entries.clear()
        loadedToken = player && player.queueEditToken || ""
        const tracks = player && player.queue || []
        for (let i = 0; i < tracks.length; ++i) {
            const t = tracks[i]
            entries.append({ordinal:i, marked:false, queueName:t.name || "", artist:t.artists || "",
                            queueCover:t.coverUrl || "", external:!!t.mixExternal})
        }
        markedCount = 0
        if (!tracks.length) editing = false
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
        return qsTr("%1 mús. · %2 álb. · %3 art. · %4").arg(tracks.length).arg(albums.size).arg(artists.size)
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

    RowLayout {
        Layout.fillWidth: true
        Label {
            objectName: "queueSummary"
            Layout.fillWidth: true
            text: editor.summary(editor.player && editor.player.queue || [])
            color: editor.tone.textSecondary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Button {
            text: qsTr("Limpar")
            enabled: editor.available && entries.count > 0 && editor.dragIndex < 0
            onClicked: {
                confirm.token = editor.loadedToken
                confirm.open()
            }
        }
        Button {
            text: editor.editing ? qsTr("Concluir") : qsTr("Editar")
            enabled: editor.available && entries.count > 0 && editor.dragIndex < 0
            onClicked: { if (editor.editing) editor.stopEditing(); else editor.editing = true }
        }
    }

    QueueTrack {
        Layout.fillWidth: true
        visible: !!editor.player && !!editor.player.hasTrack
        name: editor.player ? editor.player.trackName || "" : ""
        byline: editor.player ? editor.player.artistsText || "" : ""
        cover: editor.player ? editor.player.coverUrl || "" : ""
        highlighted: true
        current: true
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
            width: list.width
            name: queueName
            cover: queueCover
            byline: artist + (external ? qsTr(" · Descoberta") : "")
            highlighted: marked || editor.dragIndex === index
            z: editor.dragIndex === index ? 1 : 0
            selectable: editor.editing
            selected: marked
            canDrag: editor.available && entries.count > 1
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
        visible: editor.editing
        Layout.fillWidth: true
        Button { text: qsTr("↑ Subir"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.moveMarked(-1) }
        Button { text: qsTr("↓ Descer"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.moveMarked(1) }
        Button { text: qsTr("Excluir"); enabled: editor.markedCount > 0 && editor.dragIndex < 0; onClicked: editor.removeMarked() }
        Item { Layout.fillWidth: true }
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
