import QtQuick
import QtQuick.Controls
import "qrc:/companion/qml/companion" as Personal

// Barra superior: logo, voltar/avançar, Início, busca e menu da conta.
Item {
    id: root
    signal audioSettingsRequested()
    signal musicRequested()

    function focusSearch() {
        search.forceActiveFocus()
        search.selectAll()
    }

    // ---------------------------------------------------- esquerda
    Row {
        id:leftNavigation
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Logo {
            size: 32
            anchors.verticalCenter: parent.verticalCenter
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: App.hub.hub() }
        }

        Item { width: 4; height: 1 }

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            width: 32; height: 32
            circle: true
            background: Qt.rgba(0, 0, 0, 0.7)
            backgroundHover: Theme.elevated
            iconSize: 16
            strokePath: Icons.chevronLeftS
            strokeWidth: 2.4
            enabled: Nav.canBack
            tooltip: qsTr("Voltar")
            onClicked: Nav.back()
        }
        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            width: 32; height: 32
            circle: true
            background: Qt.rgba(0, 0, 0, 0.7)
            backgroundHover: Theme.elevated
            iconSize: 16
            strokePath: Icons.chevronRightS
            strokeWidth: 2.4
            enabled: Nav.canForward
            tooltip: qsTr("Avançar")
            onClicked: Nav.forward()
        }
    }

    // ---------------------------------------------------- centro
    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset:(leftNavigation.width-rightTools.width)/2
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconButton {
            width: 48; height: 48
            circle: true
            background: Theme.elevated
            backgroundHover: Theme.elevatedHover
            iconSize: 24
            readonly property bool atHome: Nav.current.page === "home"
            fillPath: atHome ? Icons.homeF : ""
            strokePath: atHome ? "" : Icons.homeS
            baseColor: atHome ? Theme.textPrimary : Theme.textSecondary
            tooltip: qsTr("Início")
            onClicked: Nav.go("home")
        }

        IconButton {
            width: 48; height: 48
            circle: true
            background: Theme.elevated
            backgroundHover: Theme.elevatedHover
            iconSize: 22
            strokeWidth: 1.8
            readonly property bool atSetlists: Nav.current.page === "setlists"
            strokePath: Icons.setlistS
            baseColor: atSetlists ? Theme.textPrimary : Theme.textSecondary
            tooltip: qsTr("Setlists: criar playlist a partir de shows do setlist.fm")
            onClicked: Nav.go("setlists")
        }

        TextField {
            id: search
            width: Math.min(474, Math.max(120, root.width-leftNavigation.width-rightTools.width-196))
            height: 48
            leftPadding: 48
            rightPadding: 40
            placeholderText: qsTr("O que você quer ouvir?")
            placeholderTextColor: Theme.textMuted
            color: Theme.textPrimary
            font.pixelSize: 16
            selectByMouse: true
            selectionColor: Theme.accent
            selectedTextColor: Theme.textOnAccent
            verticalAlignment: TextInput.AlignVCenter

            background: Rectangle {
                radius: height / 2
                color: search.hovered || search.activeFocus ? Theme.elevatedHover : Theme.elevated
                border.width: search.activeFocus ? 2 : (search.hovered ? 1 : 0)
                border.color: search.activeFocus ? Theme.textPrimary : Qt.rgba(1, 1, 1, 0.15)

                Icon {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    size: 22
                    strokePath: Icons.searchS
                    color: search.activeFocus ? Theme.textPrimary : Theme.textSecondary
                }
            }

            IconButton {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: search.text.length > 0
                iconSize: 16
                strokePath: Icons.closeS
                tooltip: qsTr("Limpar")
                onClicked: { search.clear(); search.forceActiveFocus() }
            }

            onActiveFocusChanged: if (activeFocus && Nav.current.page !== "search") Nav.go("search", { query: text })
            onTextEdited: debounce.restart()
            Keys.onEscapePressed: { focus = false }
            Keys.onReturnPressed: { debounce.stop(); Nav.go("search", { query: text.trim() }) }

            Timer {
                id: debounce
                interval: 350
                onTriggered: Nav.go("search", { query: search.text.trim() })
            }

            // Mantém o campo coerente ao navegar pelo histórico
            Connections {
                target: Nav
                function onCurrentChanged() {
                    if (Nav.current.page === "search") {
                        if (!search.activeFocus)
                            search.text = Nav.current.params.query || ""
                    } else {
                        search.focus = false
                        search.text = ""
                    }
                }
            }
        }
    }

    // ---------------------------------------------------- direita
    Row {
        id:rightTools
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        // Aviso do motor de áudio quando ele não está pronto
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            visible: App.librespot.state !== Librespot.Ready && root.width > 1100
            height: 32
            radius: 16
            width: Math.min(260, engineLabel.implicitWidth + 36)
            color: Theme.elevated
            Rectangle {
                id: dot
                width: 8; height: 8; radius: 4
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                color: App.librespot.state === Librespot.Starting || App.librespot.state === Librespot.NeedsLogin
                       ? Theme.accent : Theme.danger
            }
            Label {
                id: engineLabel
                anchors.left: dot.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    switch (App.librespot.state) {
                    case Librespot.Starting: return qsTr("Iniciando áudio...")
                    case Librespot.NeedsLogin: return qsTr("Autorize o áudio no navegador")
                    case Librespot.NotFound: return qsTr("librespot não encontrado")
                    case Librespot.Crashed: return qsTr("Áudio parou")
                    default: return qsTr("Áudio desligado")
                    }
                }
                color: Theme.textSecondary
                font.pixelSize: 13
                elide: Text.ElideRight
            }
            ToolTip.visible: engineMouse.containsMouse
            ToolTip.text: App.librespot.statusText
            MouseArea {
                id: engineMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: accountMenu.popup(parent, 0, parent.height + 8)
            }
        }

        Personal.ActionIcon{anchors.verticalCenter:parent.verticalCenter;symbol:"heart";description:"Biblioteca pessoal, estatísticas e letras";onClicked:root.musicRequested()}
        Personal.ActionIcon{anchors.verticalCenter:parent.verticalCenter;symbol:"audio";description:"Áudio e efeitos";onClicked:root.audioSettingsRequested()}

        // Avatar + menu da conta
        Rectangle {
            id: avatarButton
            width: 48; height: 48
            radius: 24
            color: avatarMouse.containsMouse ? Theme.elevatedHover : Theme.elevated
            anchors.verticalCenter: parent.verticalCenter

            Cover {
                anchors.centerIn: parent
                width: 32; height: 32
                round: true
                source: Nav.me && Nav.me.images && Nav.me.images.length > 0 ? Theme.bestImage(Nav.me.images, 64) : ""
                visible: source.toString() !== ""
            }
            Rectangle {
                anchors.centerIn: parent
                width: 32; height: 32; radius: 16
                visible: !(Nav.me && Nav.me.images && Nav.me.images.length > 0)
                color: Theme.accent
                Label {
                    anchors.centerIn: parent
                    text: Nav.me && Nav.me.display_name ? Nav.me.display_name.charAt(0).toUpperCase() : "?"
                    color: Theme.textOnAccent
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }
            }
            MouseArea {
                id: avatarMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: accountMenu.popup(avatarButton, avatarButton.width - accountMenu.width, avatarButton.height + 8)
            }
            ToolTip.visible: avatarMouse.containsMouse
            ToolTip.text: Nav.me ? (Nav.me.display_name || "") : ""
            ToolTip.delay: 700
        }
    }

    Menu {
        id: accountMenu
        width: 280
        background: Rectangle { color: "#282828"; radius: 4 }

        MenuLabel { text: Nav.me ? qsTr("Conectado como %1").arg(Nav.me.display_name || Nav.me.id) : "" }
        MenuLabel {
            text: qsTr("Motor de áudio: %1").arg(App.librespot.statusText || qsTr("desligado"))
            secondary: true
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.divider } }
        AccountItem { text: qsTr("Áudio e efeitos…"); onTriggered: root.audioSettingsRequested() }
        AccountItem { text: qsTr("Biblioteca pessoal, estatísticas e letras…"); onTriggered: root.musicRequested() }
        AccountItem {
            text: qsTr("Qualidade: 320 kbps (muito alta)")
            checked: App.librespot.bitrate === 320
            onTriggered: App.librespot.bitrate = 320
        }
        AccountItem {
            text: qsTr("Qualidade: 160 kbps (alta)")
            checked: App.librespot.bitrate === 160
            onTriggered: App.librespot.bitrate = 160
        }
        AccountItem {
            text: qsTr("Qualidade: 96 kbps (economia)")
            checked: App.librespot.bitrate === 96
            onTriggered: App.librespot.bitrate = 96
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.divider } }
        AccountItem { text: qsTr("Reiniciar motor de áudio"); onTriggered: App.librespot.restart() }
        AccountItem { text: qsTr("Refazer login do motor de áudio"); onTriggered: App.librespot.resetLogin() }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.divider } }
        MenuLabel { text: qsTr("Idioma"); secondary: true }
        // Cada idioma com o próprio nome, para quem não lê o idioma atual achar o seu.
        AccountItem {
            text: "Português (Brasil)"
            checked: root.chosenLanguage === "pt"
            onTriggered: root.chooseLanguage("pt")
        }
        AccountItem {
            text: "English"
            checked: root.chosenLanguage === "en"
            onTriggered: root.chooseLanguage("en")
        }
        MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.divider } }
        AccountItem { text: qsTr("Sair"); onTriggered: App.auth.logout() }
    }

    // Idioma marcado no menu: o em uso ou, depois de uma troca, o que vale no próximo início.
    property string chosenLanguage: App.language

    function chooseLanguage(language) {
        if (language === chosenLanguage) return
        chosenLanguage = language
        App.setLanguage(language)
        languageDialog.open()
    }

    ModalDialog {
        id: languageDialog
        title: qsTr("Reiniciar o Tonalify?")

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: Theme.textSecondary
            font.pixelSize: 14
            text: qsTr("O idioma novo vale a partir do próximo início. Se reiniciar agora, a música para por alguns segundos.")
        }

        buttons: [
            PillButton {
                primary: false
                text: qsTr("Depois")
                onClicked: languageDialog.close()
            },
            PillButton {
                text: qsTr("Reiniciar agora")
                onClicked: App.restart()
            }
        ]
    }

    component MenuLabel: Label {
        property bool secondary: false
        width: parent ? parent.width : 280
        padding: 12
        wrapMode: Text.WordWrap
        color: secondary ? Theme.textSecondary : Theme.textPrimary
        font.pixelSize: secondary ? 12 : 14
        font.weight: secondary ? Font.Normal : Font.DemiBold
    }

    component AccountItem: MenuItem {
        id: item
        implicitHeight: 40
        contentItem: Row {
            spacing: 8
            leftPadding: 4
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                size: 16
                strokePath: Icons.checkS
                color: Theme.accent
                opacity: item.checked ? 1 : 0
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: item.text
                color: Theme.textPrimary
                font.pixelSize: 14
            }
        }
        indicator: null
        background: Rectangle {
            color: item.highlighted ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
            radius: 2
        }
    }
}
