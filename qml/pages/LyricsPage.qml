import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Letra da faixa que está tocando (App.lyrics, do LRCLIB). Na letra sincronizada a linha atual fica em
// destaque, a lista acompanha a música e um clique numa linha pula para ela.
Item {
    id: root

    property var route              // a letra segue a faixa que toca, não a rota
    property color tint: Theme.elevated
    property int current: -1
    property bool follow: true      // acompanha a música; para por alguns segundos quando a pessoa rola
    readonly property int status: App.lyrics.status
    readonly property bool synced: status === Lyrics.Synced
    // Destaca a linha um pouco antes: a posição do Spotify chega com o atraso da rede.
    readonly property int lead: 250
    readonly property var translationLines: App.music.lines
    readonly property bool translationMatches: {
        if (App.music.current.uri !== App.player.trackUri || translationLines.length !== App.lyrics.lines.length || !translationLines.length)
            return false
        const original = App.lyrics.lines
        for (let i = 0; i < original.length; ++i)
            if (original[i].text !== translationLines[i].text || original[i].time !== translationLines[i].time)
                return false
        return true
    }
    readonly property bool hasTranslation: translationMatches && translationLines.some(row => !!row.translation)
    property bool autoTranslate: true
    property string requestedTranslation: ""
    function translateCurrent() {
        if (!(synced || status === Lyrics.Plain) || App.music.busy) return
        const key = App.player.trackUri + "|" + App.lyrics.lines.map(row => row.time+":"+row.text).join("\n")
        if (requestedTranslation === key) return
        if (App.music.useDisplayedLyrics(App.player.trackUri, App.lyrics.lines)) {
            requestedTranslation = key
            App.music.translate("auto")
        }
    }

    Component.onCompleted: {
        App.lyrics.active = true
        root.updateTint()
        root.sync()
        Qt.callLater(root.translateCurrent)
    }
    Component.onDestruction: App.lyrics.active = false

    function updateTint() {
        const url = App.player.coverUrl
        if (!url) {
            root.tint = Theme.elevated
            return
        }
        App.dominantColor(url, function(c) { if (url === App.player.coverUrl) root.tint = c })
    }

    // Acha a linha atual e agenda o timer para a próxima troca de linha (em vez de conferir sem parar).
    function sync() {
        if (!root.synced) {
            root.current = -1
            tick.stop()
            return
        }
        const pos = App.player.positionMs + root.lead
        const i = App.lyrics.lineAt(pos)
        root.current = i
        const next = App.lyrics.lineTime(i + 1)
        if (App.player.isPlaying && next >= 0) {
            tick.interval = Math.max(30, next - pos + 5)
            tick.restart()
        } else {
            tick.stop()
        }
    }

    Timer { id: tick; onTriggered: root.sync() }
    Timer { id: resumeFollow; interval: 4000; onTriggered: root.follow = true }

    Connections {
        target: App.player
        function onPositionChanged() { root.sync() }
        function onStateChanged() { root.sync() }
        function onTrackChanged() { root.updateTint() }
    }
    Connections {
        target: App.lyrics
        function onChanged() {
            root.follow = true
            list.positionViewAtBeginning()
            root.sync()
            if (root.autoTranslate) Qt.callLater(root.translateCurrent)
        }
    }
    Connections { target: App.music; function onLyricsChanged() { if(root.autoTranslate) Qt.callLater(root.translateCurrent) } }

    Rectangle {
        anchors.fill: parent
        color: Qt.darker(root.tint, 1.7)
        Behavior on color { ColorAnimation { duration: 400 } }
    }

    Rectangle {
        id: songHero
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: Math.min(210, root.height * 0.3)
        color: "transparent"
        RowLayout {
            anchors.fill: parent; anchors.margins: 20; spacing: 22
            Image { Layout.preferredHeight: parent.height; Layout.preferredWidth: parent.height; source: App.player.coverUrl; fillMode: Image.PreserveAspectFit; asynchronous: true; sourceSize.width: 420; sourceSize.height: 420 }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: qsTr("TOCANDO AGORA"); font.pixelSize: 11; color: Theme.textSecondary }
                Label { text: App.player.trackName || qsTr("Sua música"); textFormat: Text.PlainText; color:Theme.textPrimary; font.pixelSize: 28; font.bold: true; wrapMode: Text.Wrap; Layout.fillWidth: true; maximumLineCount: 2 }
                Label { text: App.player.artistsText || ""; textFormat: Text.PlainText; font.pixelSize: 16; color: Theme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: App.player.albumName || ""; textFormat: Text.PlainText; color: Theme.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
                RowLayout {
                    Button { text:"‹"; Accessible.name:qsTr("Anterior"); onClicked:App.player.previous() }
                    Button { text: App.player.isPlaying ? qsTr("Pausar") : qsTr("Reproduzir"); onClicked: App.player.togglePlay() }
                    Button { text:"›"; Accessible.name:qsTr("Próxima"); onClicked:App.player.next() }
                    Button { text: App.player.liked ? "♥" : "♡"; font.pixelSize: 24; Accessible.name: qsTr("Favoritar música"); onClicked: App.player.toggleLike() }
                }
            }
        }
    }
    Column {
        id: translationBar
        anchors.top: songHero.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        spacing: 6

        Flow {
            width: parent.width
            spacing: 8
            Button {
                objectName: "translateLyricsButton"
                text: App.music.busy && root.translationMatches ? qsTr("Traduzindo…") : qsTr("Traduzir pt-BR")
                enabled: (root.synced || root.status === Lyrics.Plain) && !App.music.busy
                onClicked: {
                    root.autoTranslate = true
                    root.requestedTranslation = ""
                    showTranslation.checked = true
                    root.translateCurrent()
                }
            }
            Label {
                objectName: "lyricsSourceLanguage"
                text: qsTr("Idioma detectado automaticamente")
                color: Theme.textSecondary
                height: 40
                verticalAlignment: Text.AlignVCenter
            }
            CheckBox {
                id: showTranslation
                text: qsTr("Mostrar tradução")
                palette.windowText: Theme.textPrimary
                checked: true
                visible: root.hasTranslation
            }
        }
        Label {
            width: parent.width
            text: root.translationMatches ? App.music.lyricStatus : qsTr("Traduza a letra para português sem sair desta tela.")
            textFormat: Text.PlainText
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        Label {
            width: parent.width
            text: qsTr("Ao traduzir, a letra é enviada ao Google Tradutor. A tradução automática pode conter erros.")
            color: Theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }
    }

    ListView {
        id: list
        anchors.top: translationBar.bottom
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 48
        anchors.rightMargin: 24
        visible: root.synced || root.status === Lyrics.Plain
        model: visible ? App.lyrics.lines : []
        reuseItems: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        topMargin: 16
        bottomMargin: height * 0.5          // deixa as últimas linhas subirem até a altura da linha atual
        ScrollBar.vertical: ThinScrollBar { }

        // Um highlight vazio faz a lista rolar com animação até a linha atual, mantendo-a a 30% da altura.
        currentIndex: root.current
        highlight: Item { }
        highlightFollowsCurrentItem: true
        highlightMoveDuration: 450
        highlightMoveVelocity: -1
        highlightRangeMode: root.follow && root.synced ? ListView.ApplyRange : ListView.NoHighlightRange
        preferredHighlightBegin: height * 0.3
        preferredHighlightEnd: height * 0.3 + 48

        onMovementStarted: {
            root.follow = false
            resumeFollow.stop()
        }
        onMovementEnded: resumeFollow.restart()

        header: Label {
            width: ListView.view.width
            visible: root.status === Lyrics.Plain
            height: visible ? implicitHeight + 24 : 0
            text: qsTr("Esta letra não está sincronizada com a música.")
            color: Theme.textSecondary
            font.pixelSize: 14
        }

        delegate: Item {
            id: line
            required property var modelData
            required property int index

            width: ListView.view.width
            height: verse.implicitHeight + 12
            opacity: !root.synced || hover.containsMouse || index === root.current ? 1
                     : index < root.current ? 0.7 : 0.45
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Column {
                id: verse
                width: parent.width
                y: 6
                spacing: 5
                Text {
                    width: parent.width - 24
                    text: line.modelData.text !== "" ? line.modelData.text : (root.synced ? "♪" : " ")
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    font.pixelSize: 28
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                }
                Text {
                    objectName: "lyricTranslationLine"
                    width: parent.width - 24
                    text: root.translationMatches ? (root.translationLines[line.index].translation || "") : ""
                    textFormat: Text.PlainText
                    visible: showTranslation.checked && text.length > 0
                    wrapMode: Text.Wrap
                    font.pixelSize: 22
                    color: "#d1e6ed"
                }
            }

            MouseArea {
                id: hover
                anchors.fill: parent
                enabled: root.synced
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    App.player.seek(line.modelData.time)
                    root.follow = true
                }
            }
        }

        footer: Label {
            width: ListView.view.width
            topPadding: 32
            bottomPadding: 16
            text: qsTr("Letras do LRCLIB (lrclib.net), um acervo aberto mantido por voluntários.")
            color: Theme.textSecondary
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    // Sem letra para mostrar: carregando, não achou, instrumental, erro ou nada tocando.
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 96, 520)
        spacing: 12
        visible: !list.visible

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.textPrimary
            font.pixelSize: 24
            font.weight: Font.Bold
            text: {
                switch (root.status) {
                case Lyrics.Loading: return qsTr("Procurando a letra…")
                case Lyrics.NotFound: return qsTr("Ainda não temos a letra desta música")
                case Lyrics.Instrumental: return qsTr("♪ Esta música é instrumental")
                case Lyrics.Error: return qsTr("Não consegui buscar a letra")
                default: return qsTr("Toque uma música para ver a letra")
                }
            }
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Theme.textSecondary
            font.pixelSize: 14
            visible: text !== ""
            text: root.status === Lyrics.NotFound
                  ? qsTr("As letras vêm do LRCLIB (lrclib.net), um acervo aberto mantido por voluntários.")
                  : root.status === Lyrics.Error ? qsTr("Confira a conexão e tente de novo.") : ""
        }
        Chip {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.status === Lyrics.Error
            text: qsTr("Tentar de novo")
            onClicked: App.lyrics.retry()
        }
    }
}
