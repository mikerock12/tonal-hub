import QtQuick
import QtQuick.Controls

// Card de álbum / playlist / artista usado nas prateleiras da Home, Busca e Artista.
Item {
    id: root

    property string title
    property string subtitle
    property url imageUrl
    property bool round: false
    property bool isCurrent: false   // é o contexto que está tocando agora
    property bool isPlaying: false

    signal clicked()
    signal playClicked()

    implicitWidth: 180
    implicitHeight: width + 76

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusCard
        color: hover.hovered ? Theme.cardHover : "transparent"
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    // HoverHandler é passivo: continua "hovered" mesmo com o mouse sobre o botão de play.
    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: root.clicked() }

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Item {
            width: parent.width
            height: width

            Cover {
                anchors.fill: parent
                source: root.imageUrl
                round: root.round
                radius: Theme.radiusCard
            }

            // Botão de play que "sobe" quando passa o mouse
            PlayButton {
                id: play
                diameter: 48
                anchors.right: parent.right
                anchors.rightMargin: 8
                y: parent.height - height - (shown ? 8 : 0)
                opacity: shown ? 1 : 0
                visible: opacity > 0
                playing: root.isCurrent && root.isPlaying
                tooltip: ""
                readonly property bool shown: hover.hovered || hovered || (root.isCurrent && root.isPlaying)
                readonly property bool hovered: playHover.hovered
                Behavior on opacity { NumberAnimation { duration: 150 } }
                Behavior on y { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                onClicked: root.playClicked()
                HoverHandler { id: playHover }
            }
        }

        Label {
            width: parent.width
            text: root.title
            color: root.isCurrent ? Theme.accent : Theme.textPrimary
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
        }
        Label {
            width: parent.width
            text: root.subtitle
            color: Theme.textSecondary
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            elide: Text.ElideRight
            maximumLineCount: 2
            lineHeight: 1.1
        }
    }
}
