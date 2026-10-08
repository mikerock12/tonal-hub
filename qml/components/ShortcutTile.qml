import QtQuick
import QtQuick.Controls

// Atalho retangular do topo da Home (capa à esquerda, nome, play no hover).
Item {
    id: root

    property string title
    property url imageUrl
    property bool isCurrent: false
    property bool isPlaying: false
    property bool likedSongs: false

    signal clicked()
    signal playClicked()

    implicitHeight: 64

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCover
        color: hover.hovered ? Theme.tileHover : Theme.tile
        clip: true
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    // HoverHandler é passivo: continua "hovered" mesmo com o mouse sobre o botão de play.
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Row {
        anchors.fill: parent
        spacing: 12

        Item {
            width: root.height
            height: root.height
            Cover {
                anchors.fill: parent
                source: root.imageUrl
                visible: !root.likedSongs
                radius: 0
            }
            Loader {
                anchors.fill: parent
                active: root.likedSongs
                sourceComponent: LikedArt { radius: 0 }
            }
        }

        Label {
            width: root.width - root.height - 12 - 60
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: root.isCurrent ? Theme.accent : Theme.textPrimary
            font.pixelSize: 14
            font.weight: Font.Bold
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
        }
    }

    PlayButton {
        diameter: 34
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        playing: root.isCurrent && root.isPlaying
        opacity: hover.hovered || playHover.hovered || (root.isCurrent && root.isPlaying) ? 1 : 0
        visible: opacity > 0
        tooltip: ""
        Behavior on opacity { NumberAnimation { duration: 150 } }
        onClicked: root.playClicked()
        HoverHandler { id: playHover }
    }
}
