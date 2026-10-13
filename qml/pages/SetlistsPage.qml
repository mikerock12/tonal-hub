import QtQuick
import QtQuick.Controls

// Setlists: busca shows no setlist.fm e cria uma playlist com as músicas dos shows marcados. O estado (buscas,
// seleção, criação em andamento) vive no singleton Setlists; esta página só mostra.
Item {
    id: root

    property var route                 // a página não depende da rota
    property var expanded: ({})        // shows com "Ver músicas" aberto, por id
    property bool editingKey: false
    property string keyError: ""
    property bool checkingKey: false
    readonly property bool needsKey: !App.setlistFm.hasApiKey || editingKey

    // O campo da key fica no cabeçalho da lista, que é outro componente: a página não enxerga o id dele.
    signal keySaved()

    // Uma linha por show da última busca.
    ListModel { id: showModel }

    function rebuild() {
        showModel.clear()
        for (const s of Setlists.results) {
            showModel.append({
                sid: s.id, venue: s.venue, eventDate: s.eventDate,
                details: [s.artist, [s.city, s.country].filter(x => !!x).join(", "),
                          Setlists.plural(s.songs.length, qsTr("música"), qsTr("músicas")), s.tour]
                         .filter(x => !!x).join(" · "),
                songsText: s.songs.join("\n")
            })
        }
    }

    function toggleExpanded(id) {
        const next = Object.assign({}, expanded)
        if (next[id]) delete next[id]
        else next[id] = true
        expanded = next
    }

    function saveKey(key) {
        if (checkingKey) return
        checkingKey = true
        keyError = ""
        App.setlistFm.setApiKey(key, function(error) {
            root.checkingKey = false
            root.keyError = error
            if (error === "") {
                root.editingKey = false
                root.keySaved()
            }
        })
    }

    Component.onCompleted: rebuild()
    Component.onDestruction: Setlists.clearSearch()
    Connections {
        target: Setlists
        function onResultsChanged() { root.rebuild() }
    }

    // ------------------------------------------------------------ lista
    ListView {
        id: list
        anchors.fill: parent
        model: showModel
        reuseItems: true
        clip: true
        spacing: 8
        boundsBehavior: Flickable.StopAtBounds
        bottomMargin: bottomBar.visible ? bottomBar.height + 16 : 24
        ScrollBar.vertical: ThinScrollBar { }

        header: Column {
            width: list.width
            topPadding: 32
            bottomPadding: 8
            spacing: 16

            // Título
            Item {
                x: 24
                width: parent.width - 48
                height: titleColumn.height
                Column {
                    id: titleColumn
                    width: parent.width - keyLink.width - 16
                    spacing: 6
                    Label {
                        text: qsTr("Setlists")
                        color: Theme.textPrimary
                        font.pixelSize: 40
                        font.weight: Font.Black
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Busque shows no setlist.fm e transforme em playlist. Os shows marcados continuam marcados quando você busca outra banda: dá para juntar bandas diferentes na mesma playlist.")
                        color: Theme.textSecondary
                        font.pixelSize: 14
                    }
                }
                TextAction {
                    id: keyLink
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 14
                    visible: App.setlistFm.hasApiKey && !root.editingKey
                    text: qsTr("Trocar a API key")
                    onClicked: root.editingKey = true
                }
            }

            // Chave do setlist.fm: na primeira vez, ou ao trocar
            Rectangle {
                visible: root.needsKey
                x: 24
                width: Math.min(parent.width - 48, 720)
                height: visible ? keyColumn.height + 40 : 0
                radius: 8
                color: Theme.tile

                Column {
                    id: keyColumn
                    x: 20
                    y: 20
                    width: parent.width - 40
                    spacing: 12

                    Label {
                        text: qsTr("Conecte o setlist.fm")
                        color: Theme.textPrimary
                        font.pixelSize: 18
                        font.weight: Font.Bold
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                        font.pixelSize: 14
                        text: qsTr("O Tonal busca os shows na API do setlist.fm, que pede uma API key gratuita (para uso não comercial). Ela fica guardada no Gerenciador de Credenciais do Windows.")
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Theme.textPrimary
                        font.pixelSize: 14
                        lineHeight: 1.3
                        text: qsTr("1. Entre no setlist.fm (ou crie uma conta grátis).\n2. Em Configurações → API, peça uma key: no formulário, descreva o uso como pessoal.\n3. Copie a key e cole aqui.")
                    }
                    PillButton {
                        primary: false
                        text: qsTr("Abrir o setlist.fm")
                        onClicked: Qt.openUrlExternally("https://www.setlist.fm/settings/api")
                    }
                    Row {
                        width: parent.width
                        spacing: 12
                        TextField {
                            id: keyField
                            width: parent.width - saveKeyButton.width - (cancelKeyButton.visible ? cancelKeyButton.width + 12 : 0) - 12
                            height: 40
                            leftPadding: 12
                            placeholderText: qsTr("API key do setlist.fm")
                            placeholderTextColor: Theme.textMuted
                            color: Theme.textPrimary
                            font.pixelSize: 14
                            font.family: "Consolas"
                            selectByMouse: true
                            enabled: !root.checkingKey
                            onAccepted: root.saveKey(text)
                            Connections {
                                target: root
                                function onKeySaved() { keyField.text = "" }
                            }
                            background: Rectangle {
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.07)
                                border.width: keyField.activeFocus ? 1 : 0
                                border.color: Theme.textMuted
                            }
                        }
                        PillButton {
                            id: saveKeyButton
                            text: root.checkingKey ? qsTr("Conferindo…") : qsTr("Salvar")
                            enabled: keyField.text.trim() !== "" && !root.checkingKey
                            onClicked: root.saveKey(keyField.text)
                        }
                        PillButton {
                            id: cancelKeyButton
                            visible: App.setlistFm.hasApiKey
                            primary: false
                            text: qsTr("Cancelar")
                            onClicked: {
                                root.editingKey = false
                                root.keyError = ""
                                keyField.text = ""
                            }
                        }
                    }
                    Label {
                        visible: root.keyError !== ""
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: root.keyError
                        color: Theme.danger
                        font.pixelSize: 13
                    }
                }
            }

            // Busca
            Row {
                visible: !root.needsKey
                x: 24
                spacing: 12
                TextField {
                    id: searchField
                    width: Math.min(list.width - 48 - searchButton.width - 12, 560)
                    height: 48
                    leftPadding: 48
                    placeholderText: Setlists.query !== "" ? qsTr("Buscar outra banda…") : qsTr("Nome da banda")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.pixelSize: 16
                    selectByMouse: true
                    onAccepted: searchButton.clicked()
                    background: Rectangle {
                        radius: height / 2
                        color: searchField.hovered || searchField.activeFocus ? Theme.elevatedHover : Theme.elevated
                        border.width: searchField.activeFocus ? 2 : 0
                        border.color: Theme.textPrimary
                        Icon {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            size: 22
                            strokePath: Icons.searchS
                            color: Theme.textSecondary
                        }
                    }
                }
                PillButton {
                    id: searchButton
                    anchors.verticalCenter: parent.verticalCenter
                    height: 44
                    text: Setlists.searching ? qsTr("Buscando…") : qsTr("Buscar")
                    enabled: searchField.text.trim() !== "" && !Setlists.searching
                    onClicked: {
                        const query = searchField.text.trim()
                        if (query === "" || Setlists.searching) return
                        Setlists.search(query)
                        searchField.text = ""
                    }
                }
            }

            Label {
                visible: Setlists.searchError !== "" && !root.needsKey
                x: 24
                width: parent.width - 48
                wrapMode: Text.WordWrap
                text: Setlists.searchError
                color: Theme.danger
                font.pixelSize: 14
            }

            BusyIndicator {
                visible: Setlists.searching
                running: visible
                x: 24
            }

            // Ainda sem buscas: exemplos
            Column {
                visible: !root.needsKey && Setlists.results.length === 0 && !Setlists.searching
                x: 24
                width: parent.width - 48
                topPadding: 24
                spacing: 12
                Label {
                    text: qsTr("Comece buscando uma banda")
                    color: Theme.textPrimary
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }
                Flow {
                    width: parent.width
                    spacing: 8
                    Repeater {
                        model: Setlists.examples
                        Chip {
                            required property string modelData
                            text: modelData
                            onClicked: Setlists.search(modelData)
                        }
                    }
                }
            }

            // Banda pesquisada: contagem e marcar/desmarcar os shows desta busca
            Item {
                visible: Setlists.results.length > 0
                x: 24
                width: parent.width - 48
                height: visible ? 48 : 0
                Label {
                    id: bandTitle
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    text: Setlists.query
                    color: Theme.textPrimary
                    font.pixelSize: 20
                    font.weight: Font.Bold
                }
                Label {
                    anchors.left: bandTitle.right
                    anchors.leftMargin: 10
                    anchors.baseline: bandTitle.baseline
                    text: {
                        const n = Setlists.selectedInResults()
                        return Setlists.plural(Setlists.results.length, qsTr("show"), qsTr("shows"))
                               + (n > 0 ? " · " + Setlists.plural(n, qsTr("marcado"), qsTr("marcados")) : "")
                    }
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
                Row {
                    anchors.right: parent.right
                    anchors.baseline: bandTitle.baseline
                    spacing: 16
                    TextAction {
                        text: qsTr("Marcar todos")
                        onClicked: Setlists.selectAll(Setlists.results)
                    }
                    TextAction {
                        text: qsTr("Desmarcar todos")
                        enabled: Setlists.selectedInResults() > 0
                        onClicked: Setlists.deselect(Setlists.results)
                    }
                }
            }
        }

        delegate: Item {
            id: row
            required property string sid
            required property string venue
            required property string eventDate
            required property string details
            required property string songsText

            readonly property bool checked: Setlists.selected[sid] !== undefined
            readonly property bool open: root.expanded[sid] === true
            readonly property var songs: open ? songsText.split("\n") : []

            x: 24
            width: list.width - 48
            height: card.height

            Rectangle {
                id: card
                width: parent.width
                height: cardColumn.height
                radius: 8
                color: row.checked ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                                   : (cardHover.hovered ? Theme.tileHover : Theme.tile)
                border.width: row.checked ? 1 : 0
                border.color: Theme.accent
                HoverHandler { id: cardHover }

                Column {
                    id: cardColumn
                    width: parent.width

                    // Marcar / desmarcar
                    Item {
                        width: parent.width
                        height: 64
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const show = Setlists.find(row.sid)
                                if (show) Setlists.toggle(show)
                            }
                        }
                        Rectangle {
                            id: checkBox
                            x: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20
                            height: 20
                            radius: 4
                            color: row.checked ? Theme.accent : "transparent"
                            border.width: row.checked ? 0 : 2
                            border.color: Theme.textMuted
                            Icon {
                                anchors.centerIn: parent
                                visible: row.checked
                                size: 14
                                strokeWidth: 3
                                strokePath: Icons.checkS
                                color: Theme.textOnAccent
                            }
                        }
                        Column {
                            anchors.left: checkBox.right
                            anchors.leftMargin: 14
                            anchors.right: parent.right
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 2
                            Item {
                                width: parent.width
                                height: venueLabel.height
                                Label {
                                    id: venueLabel
                                    width: parent.width - dateLabel.width - 12
                                    text: row.venue
                                    color: Theme.textPrimary
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Label {
                                    id: dateLabel
                                    anchors.right: parent.right
                                    text: Setlists.formatDate(row.eventDate)
                                    color: Theme.textSecondary
                                    font.pixelSize: 13
                                    font.features: ({ "tnum": 1 })
                                }
                            }
                            Label {
                                width: parent.width
                                text: row.details
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Ver músicas
                    Rectangle { width: parent.width; height: 1; color: Theme.divider }
                    Item {
                        width: parent.width
                        height: 36
                        Row {
                            x: 16
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6
                            Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                size: 14
                                strokePath: Icons.chevronRightS
                                rotation: row.open ? 90 : 0
                                color: songsMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                            }
                            Label {
                                text: row.open ? qsTr("Esconder músicas") : qsTr("Ver músicas")
                                color: songsMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: 13
                            }
                        }
                        MouseArea {
                            id: songsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.toggleExpanded(row.sid)
                        }
                    }
                    Grid {
                        id: songGrid
                        visible: row.open
                        x: 16
                        width: parent.width - 32
                        columns: width > 560 ? 2 : 1
                        columnSpacing: 24
                        rowSpacing: 4
                        bottomPadding: 12
                        Repeater {
                            model: row.songs
                            Label {
                                required property string modelData
                                required property int index
                                width: (songGrid.width - (songGrid.columns - 1) * songGrid.columnSpacing) / songGrid.columns
                                text: (index + 1) + ". " + modelData
                                color: Theme.textSecondary
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        footer: Label {
            x: 24
            width: list.width - 48
            topPadding: 24
            visible: Setlists.results.length > 0
            text: qsTr("Setlists do setlist.fm, a enciclopédia de shows mantida pelos fãs.")
            color: Theme.textMuted
            font.pixelSize: 12
        }
    }

    // ------------------------------------------------------------ barra de baixo: nome e criar
    Rectangle {
        id: bottomBar
        visible: Setlists.selectedCount > 0 || Setlists.creating || Setlists.result !== null
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: barColumn.height + 32
        color: Theme.elevated

        Rectangle { width: parent.width; height: 1; color: Theme.divider }

        Column {
            id: barColumn
            x: 24
            y: 16
            width: parent.width - 48
            spacing: 10

            // Marcados em buscas anteriores: não estão na lista, mas entram na playlist
            Flow {
                readonly property var orphans: Setlists.fromOtherSearches()
                visible: orphans.length > 0
                width: parent.width
                spacing: 8
                Label {
                    height: 32
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Marcados de outras buscas:")
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
                Repeater {
                    model: parent.orphans
                    Chip {
                        required property var modelData
                        text: modelData.artist + " — " + modelData.venue
                        closable: true
                        onClicked: Setlists.deselect([modelData])
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 12
                TextField {
                    id: nameField
                    width: parent.width - createButton.width - 12
                    height: 44
                    leftPadding: 12
                    rightPadding: 64
                    text: Setlists.playlistName
                    maximumLength: Setlists.maxNameLength
                    placeholderText: qsTr("Nome da playlist")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.pixelSize: 15
                    selectByMouse: true
                    enabled: !Setlists.creating
                    onTextEdited: {
                        Setlists.nameEdited = true
                        Setlists.customName = text
                    }
                    onAccepted: createButton.clicked()
                    background: Rectangle {
                        radius: 4
                        color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: nameField.activeFocus ? 1 : 0
                        border.color: Theme.textMuted
                    }
                    Label {
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: nameField.text.length + "/" + Setlists.maxNameLength
                        color: Theme.textMuted
                        font.pixelSize: 12
                        font.features: ({ "tnum": 1 })
                    }
                }
                PillButton {
                    id: createButton
                    anchors.verticalCenter: parent.verticalCenter
                    height: 44
                    text: Setlists.creating ? qsTr("Criando…") : qsTr("Criar playlist (%1)").arg(Setlists.selectedCount)
                    enabled: !Setlists.creating && Setlists.selectedCount > 0 && nameField.text.trim() !== ""
                    onClicked: if (enabled) Setlists.create()
                }
            }

            Row {
                spacing: 16
                Label {
                    text: Setlists.plural(Setlists.selectedCount, qsTr("show"), qsTr("shows")) + " · "
                          + Setlists.plural(Setlists.selectedSongs, qsTr("música"), qsTr("músicas"))
                    color: Theme.textSecondary
                    font.pixelSize: 13
                }
                TextAction {
                    text: qsTr("Limpar seleção")
                    onClicked: Setlists.clearSelection()
                }
                TextAction {
                    visible: Setlists.nameEdited
                    text: qsTr("Usar o nome sugerido")
                    onClicked: {
                        Setlists.nameEdited = false
                        Setlists.customName = ""
                    }
                }
            }

            Row {
                visible: Setlists.creating
                spacing: 10
                BusyIndicator {
                    width: 24
                    height: 24
                    running: Setlists.creating
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Setlists.progress
                    color: Theme.textPrimary
                    font.pixelSize: 13
                }
            }

            Label {
                visible: Setlists.createError !== ""
                width: parent.width
                wrapMode: Text.WordWrap
                text: Setlists.createError
                color: Theme.danger
                font.pixelSize: 13
            }

            // Pronto: resumo e atalho para a playlist
            Rectangle {
                visible: Setlists.result !== null
                width: parent.width
                height: visible ? 56 : 0
                radius: 8
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                border.color: Theme.accent
                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.right: openButton.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    color: Theme.textPrimary
                    font.pixelSize: 14
                    text: {
                        const r = Setlists.result
                        if (!r) return ""
                        let text = r.name + " · " + qsTr("%1 de %2 músicas encontradas").arg(r.found).arg(r.total)
                                   + " · " + Setlists.plural(r.showsCount, qsTr("show"), qsTr("shows"))
                        const repeated = r.songsTotal - r.total
                        if (repeated > 0)
                            text += " · " + (repeated === 1 ? qsTr("1 repetida entrou uma vez só")
                                                            : qsTr("%1 repetidas entraram uma vez só").arg(repeated))
                        return text
                    }
                }
                PillButton {
                    id: openButton
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    height: 36
                    text: qsTr("Abrir playlist")
                    onClicked: Nav.go("playlist", { id: Setlists.result.id })
                }
            }
        }
    }

    // Ação de texto (limpar, remover, trocar a key): sublinha no hover; a destrutiva fica vermelha.
    component TextAction: Label {
        id: action
        property bool destructive: false
        signal clicked()
        color: !enabled ? Theme.textMuted
             : actionMouse.containsMouse ? (destructive ? Theme.danger : Theme.textPrimary) : Theme.textSecondary
        font.pixelSize: 13
        font.underline: actionMouse.containsMouse && enabled
        MouseArea {
            id: actionMouse
            anchors.fill: parent
            hoverEnabled: true
            enabled: action.enabled
            cursorShape: Qt.PointingHandCursor
            onClicked: action.clicked()
        }
    }
}
