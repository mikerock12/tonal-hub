import QtQuick
import QtQuick.Controls
import "qrc:/companion/qml/companion" as Personal

// Painel direito: "Tocando agora" (capa grande) ou "Fila".
Rectangle {
    id: root

    color: Theme.panel
    radius: Theme.radiusPanel
    clip: true

    readonly property var player: App.player

    Item {
        id: header
        width: parent.width
        height: 56

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: closeBtn.left
            anchors.verticalCenter: parent.verticalCenter
            text: Nav.rightPanel === "queue" ? qsTr("Fila") : (root.player.albumName || qsTr("Tocando agora"))
            color: Theme.textPrimary
            font.pixelSize: 16
            font.weight: Font.Bold
            elide: Text.ElideRight
        }
        IconButton {
            id: closeBtn
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            iconSize: 16
            strokePath: Icons.closeS
            tooltip: qsTr("Fechar")
            onClicked: Nav.rightPanel = ""
        }
    }

    // ---------------------------------------------------- Tocando agora
    Flickable {
        visible: Nav.rightPanel === "nowplaying"
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        contentHeight: npCol.implicitHeight + 32
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ThinScrollBar { }

        Column {
            id: npCol
            x: 16
            width: root.width - 32
            spacing: 16
            topPadding: nowCover.christmas ? 20 : 0   // room for the strand above the cover

            // "Tocando agora" shows one cover, the current song: same stage as the song page.
            Item {
                id: nowCover
                // The panel clips, so the cover steps in to leave the strands room.
                readonly property real inset: Theme.themeId === "natal" ? 16 : 0
                x: inset
                width: parent.width - 2 * inset
                height: width
                readonly property bool christmas: Theme.themeId === "natal" && nowArt.ready
                readonly property bool animated: root.visible && Nav.rightPanel === "nowplaying"
                Personal.ChristmasGarland {
                    objectName: "nowGarlandBack"; plane: "back"; ownClock: false; phase: nowFront.phase
                    anchors.centerIn: parent; visible: nowCover.christmas
                    coverWidth: nowCover.width; coverHeight: nowCover.height
                    playing: root.player.isPlaying; animationAllowed: nowCover.animated
                }
                Cover {
                    id: nowArt
                    anchors.fill: parent
                    radius: 8
                    decorated: Theme.themeId !== "natal"
                    source: root.player.coverUrlLarge
                }
                Personal.ChristmasGarland {
                    id: nowFront
                    objectName: "nowGarlandFront"; plane: "front"
                    anchors.centerIn: parent; visible: nowCover.christmas
                    coverWidth: nowCover.width; coverHeight: nowCover.height
                    playing: root.player.isPlaying; animationAllowed: nowCover.animated
                }
                Personal.ChristmasSanta {
                    objectName: "nowSanta"; visible: nowCover.christmas
                    width: nowCover.width * .46; height: width
                    x: nowCover.width - width + nowCover.width * .008
                    y: nowCover.height - height + nowCover.width * .006
                    musicPlaying: root.player.isPlaying; animationAllowed: nowCover.animated
                }
            }

            Item {
                width: parent.width
                height: titleCol.implicitHeight
                Column {
                    id: titleCol
                    anchors.left: parent.left
                    anchors.right: npLike.left
                    anchors.rightMargin: 8
                    spacing: 4
                    Label {
                        width: parent.width
                        text: root.player.trackName
                        color: Theme.textPrimary
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                    }
                    Label {
                        width: parent.width
                        text: root.player.artistsText
                        color: Theme.textSecondary
                        font.pixelSize: 16
                        elide: Text.ElideRight
                        font.underline: npArtistMouse.containsMouse
                        MouseArea {
                            id: npArtistMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const a = root.player.artists
                                if (a.length > 0 && a[0].id) Nav.go("artist", { id: a[0].id })
                            }
                        }
                    }
                }
                IconButton {
                    id: npLike
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.player.trackUri.indexOf("spotify:track:") === 0
                    iconSize: 20
                    fillPath: root.player.liked ? Icons.heart : ""
                    strokePath: root.player.liked ? "" : Icons.heart
                    strokeWidth: 1.8
                    active: root.player.liked
                    onClicked: root.player.toggleLike()
                }
            }

            // Próxima da fila
            Rectangle {
                visible: root.player.queue.length > 0
                width: parent.width
                height: nextCol.implicitHeight + 32
                radius: 8
                color: Theme.elevated
                Column {
                    id: nextCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 8
                    Label {
                        text: qsTr("A seguir na fila")
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }
                    QueueRow {
                        width: parent.width
                        item: root.player.queue.length > 0 ? root.player.queue[0] : null
                    }
                }
            }
        }
    }

    // ---------------------------------------------------- Fila
    Personal.QueueEditor {
        visible: Nav.rightPanel === "queue"
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        player: root.player
        tone: App.theme.current
    }

    component QueueRow: Item {
        id: qr
        property var item: null
        property bool current: false
        height: item ? 56 : 0
        visible: item !== null

        Rectangle {
            anchors.fill: parent
            radius: 4
            color: qrHover.hovered ? Theme.rowHover : "transparent"
        }
        HoverHandler { id: qrHover }

        Cover {
            id: qrCover
            x: 8
            width: 40; height: 40
            anchors.verticalCenter: parent.verticalCenter
            source: qr.item ? qr.item.coverUrl : ""
        }
        Column {
            anchors.left: qrCover.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Label {
                width: parent.width
                text: qr.item ? qr.item.name : ""
                color: qr.current ? Theme.accentText : Theme.textPrimary
                font.pixelSize: 15
                elide: Text.ElideRight
            }
            Label {
                width: parent.width
                text: qr.item ? qr.item.artists : ""
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }
        }
    }
}
