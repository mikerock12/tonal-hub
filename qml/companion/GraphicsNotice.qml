import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Aviso quando o Tonal trocou o Direct3D por OpenGL ou software depois de uma
// queda do DirectX do Windows. Nunca troca em silêncio (pedido do usuário).
Rectangle {
    id: notice
    objectName: "graphicsNotice"
    readonly property var safety: typeof graphicsSafety !== "undefined" ? graphicsSafety : null
    visible: !!safety && safety.noticePending
    z: 1000
    anchors.left: parent.left
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.margins: 12
    height: column.implicitHeight + 28
    radius: 12
    color: "#3a2a12"
    border.color: "#f0b44c"
    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6
        Label {
            Layout.fillWidth: true
            text: notice.safety ? notice.safety.title : ""
            color: "#ffe2ad"
            font.bold: true
            font.pixelSize: 15
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            text: notice.safety ? notice.safety.message : ""
            color: "#fbeedd"
            wrapMode: Text.Wrap
        }
        Label {
            Layout.fillWidth: true
            visible: !!notice.safety && !!notice.safety.adapter
            text: notice.safety ? qsTr("Placa de vídeo: %1 · Desenhando com: %2")
                                  .arg(notice.safety.adapter)
                                  .arg(notice.safety.activeApi || notice.safety.backend) : ""
            color: "#d9c6a8"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        RowLayout {
            spacing: 8
            Button {
                text: qsTr("Tentar DirectX de novo na próxima abertura")
                onClicked: { notice.safety.retryDirectX(); retried.visible = true }
            }
            Button {
                objectName: "graphicsNoticeDismiss"
                text: qsTr("Entendi")
                onClicked: notice.safety.dismissNotice()
            }
            Label { id: retried; visible: false; color: "#ffe2ad"; text: qsTr("Feche e abra o app para testar.") }
        }
    }
}
