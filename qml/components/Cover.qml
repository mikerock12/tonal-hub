import QtQuick
import QtQuick.Effects

// Capa de álbum/playlist/artista com placeholder e (opcional) recorte redondo.
// sourceSize limita a imagem decodificada ao tamanho exibido: economia de RAM.
Item {
    id: root

    property url source
    property bool round: false
    property real radius: Theme.radiusCover
    property string placeholderIcon: round ? Icons.personS : Icons.noteS

    property int retries:0
    onSourceChanged:{retries=0;retry.stop();img.source=root.source}
    Timer{id:retry;interval:1000*(root.retries+1);onTriggered:{const url=root.source;img.source="";img.source=url}}
    readonly property bool ready: img.status === Image.Ready

    Rectangle {
        anchors.fill: parent
        radius: root.round ? width / 2 : root.radius
        color: Theme.elevatedHover
        visible: !root.ready

        Icon {
            anchors.centerIn: parent
            size: Math.min(parent.width, parent.height) * 0.36
            strokePath: root.placeholderIcon
            strokeWidth: 1.6
            color: Theme.textMuted
        }
    }

    Image {
        id: img
        onStatusChanged:if(status===Image.Error&&root.source&&root.retries<2){root.retries++;retry.restart()}
        anchors.fill: parent
        source: root.source
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        // arredonda em degraus de 64px para não redecodificar a cada pixel de redimensionamento
        sourceSize.width: Math.ceil(width * Screen.devicePixelRatio / 64) * 64
        sourceSize.height: Math.ceil(height * Screen.devicePixelRatio / 64) * 64
        visible: !root.round && root.ready
        opacity: root.ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    // Recorte redondo só para artistas: criado sob demanda.
    Loader {
        anchors.fill: parent
        active: root.round && root.ready
        sourceComponent: MultiEffect {
            source: img
            maskEnabled: true
            maskSource: mask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0

            Item {
                id: mask
                width: root.width
                height: root.height
                layer.enabled: true
                visible: false
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    antialiasing: true
                }
            }
        }
    }
}
