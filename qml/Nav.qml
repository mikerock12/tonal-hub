pragma Singleton
import QtQuick

// Navegação (com histórico de voltar/avançar) e estado compartilhado entre as telas.
QtObject {
    id: nav

    // Rotas: home | search {query} | playlist {id} | album {id} | liked | artist {id}
    property var history: [{ page: "home", params: {} }]
    property int index: 0
    readonly property var current: history[index]
    readonly property bool canBack: index > 0
    readonly property bool canForward: index < history.length - 1

    // Perfil do usuário logado (GET /me)
    property var me: null
    // Painel direito: "" (fechado) | "queue" | "nowplaying"
    property string rightPanel: ""

    signal toast(string text)
    // Pede o menu de contexto de faixa. track = { uri, name, albumId, artists: [{id,name}] }
    signal trackMenuRequested(var track, Item item, real x, real y)

    function go(page, params) {
        params = params || {}
        const cur = history[index]
        if (cur.page === page && JSON.stringify(cur.params) === JSON.stringify(params))
            return
        // Busca: digitar não deve empilhar uma entrada por letra
        if (page === "search" && cur.page === "search") {
            const h = history.slice()
            h[index] = { page: page, params: params }
            history = h
            return
        }
        const h = history.slice(0, index + 1)
        h.push({ page: page, params: params })
        if (h.length > 50) h.shift()
        history = h
        index = h.length - 1
    }

    function back() { if (canBack) index-- }
    function forward() { if (canForward) index++ }

    function openUri(uri) {
        if (!uri) return
        const p = uri.split(":")
        const id = p[p.length - 1]
        if (uri.endsWith(":collection")) go("liked")
        else if (p[1] === "playlist") go("playlist", { id: id })
        else if (p[1] === "album") go("album", { id: id })
        else if (p[1] === "artist") go("artist", { id: id })
    }

    function likedSongsUri() {
        return me ? "spotify:user:" + me.id + ":collection" : ""
    }

    function loadMe() {
        App.api.call("GET", "/me", {}, null, function(status, data) {
            if (status === 200) nav.me = data
        })
    }
}
