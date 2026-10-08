import QtQuick
import QtQuick.Controls

// Primeira tela: configurar o Client ID e fazer login.
Rectangle {
    id: root
    color: Theme.windowBg

    gradient: Gradient {
        GradientStop { position: 0; color: "#1f1f1f" }
        GradientStop { position: 1; color: "#000000" }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: Math.max(height, card.height + 96)
        boundsBehavior: Flickable.StopAtBounds

        Rectangle {
            id: card
            width: Math.min(560, root.width - 48)
            height: col.implicitHeight + 72
            anchors.centerIn: parent
            radius: 12
            color: Theme.panel

            Column {
                id: col
                anchors.top: parent.top
                anchors.topMargin: 36
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 72
                spacing: 20

                Logo { size: 56; anchors.horizontalCenter: parent.horizontalCenter }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Bem-vindo ao Tonal")
                    color: Theme.textPrimary
                    font.pixelSize: 30
                    font.weight: Font.Bold
                }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Um player leve para a sua conta Spotify Premium. Na primeira vez é preciso criar um app gratuito no painel de desenvolvedor do Spotify (leva 2 minutos).")
                    color: Theme.textSecondary
                    font.pixelSize: 14
                }

                Rectangle {
                    width: parent.width
                    height: steps.implicitHeight + 32
                    radius: 8
                    color: Theme.elevated

                    Column {
                        id: steps
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 10

                        Step { n: 1; text: qsTr("Abra o painel de desenvolvedor e clique em <b>Create app</b>.") }
                        Chip {
                            x: 32
                            text: "developer.spotify.com/dashboard"
                            onClicked: Qt.openUrlExternally("https://developer.spotify.com/dashboard")
                        }
                        Step { n: 2; text: qsTr("Em <b>Redirect URIs</b>, adicione exatamente:") }
                        Row {
                            x: 32
                            spacing: 8
                            Rectangle {
                                width: uriLabel.implicitWidth + 20
                                height: 32
                                radius: 4
                                color: "#121212"
                                Label {
                                    id: uriLabel
                                    anchors.centerIn: parent
                                    text: App.auth.redirectUri
                                    color: Theme.accent
                                    font.family: "Consolas"
                                    font.pixelSize: 13
                                }
                            }
                            Chip {
                                text: copied ? qsTr("Copiado!") : qsTr("Copiar")
                                property bool copied: false
                                onClicked: { App.copyToClipboard(App.auth.redirectUri); copied = true }
                            }
                        }
                        Step { n: 3; text: qsTr("Marque a opção <b>Web API</b> e salve.") }
                        Step { n: 4; text: qsTr("Abra <b>Settings</b> do app, copie o <b>Client ID</b> e cole abaixo.") }
                    }
                }

                TextField {
                    id: clientId
                    width: parent.width
                    height: 48
                    leftPadding: 16
                    text: App.auth.clientId
                    placeholderText: qsTr("Client ID")
                    placeholderTextColor: Theme.textMuted
                    color: Theme.textPrimary
                    font.pixelSize: 15
                    font.family: "Consolas"
                    selectByMouse: true
                    background: Rectangle {
                        radius: 6
                        color: Theme.elevated
                        border.width: clientId.activeFocus ? 2 : 1
                        border.color: clientId.activeFocus ? Theme.textPrimary : Qt.rgba(1, 1, 1, 0.2)
                    }
                    onAccepted: loginButton.clicked()
                }

                Rectangle {
                    id: loginButton
                    signal clicked()
                    width: parent.width
                    height: 48
                    radius: 24
                    color: App.auth.busy ? Theme.elevatedHover
                                         : (loginMouse.containsMouse ? Theme.accentHover : Theme.accent)
                    Label {
                        anchors.centerIn: parent
                        text: App.auth.busy ? qsTr("Aguardando o navegador...") : qsTr("Entrar com Spotify")
                        color: App.auth.busy ? Theme.textPrimary : Theme.textOnAccent
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    MouseArea {
                        id: loginMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: loginButton.clicked()
                    }
                    onClicked: {
                        App.auth.clientId = clientId.text
                        App.auth.login()
                    }
                }

                Label {
                    visible: App.auth.lastError !== ""
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: App.auth.lastError
                    color: Theme.danger
                    font.pixelSize: 13
                }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Depois do login, o motor de áudio (librespot) abre uma segunda autorização no navegador, só na primeira vez.")
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
            }
        }
    }

    component Step: Row {
        id: step
        property int n: 1
        property alias text: stepLabel.text
        width: parent ? parent.width : 400
        spacing: 10
        Rectangle {
            width: 22; height: 22; radius: 11
            color: Theme.accent
            Label {
                anchors.centerIn: parent
                text: step.n
                color: Theme.textOnAccent
                font.pixelSize: 12
                font.weight: Font.Bold
            }
        }
        Label {
            id: stepLabel
            width: parent.width - 32
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.StyledText
            wrapMode: Text.WordWrap
            color: Theme.textPrimary
            font.pixelSize: 14
        }
    }
}
