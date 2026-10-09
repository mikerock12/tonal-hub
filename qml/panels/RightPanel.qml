import QtQuick
import QtQuick.Controls

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

            Cover {
                width: parent.width
                height: width
                radius: 8
                source: root.player.coverUrlLarge
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
    ListView {
        id: queueList
        visible: Nav.rightPanel === "queue"
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.player.queue
        ScrollBar.vertical: ThinScrollBar { }

        header: Column {
            width: queueList.width
            spacing: 8
            bottomPadding: 16
            Label {
                leftPadding: 8
                text: qsTr("Tocando agora")
                color: Theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.Bold
            }
            QueueRow {
                width: parent.width
                current: true
                item: root.player.hasTrack ? {
                    name: root.player.trackName,
                    artists: root.player.artistsText,
                    coverUrl: root.player.coverUrl
                } : null
            }
            Label {
                visible: root.player.queue.length > 0
                leftPadding: 8
                topPadding: 16
                text: qsTr("A seguir")
                color: Theme.textPrimary
                font.pixelSize: 16
                font.weight: Font.Bold
            }
        }

        delegate: QueueRow {
            required property var modelData
            width: queueList.width
            item: modelData
        }
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
