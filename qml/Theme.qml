pragma Singleton
import QtQuick

// Todas as cores e medidas do app num lugar só.
QtObject {
    // Cor de destaque do Tonal. Troque aqui para mudar a identidade do app inteiro.
    // Paleta: azul, preto e branco (o preto e o branco já são o fundo e o texto).
    readonly property color accent: "#0d80bf"        // azul de destaque: botões, barras, pontos, ícones ativos
    readonly property color accentHover: "#3193c9"
    // Texto em destaque sobre o fundo escuro: o #0d80bf dá só 4,3:1 no #121212 (abaixo de 4,5 para texto
    // pequeno), então o texto usa um azul mais claro do mesmo tom (6,4:1).
    readonly property color accentText: "#4aa0cf"
    readonly property color accentDeep: "#0a3d5c"    // fundos tingidos (degradê da Home, Músicas Curtidas)
    readonly property color textOnAccent: "#000000"  // preto sobre o azul: 4,9:1 (branco daria 4,3:1)

    readonly property color windowBg: "#000000"
    readonly property color panel: "#121212"
    readonly property color elevated: "#1f1f1f"
    readonly property color elevatedHover: "#2a2a2a"
    readonly property color rowHover: Qt.rgba(1, 1, 1, 0.1)
    readonly property color cardHover: Qt.rgba(1, 1, 1, 0.07)
    readonly property color tile: Qt.rgba(1, 1, 1, 0.07)
    readonly property color tileHover: Qt.rgba(1, 1, 1, 0.16)
    readonly property color divider: Qt.rgba(1, 1, 1, 0.1)
    readonly property color trackBg: "#4d4d4d"

    readonly property color textPrimary: "#ffffff"
    readonly property color textSecondary: "#b3b3b3"
    readonly property color textMuted: "#8a8a8a"
    readonly property color danger: "#f15e6c"

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
