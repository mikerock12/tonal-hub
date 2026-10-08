import QtQuick

// Prateleira da Home: mostra só os cards que cabem na largura (como o app oficial),
// sem rolagem horizontal que roubaria a roda do mouse da página.
//
// model: array de objetos { title, subtitle, imageUrl, round, uri, kind, id }
Column {
    id: root

    property string title
    property var model: []
    property real minCardWidth: 176
    property bool showAll: false   // true = quebra em várias linhas (página "Mostrar tudo")
    signal itemClicked(var item)
    signal itemPlayClicked(var item)
    signal showAllClicked()

    readonly property int perRow: Math.max(1, Math.floor((width + 8) / (minCardWidth + 8)))
    readonly property real cardWidth: (width - (perRow - 1) * 8) / perRow
    readonly property var visibleItems: showAll ? model : model.slice(0, perRow)

    spacing: 4
    visible: model.length > 0

    SectionHeader {
        width: parent.width
        title: root.title
        actionText: !root.showAll && root.model.length > root.perRow ? qsTr("Mostrar tudo") : ""
        onActionClicked: root.showAllClicked()
    }

    Flow {
        width: parent.width
        spacing: 8
        anchors.left: parent.left
        anchors.leftMargin: -12   // alinha o texto do card com o título

        Repeater {
            model: root.visibleItems
            MediaCard {
                required property var modelData
                width: root.cardWidth
                title: modelData.title
                subtitle: modelData.subtitle || ""
                imageUrl: modelData.imageUrl || ""
                round: !!modelData.round
                isCurrent: modelData.uri !== undefined && modelData.uri !== ""
                           && (App.player.contextUri === modelData.uri || App.player.trackUri === modelData.uri)
                isPlaying: App.player.isPlaying
                onClicked: root.itemClicked(modelData)
                onPlayClicked: root.itemPlayClicked(modelData)
            }
        }
    }
}
