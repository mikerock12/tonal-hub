pragma Singleton
import QtQuick

// Todas as cores e medidas do app num lugar só. As cores vêm do tema escolhido
// (App.theme, lido de resources/themes.json, o mesmo catálogo do Android);
// trocar o tema atualiza todas as telas na hora, sem reiniciar.
QtObject {
    // Tema "Tonal Azul" embutido: vale se o catálogo não carregar (e nos testes de QML).
    readonly property var fallback: ({
        id: "tonal", name: "Tonal Azul",
        bg: "#000000", panel: "#121212", card: "#1f1f1f", cardHover: "#2a2a2a",
        text: "#ffffff", textSecondary: "#b3b3b3", textMuted: "#8a8a8a",
        accent: "#0d80bf", accentHover: "#3193c9", accentText: "#4aa0cf",
        accentDeep: "#0a3d5c", onAccent: "#000000", divider: "#2a2a2a",
        seek: { height: 4, activeHeight: 6, track: "#4d4d4d", fill: ["#ffffff", "#ffffff"],
                activeFill: ["#0d80bf", "#3193c9"], thumb: "#ffffff", thumbBorder: "#ffffff",
                thumbSize: 12, thumbShape: "circle", thumbAlways: false, glow: false }
    })
    readonly property var palette: App.theme && App.theme.current && App.theme.current.id ? App.theme.current : fallback
    readonly property var themes: App.theme && App.theme.themes && App.theme.themes.length ? App.theme.themes : [fallback]
    readonly property string themeId: palette.id
    function setTheme(id) { if (App.theme) App.theme.themeId = id }

    // Destaque do tema: botões, barras, pontos, ícones ativos. No Tonal Azul, o texto em destaque usa
    // um azul mais claro (6,4:1 no painel) e o texto sobre o destaque é preto (4,9:1).
    readonly property color accent: palette.accent
    readonly property color accentHover: palette.accentHover
    readonly property color accentText: palette.accentText
    readonly property color accentDeep: palette.accentDeep    // fundos tingidos (degradê da Home, Músicas Curtidas)
    readonly property color textOnAccent: palette.onAccent

    readonly property color windowBg: palette.bg
    readonly property color panel: palette.panel
    readonly property color elevated: palette.card
    readonly property color elevatedHover: palette.cardHover
    readonly property color rowHover: Qt.rgba(1, 1, 1, 0.1)
    readonly property color cardHover: Qt.rgba(1, 1, 1, 0.07)
    readonly property color tile: Qt.rgba(1, 1, 1, 0.07)
    readonly property color tileHover: Qt.rgba(1, 1, 1, 0.16)
    readonly property color divider: Qt.rgba(1, 1, 1, 0.1)
    readonly property color trackBg: palette.seek.track

    readonly property color textPrimary: palette.text
    readonly property color textSecondary: palette.textSecondary
    readonly property color textMuted: palette.textMuted
    readonly property color danger: "#f15e6c"

    // Barra de progresso/volume e o marcador (thumb) de cada tema.
    readonly property var seek: palette.seek

    readonly property int radiusPanel: 8
    readonly property int radiusCard: 6
    readonly property int radiusCover: 4

    readonly property int gap: 8
    readonly property int playerBarHeight: 80
    readonly property int topBarHeight: 64

    function formatTime(ms) {
        if (!ms || ms < 0) ms = 0
        const total = Math.floor(ms / 1000)
        const h = Math.floor(total / 3600)
        const m = Math.floor((total % 3600) / 60)
        const s = total % 60
        const ss = s < 10 ? "0" + s : "" + s
        if (h > 0)
            return h + ":" + (m < 10 ? "0" + m : m) + ":" + ss
        return m + ":" + ss
    }

    function formatLongDuration(ms) {
        const totalMin = Math.round(ms / 60000)
        const h = Math.floor(totalMin / 60)
        const m = totalMin % 60
        if (h > 0)
            return qsTr("%1 h %2 min").arg(h).arg(m)
        return qsTr("%1 min").arg(m)
    }

    // Data no idioma do app (Qt.locale() segue o idioma escolhido): "1 de ago. de 2026" / "Aug 1, 2026";
    // com longMonth, "1 de agosto de 2026" / "August 1, 2026".
    function formatDate(date, longMonth) {
        return date.toLocaleDateString(Qt.locale(), longMonth ? qsTr("d 'de' MMMM 'de' yyyy")
                                                              : qsTr("d 'de' MMM 'de' yyyy"))
    }

    function formatAdded(iso) {
        if (!iso) return ""
        const d = new Date(iso)
        const days = Math.floor((Date.now() - d.getTime()) / 86400000)
        if (days < 1) return qsTr("hoje")
        if (days === 1) return qsTr("ontem")
        if (days < 7) return qsTr("há %1 dias").arg(days)
        if (days < 30) {
            const w = Math.floor(days / 7)
            return w === 1 ? qsTr("há 1 semana") : qsTr("há %1 semanas").arg(w)
        }
        return formatDate(d, false)
    }

    function bestImage(images, minWidth) {
        if (!images || images.length === 0) return ""
        let best = ""
        let bestW = 1e9
        for (let i = 0; i < images.length; ++i) {
            const w = images[i].width || 0
            if (w === 0 && best === "") best = images[i].url
            if (w >= minWidth && w < bestW) { best = images[i].url; bestW = w }
        }
        return best !== "" ? best : images[0].url
    }

    function joinNames(list) {
        if (!list) return ""
        return list.map(a => a.name).join(", ")
    }

    function idFromUri(uri) {
        if (!uri) return ""
        const parts = uri.split(":")
        return parts[parts.length - 1]
    }
}
