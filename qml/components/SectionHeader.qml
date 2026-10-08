import QtQuick
import QtQuick.Controls

// Título de seção ("Tocadas recentemente") com link opcional "Mostrar tudo".
Item {
    id: root

    property string title
    property string actionText: ""
    signal actionClicked()

    implicitHeight: 40

    Label {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - (action.visible ? action.width + 16 : 0)
        text: root.title
        color: Theme.textPrimary
        font.pixelSize: 24
        font.weight: Font.Bold
        elide: Text.ElideRight
    }

    Label {
        id: action
        visible: root.actionText !== ""
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        text: root.actionText
        color: Theme.textSecondary
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.underline: actionMouse.containsMouse
        MouseArea {
            id: actionMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.actionClicked()
        }
    }
}
