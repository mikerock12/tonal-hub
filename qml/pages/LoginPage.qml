import QtQuick
import QtQuick.Controls

// Primeira tela: criar o app no painel do Spotify, colar o Client ID e fazer login.
Rectangle {
    id: root
    color: Theme.windowBg

    readonly property string createAppUrl: "https://developer.spotify.com/dashboard/create"
    readonly property bool formatOk: App.auth.isClientIdFormat(clientId.text)
    readonly property string status: App.auth.clientIdStatus
    readonly property bool canLogin: formatOk && status !== "invalid" && status !== "checking" && !App.auth.busy

    gradient: Gradient {
        GradientStop { position: 0; color: Theme.elevated }
        GradientStop { position: 1; color: Theme.windowBg }
    }

    function login() {
        if (!root.canLogin) return
        App.auth.clientId = clientId.text.trim()
        App.auth.login()
    }

    // Ao voltar do navegador com o Client ID copiado, cola sozinho. Só aceita o formato de um
    // Client ID e não mexe num ID que já foi conferido.
    function pasteClientIdFromClipboard() {
        if (App.auth.busy || (root.formatOk && root.status === "valid")) return
        const t = App.clipboardText().trim()
        if (t !== clientId.text.trim() && App.auth.isClientIdFormat(t)) {
            clientId.text = t
            Nav.toast(qsTr("Client ID colado da área de transferência"))
        }
    }

    readonly property bool windowActive: Window.active
    onWindowActiveChanged: if (windowActive) pasteClientIdFromClipboard()

    // Confere o Client ID no Spotify pouco depois de parar de digitar ou colar.
    Timer {
        id: checkTimer
        interval: 400
        onTriggered: App.auth.checkClientId(clientId.text)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: Math.max(height, card.height + 96)
        boundsBehavior: Flickable.StopAtBounds

        Rectangle {
            id: card
            width: Math.min(600, root.width - 48)
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
                    text: qsTr("Bem-vindo ao Tonalify")
                    color: Theme.textPrimary
                    font.pixelSize: 30
                    font.weight: Font.Bold
                }

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("Um player leve para a sua conta Spotify Premium. Na primeira vez, crie um app gratuito no painel do Spotify: são 3 passos e uns 2 minutos.")
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

                        Step { n: 1; text: qsTr("Abra o formulário de criação de app (entre com sua conta Spotify se ele pedir).") }
                        Chip {
                            x: 32
                            text: qsTr("Abrir o painel do Spotify")
                            onClicked: Qt.openUrlExternally(root.createAppUrl)
                        }

                        Step { n: 2; text: qsTr("Preencha assim (no Redirect URIs, cole e clique em <b>Add</b>), marque <b>Web API</b>, aceite os termos e clique em <b>Save</b>:") }
                        CopyRow { label: qsTr("App name"); value: "Tonalify" }
                        CopyRow { label: qsTr("App description"); value: qsTr("Player pessoal") }
                        CopyRow { label: qsTr("Redirect URIs"); value: App.auth.redirectUri }

                        Step { n: 3; text: qsTr("Na página do app, copie o <b>Client ID</b>. Ao voltar para cá ele é colado sozinho.") }
                    }
                }

                Column {
                    width: parent.width
                    spacing: 8

                    TextField {
                        id: clientId
                        width: parent.width
                        height: 48
                        leftPadding: 16
                        rightPadding: 44
                        text: App.auth.clientId
                        placeholderText: qsTr("Client ID")
                        placeholderTextColor: Theme.textMuted
                        color: Theme.textPrimary
                        font.pixelSize: 15
                        font.family: "Consolas"
                        selectByMouse: true
                        enabled: !App.auth.busy
                        background: Rectangle {
                            radius: 6
                            color: Theme.elevated
                            border.width: clientId.activeFocus ? 2 : 1
                            border.color: root.status === "invalid" || (clientId.text !== "" && !root.formatOk) ? Theme.danger
                                        : clientId.activeFocus ? Theme.textPrimary : Qt.rgba(1, 1, 1, 0.2)
                        }
                        onTextChanged: checkTimer.restart()
                        onAccepted: root.login()

                        Icon {
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            visible: root.formatOk && root.status === "valid"
                            size: 20
                            strokePath: Icons.checkS
                            strokeWidth: 2.4
                            color: Theme.accentText
                        }
                        BusyIndicator {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 32; height: 32
                            running: root.status === "checking"
                            visible: running
                        }
                    }

                    Label {
                        width: parent.width
                        visible: text !== ""
                        wrapMode: Text.WordWrap
                        textFormat: Text.StyledText
                        font.pixelSize: 13
                        color: root.status === "valid" ? Theme.accentText
                             : (root.status === "invalid" || !root.formatOk) ? Theme.danger : Theme.textMuted
                        text: {
                            if (clientId.text.trim() === "") return ""
                            if (!root.formatOk)
                                return qsTr("O Client ID tem 32 caracteres, só números e letras de a a f.")
                            switch (root.status) {
                            case "checking": return qsTr("Conferindo com o Spotify...")
                            case "valid": return qsTr("Client ID encontrado. Pode entrar.")
                            case "invalid": return qsTr("O Spotify não conhece esse Client ID. Confira se copiou o <b>Client ID</b>, e não o <b>Client Secret</b>.")
                            case "unknown": return qsTr("Não deu para conferir agora (sem internet?). Você ainda pode tentar entrar.")
                            }
                            return ""
                        }
                    }
                }

                Rectangle {
                    id: loginButton
                    width: parent.width
                    height: 48
                    radius: 24
                    color: !root.canLogin ? Theme.elevatedHover
                                          : (loginMouse.containsMouse ? Theme.accentHover : Theme.accent)
                    Label {
                        anchors.centerIn: parent
                        text: App.auth.busy ? qsTr("Aguardando o navegador...") : qsTr("Entrar com Spotify")
                        color: root.canLogin ? Theme.textOnAccent : Theme.textMuted
                        font.pixelSize: 16
                        font.weight: Font.Bold
                    }
                    MouseArea {
                        id: loginMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: root.canLogin ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: root.login()
                    }
                }

                // Esperando o navegador: o erro mais comum aparece lá, não aqui.
                Column {
                    visible: App.auth.busy
                    width: parent.width
                    spacing: 10
                    Label {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        textFormat: Text.StyledText
                        text: qsTr("Se o navegador mostrar <b>Invalid redirect URI</b>, o endereço do passo 2 não foi cadastrado exatamente igual: confira, clique em Save e tente de novo.")
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                    Chip {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: qsTr("Cancelar")
                        onClicked: App.auth.cancelLogin()
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

                // Troca de idioma antes de entrar: reinicia na hora (não há sessão para perder). O nome do outro
                // idioma vai nele mesmo, para quem não lê o atual.
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    readonly property string other: App.language === "pt" ? "en" : "pt"
                    text: other === "en" ? "English" : "Português (Brasil)"
                    color: languageMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary
                    font.pixelSize: 13
                    font.underline: languageMouse.containsMouse
                    MouseArea {
                        id: languageMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            App.setLanguage(parent.other)
                            App.restart()
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: if (clientId.text !== "") checkTimer.restart()

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

    // Um campo do formulário do Spotify, pronto para copiar.
    component CopyRow: Row {
        id: copyRow
        property string label
        property string value
        property bool copied: false
        x: 32
        spacing: 8
        Label {
            width: 118
            anchors.verticalCenter: parent.verticalCenter
            text: copyRow.label
            color: Theme.textSecondary
            font.pixelSize: 13
        }
        Rectangle {
            width: valueLabel.implicitWidth + 20
            height: 32
            radius: 4
            color: Theme.panel
            Label {
                id: valueLabel
                anchors.centerIn: parent
                text: copyRow.value
                color: Theme.accentText
                font.family: "Consolas"
                font.pixelSize: 13
            }
        }
        Chip {
            anchors.verticalCenter: parent.verticalCenter
            text: copyRow.copied ? qsTr("Copiado!") : qsTr("Copiar")
            onClicked: { App.copyToClipboard(copyRow.value); copyRow.copied = true }
        }
    }
}
