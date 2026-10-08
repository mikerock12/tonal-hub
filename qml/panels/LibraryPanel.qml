import QtQuick
import QtQuick.Controls

// "Sua Biblioteca": playlists, álbuns salvos e artistas seguidos.
Rectangle {
    id: root

    property bool collapsed: false
    signal toggleCollapse()

    property string filter: ""        // "" | playlist | album | artist
    property string textFilter: ""
    property var playlists: []
    property var albums: []
    property var artists: []
    property bool loading: false

    color: Theme.panel
    radius: Theme.radiusPanel
    clip: true

    function load() {
        if (!App.auth.authenticated) return
        loading = true
        playlists = []; albums = []; artists = []
        fetchPlaylists("/me/playlists?limit=50", [])
        App.api.call("GET", "/me/albums", { limit: 50 }, null, function(status, data) {
            if (status !== 200) return
            root.albums = data.items.map(function(it) {
                const a = it.album
                return { kind: "album", itemId: a.id, uri: a.uri, title: a.name,
                         subtitle: qsTr("Álbum") + " • " + Theme.joinNames(a.artists),
                         imageUrl: Theme.bestImage(a.images, 64), round: false }
            })
            root.rebuild()
        })
        App.api.call("GET", "/me/following", { type: "artist", limit: 50 }, null, function(status, data) {
            if (status !== 200) return
            root.artists = data.artists.items.map(function(a) {
                return { kind: "artist", itemId: a.id, uri: a.uri, title: a.name, subtitle: qsTr("Artista"),
                         imageUrl: Theme.bestImage(a.images, 64), round: true }
            })
            root.rebuild()
        })
    }

    function fetchPlaylists(url, acc) {
        App.api.call("GET", url, {}, null, function(status, data) {
            if (status !== 200) { root.loading = false; return }
            const list = acc.concat(data.items.filter(p => p !== null).map(function(p) {
                const owner = p.owner ? (p.owner.display_name || p.owner.id) : ""
                return { kind: "playlist", itemId: p.id, uri: p.uri, title: p.name,
                         subtitle: qsTr("Playlist") + (owner ? " • " + owner : ""),
                         imageUrl: Theme.bestImage(p.images, 64), round: false }
            }))
            root.playlists = list
            root.rebuild()
            if (data.next && list.length < 500)
                root.fetchPlaylists(data.next, list)
            else
                root.loading = false
        })
    }

    function rebuild() {
        let items = []
        if (filter === "" || filter === "playlist") {
            items.push({ kind: "liked", itemId: "", uri: Nav.likedSongsUri(), title: qsTr("Músicas Curtidas"),
                         subtitle: qsTr("Playlist"), imageUrl: "", round: false })
            items = items.concat(playlists)
        }
        if (filter === "" || filter === "album") items = items.concat(albums)
        if (filter === "" || filter === "artist") items = items.concat(artists)
        if (textFilter !== "") {
            const t = textFilter.toLowerCase()
            items = items.filter(i => i.title.toLowerCase().indexOf(t) >= 0)
        }
        libraryModel.clear()
        for (let i = 0; i < items.length; ++i)
            libraryModel.append(items[i])
    }

    onFilterChanged: rebuild()
    onTextFilterChanged: rebuild()
    Component.onCompleted: load()
    Connections {
        target: App.auth
        function onAuthenticatedChanged() { root.load() }
    }
    Connections {
        target: Nav
        function onMeChanged() { root.rebuild() }
    }

    ListModel { id: libraryModel }

    // ---------------------------------------------------- cabeçalho
    Column {
        id: header
        width: parent.width
        topPadding: 12
        spacing: 8

        Item {
            width: parent.width
            height: 40

            Row {
                anchors.left: parent.left
                anchors.leftMargin: root.collapsed ? (root.width - 32) / 2 : 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12
                Icon {
                    size: 24
                    strokePath: Icons.libraryS
                    color: headerHit.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    visible: !root.collapsed
                    text: qsTr("Sua Biblioteca")
                    color: headerHit.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: headerHit
                anchors.left: parent.left
                width: root.collapsed ? parent.width : 190
                height: parent.height
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.toggleCollapse()
                ToolTip.visible: containsMouse
                ToolTip.text: root.collapsed ? qsTr("Expandir Sua Biblioteca") : qsTr("Recolher Sua Biblioteca")
                ToolTip.delay: 700
            }

            Row {
                visible: !root.collapsed
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                IconButton {
                    iconSize: 16
                    strokePath: Icons.searchS
                    tooltip: qsTr("Buscar em Sua Biblioteca")
                    active: searchBox.visible
                    onClicked: {
                        searchBox.visible = !searchBox.visible
                        if (searchBox.visible) searchField.forceActiveFocus()
                        else root.textFilter = ""
                    }
                }
                IconButton {
                    iconSize: 16
                    strokePath: Icons.refreshS
                    tooltip: qsTr("Atualizar")
                    onClicked: root.load()
                }
            }
        }

        Flow {
            visible: !root.collapsed
            width: parent.width - 32
            x: 16
            spacing: 8
            Chip { text: qsTr("Playlists"); checked: root.filter === "playlist"; closable: checked; onClicked: root.filter = checked ? "" : "playlist" }
            Chip { text: qsTr("Álbuns"); checked: root.filter === "album"; closable: checked; onClicked: root.filter = checked ? "" : "album" }
            Chip { text: qsTr("Artistas"); checked: root.filter === "artist"; closable: checked; onClicked: root.filter = checked ? "" : "artist" }
        }

        Item {
            id: searchBox
            visible: false
            width: parent.width - 32
            x: 16
            height: 36
            TextField {
                id: searchField
                anchors.fill: parent
                leftPadding: 12
                placeholderText: qsTr("Buscar em Sua Biblioteca")
                placeholderTextColor: Theme.textMuted
                color: Theme.textPrimary
                font.pixelSize: 14
                background: Rectangle { radius: 4; color: Theme.elevated }
                onTextChanged: root.textFilter = text
                Keys.onEscapePressed: { text = ""; searchBox.visible = false }
            }
        }
    }

    // ---------------------------------------------------- lista
    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        clip: true
        model: libraryModel
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        ScrollBar.vertical: ThinScrollBar { }

        delegate: Item {
            id: row
            required property string kind
            required property string itemId
            required property string uri
            required property string title
            required property string subtitle
            required property string imageUrl
            required property bool round

            readonly property bool isCurrent: uri !== "" && App.player.contextUri === uri
            width: list.width
            height: 64

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusCover
                color: rowMouse.pressed ? Qt.rgba(1, 1, 1, 0.14) : (rowMouse.containsMouse ? Theme.rowHover : "transparent")
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (row.kind === "liked") Nav.go("liked")
                    else Nav.go(row.kind, { id: row.itemId })
                }
                onDoubleClicked: if (row.uri) App.player.playContext(row.uri)
                ToolTip.visible: root.collapsed && containsMouse
                ToolTip.text: row.title
                ToolTip.delay: 300
            }

            Item {
                id: art
                x: root.collapsed ? (row.width - 48) / 2 : 8
                width: 48
                height: 48
                anchors.verticalCenter: parent.verticalCenter
                Cover {
                    anchors.fill: parent
                    visible: row.kind !== "liked"
                    source: row.imageUrl
                    round: row.round
                }
                LikedArt {
                    anchors.fill: parent
                    visible: row.kind === "liked"
                }
            }

            Column {
                visible: !root.collapsed
                anchors.left: art.right
                anchors.leftMargin: 12
                anchors.right: parent.right
                anchors.rightMargin: 36
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Label {
                    width: parent.width
                    text: row.title
                    color: row.isCurrent ? Theme.accent : Theme.textPrimary
                    font.pixelSize: 15
                    elide: Text.ElideRight
                }
                Label {
                    width: parent.width
                    text: row.subtitle
                    color: Theme.textSecondary
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }

            Icon {
                visible: !root.collapsed && row.isCurrent && App.player.isPlaying
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                size: 16
                fillPath: Icons.speakerF
                strokePath: Icons.volHighS
                strokeWidth: 1.8
                color: Theme.accent
            }
        }

        footer: Item {
            width: list.width
            height: root.loading ? 48 : 16
            BusyIndicator {
                anchors.centerIn: parent
                running: root.loading
                visible: running
                width: 28; height: 28
            }
        }
    }
}
