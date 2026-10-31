import QtQuick
import QtQuick.Controls

// Linha de faixa das listas (playlist, álbum, curtidas, busca, fila).
Item {
    id: root

    property bool showStats:false
    property string statisticKey:""
    property var listening:({})
    property int number: 1
    property string title
    property string artists
    property string album
    property url coverUrl
    property int durationMs: 0
    property string addedAt
    property bool explicitContent: false
    property bool showCover: true
    property bool showAlbum: width > 640
    property bool showAdded: false
    property bool showFavorite: false
    property bool liked: false
    property bool showNumber: true
    property bool isCurrent: false
    property bool isPlaying: false
    property bool selected: false
    // Playlist sua: arrastar a linha muda a ordem. Quem move as linhas é a página (dragMoved em
    // coordenadas da janela, porque a linha muda de lugar enquanto é arrastada).
    property bool draggable: false
    property bool dragging: false

    signal play()
    signal dragStarted()
    signal dragMoved(real sceneX, real sceneY)
    signal dragEnded(bool canceled)
    signal selectRequested()
    signal menuRequested(real x, real y)
    signal artistClicked()
    signal albumClicked()
    signal favoriteRequested()

    implicitHeight: showStats ? 74 : 56
    readonly property bool hovered: hover.hovered
    readonly property real albumWidth: showAlbum ? Math.max(0, (width - 48 - (showFavorite ? 112 : 76) - (showAdded ? 150 : 0)) * 0.38) : 0
    readonly property real addedWidth: showAdded && width > 820 ? 150 : 0

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCover
        color: root.selected || root.dragging ? Qt.rgba(1, 1, 1, 0.18) : (root.hovered ? Theme.rowHover : "transparent")
        border.width: root.dragging ? 1 : 0
        border.color: Theme.textMuted
    }

    HoverHandler { id: hover }

    MouseArea {
        id: rowMouse
        property point pressPos
        property bool moved: false

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        // Nas linhas arrastáveis o arraste é da linha, não da rolagem da lista.
        preventStealing: root.draggable
        cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.ArrowCursor

        onPressed: (e) => {
            pressPos = Qt.point(e.x, e.y)
            moved = false
        }
        onPositionChanged: (e) => {
            if (!root.draggable || !(e.buttons & Qt.LeftButton)) return
            if (!moved && Math.abs(e.y - pressPos.y) < 6) return
            const p = mapToItem(null, e.x, e.y)
            if (!moved) {
                moved = true
                root.dragStarted()
            }
            root.dragMoved(p.x, p.y)
        }
        onReleased: if (moved) root.dragEnded(false)
        onCanceled: {
            if (!moved) return
            moved = false
            root.dragEnded(true)
        }
        onClicked: (e) => {
            if (moved) return // fim de um arraste
            if (e.button === Qt.RightButton)
                root.menuRequested(e.x, e.y)
            else
                root.selectRequested()
        }
        onDoubleClicked: (e) => { if (e.button === Qt.LeftButton) root.play() }
    }

    // # / play / equalizador
    Item {
        id: numberCell
        visible: root.showNumber
        width: root.showNumber ? 48 : 8
        height: parent.height

        Label {
            anchors.centerIn: parent
            visible: !root.hovered && !(root.isCurrent && root.isPlaying)
            text: root.number
            color: root.isCurrent ? Theme.accentText : Theme.textSecondary
            font.pixelSize: 15
            font.features: ({ "tnum": 1 })
        }

        Equalizer {
            anchors.centerIn: parent
            visible: !root.hovered && root.isCurrent && root.isPlaying
            running: visible
        }

        IconButton {
            anchors.centerIn: parent
            visible: root.hovered
            iconSize: 14
            fillPath: root.isCurrent && root.isPlaying ? Icons.pauseF : Icons.playF
            baseColor: Theme.textPrimary
            onClicked: {
                if (root.isCurrent)
                    App.player.togglePlay()
                else
                    root.play()
            }
        }
    }

    Row {
        id: titleCell
        anchors.left: numberCell.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.width - numberCell.width - root.albumWidth - root.addedWidth - durationCell.width
        spacing: 12

        Cover {
            visible: root.showCover
            width: visible ? 40 : 0
            height: 40
            source: root.coverUrl
        }

        Column {
            width: parent.width - (root.showCover ? 52 : 0) - 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Label {
                width: parent.width
                text: root.title
                color: root.isCurrent ? Theme.accentText : Theme.textPrimary
                font.pixelSize: 15
                elide: Text.ElideRight
            }
            Row {
                width: parent.width
                spacing: 6
                Rectangle {
                    visible: root.explicitContent
                    width: 16; height: 16; radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textSecondary
                    Label {
                        anchors.centerIn: parent
                        text: "E"
                        color: "black"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                    }
                }
                Label {
                    width: parent.width - (root.explicitContent ? 22 : 0)
                    text: root.artists
                    color: root.hovered || artistMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: 13
                    font.underline: artistMouse.containsMouse
                    elide: Text.ElideRight
                    MouseArea {
                        id: artistMouse
                        width: Math.min(parent.width, parent.implicitWidth)
                        height: parent.height
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.artistClicked()
                    }
                }
            }
            Label{visible:root.showStats;text:(root.listening.plays || 0)+" escutas · "+((root.listening.ms || 0)/60000).toFixed(1)+" min";color:"#7ddbff";font.pixelSize:12}
        }
    }

    Label {
        id: albumCell
        visible: root.albumWidth > 0
        anchors.left: titleCell.right
        anchors.verticalCenter: parent.verticalCenter
        width: root.albumWidth - 16
        text: root.album
        color: root.hovered || albumMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
        font.pixelSize: 14
        font.underline: albumMouse.containsMouse
        elide: Text.ElideRight
        MouseArea {
            id: albumMouse
            width: Math.min(parent.width, parent.implicitWidth)
            height: parent.height
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.albumClicked()
        }
    }

    Label {
        visible: root.addedWidth > 0
        x: titleCell.x + titleCell.width + root.albumWidth
        anchors.verticalCenter: parent.verticalCenter
        width: root.addedWidth - 16
        text: Theme.formatAdded(root.addedAt)
        color: Theme.textSecondary
        font.pixelSize: 14
        elide: Text.ElideRight
    }

    Item {
        id: durationCell
        anchors.right: parent.right
        width: root.showFavorite ? 112 : 76
        height: parent.height

        Label {
            anchors.right: parent.right
            anchors.rightMargin: root.showFavorite ? 76 : 40
            anchors.verticalCenter: parent.verticalCenter
            text: Theme.formatTime(root.durationMs)
            color: Theme.textSecondary
            font.pixelSize: 14
            font.features: ({ "tnum": 1 })
        }

        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            visible: root.showFavorite
            iconSize: 17
            fillPath: root.liked ? Icons.heart : ""
            strokePath: root.liked ? "" : Icons.heart
            strokeWidth: 1.8
            active: root.liked
            tooltip: root.liked ? qsTr("Remover das favoritas") : qsTr("Favoritar música")
            onClicked: root.favoriteRequested()
        }

        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: root.showFavorite ? 36 : 4
            anchors.verticalCenter: parent.verticalCenter
            visible: root.hovered
            iconSize: 16
            fillPath: Icons.dotsF
            tooltip: qsTr("Mais opções")
            onClicked: root.menuRequested(x + width / 2 + durationCell.x, y + height)
        }
    }

    // Barrinhas animadas da faixa tocando agora
    component Equalizer: Row {
        id: eq
        property bool running: false
        spacing: 2
        height: 14
        Repeater {
            model: 4
            Rectangle {
                id: bar
                required property int index
                width: 2.5
                height: 4
                anchors.bottom: parent.bottom
                color: Theme.accent
                SequentialAnimation on height {
                    running: eq.running
                    loops: Animation.Infinite
                    NumberAnimation { to: 14; duration: 260 + bar.index * 90; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 3; duration: 300 + bar.index * 70; easing.type: Easing.InOutQuad }
                }
            }
        }
    }
}
