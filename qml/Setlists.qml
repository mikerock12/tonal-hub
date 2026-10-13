pragma Singleton
import QtQuick

// Sessão Setlists: shows do setlist.fm viram uma playlist (o fluxo do setlist-spotify). A tela mostra só a
// última busca, mas os shows marcados continuam marcados ao buscar outra banda: é assim que se juntam bandas
// diferentes. As músicas repetidas entre os shows entram uma vez só, e cada música vira uma busca no Spotify.
// O estado fica aqui, e não na página, para sobreviver à navegação (a página é destruída ao sair dela).
QtObject {
    id: setlists

    readonly property int maxNameLength: 100      // a API aceita mais, mas o app do Spotify corta
    readonly property int maxDescription: 300     // limite da Web API
    readonly property int searchConcurrency: 5    // buscas no Spotify ao mesmo tempo
    readonly property var examples: ["Radiohead", "Metallica", "Foo Fighters", "Charlie Brown Jr."]

    // Última busca: a banda pesquisada e os shows achados (cada busca nova substitui a anterior).
    property string query: ""
    property var results: []
    // Shows marcados, por id. Guarda o show inteiro: os de buscas anteriores continuam aqui e entram na playlist.
    // Cada mudança atribui um objeto novo, para os bindings verem.
    property var selected: ({})
    readonly property int selectedCount: Object.keys(selected).length
    readonly property int selectedSongs: {
        let total = 0
        for (const id in selected) total += selected[id].songs.length
        return total
    }

    property bool searching: false
    property string searchError: ""

    // Enquanto a pessoa não edita, o nome acompanha a sugestão; depois da primeira edição, vale o dela.
    property string customName: ""
    property bool nameEdited: false
    readonly property string suggestedName: selectedCount > 0 ? buildName(selectedList()) : ""
    readonly property string playlistName: nameEdited ? customName : suggestedName

    property bool creating: false
    property string progress: ""
    property string createError: ""
    property var result: null    // { id, name, found, total, songsTotal, showsCount }

    // ------------------------------------------------------------ busca e seleção
    function search(text) {
        text = (text || "").trim()
        if (text === "" || searching) return
        // A busca anterior sai da tela já; o que estava marcado nela continua marcado.
        query = text
        results = []
        searching = true
        searchError = ""
        App.setlistFm.searchSetlists(text, function(error, list) {
            if (text !== setlists.query) return // outra busca começou depois
            setlists.searching = false
            if (error) {
                setlists.searchError = error
                return
            }
            if (!list || list.length === 0) {
                setlists.searchError = qsTr("Nenhum setlist com músicas para “%1”.").arg(text)
                return
            }
            setlists.results = list
        })
    }

    // Ao sair da página: a busca some, os shows marcados ficam. Uma busca em andamento é descartada (a resposta
    // dela vê que a busca mudou e não é mostrada).
    function clearSearch() {
        query = ""
        results = []
        searching = false
        searchError = ""
    }

    function find(id) {
        return results.find(s => s.id === id) || selected[id] || null
    }

    function selectedList() {
        return Object.keys(selected).map(id => selected[id])
    }

    function selectedInResults() {
        return results.filter(s => selected[s.id] !== undefined).length
    }

    // Marcados de buscas anteriores: não aparecem na lista, então a barra de baixo mostra quais são.
    function fromOtherSearches() {
        const visible = {}
        for (const s of results) visible[s.id] = true
        return selectedList().filter(s => !visible[s.id])
    }

    function toggle(show) {
        const next = Object.assign({}, selected)
        if (next[show.id]) delete next[show.id]
        else next[show.id] = show
        selected = next
        result = null
    }

    function selectAll(shows) {
        const next = Object.assign({}, selected)
        for (const s of shows) next[s.id] = s
        selected = next
        result = null
    }

    function deselect(shows) {
        const next = Object.assign({}, selected)
        for (const s of shows) delete next[s.id]
        selected = next
        result = null
    }

    function clearSelection() {
        selected = ({})
        customName = ""
        nameEdited = false
        result = null
        createError = ""
    }

    // ------------------------------------------------------------ datas, nome e descrição
    // "1 música", "12 músicas" (o app não tem arquivos de tradução, então o %n do qsTr não resolve o plural).
    function plural(n, one, many) {
        return n + " " + (n === 1 ? one : many)
    }

    // eventDate do setlist.fm vem como dd-MM-yyyy.
    function timestamp(eventDate) {
        const p = eventDate.split("-")
        return new Date(Number(p[2]), (Number(p[1]) || 1) - 1, Number(p[0])).getTime()
    }

    // "01-08-2026" → "1 de ago. de 2026"
    function formatDate(eventDate) {
        const p = eventDate.split("-")
        if (p.length !== 3 || !Theme.monthNames[Number(p[1]) - 1]) return eventDate
        return Number(p[0]) + " de " + Theme.monthNames[Number(p[1]) - 1] + " de " + p[2]
    }

    // Do show mais antigo para o mais recente: a ordem em que a playlist toca.
    function sortByDate(shows) {
        return shows.slice().sort((a, b) => timestamp(a.eventDate) - timestamp(b.eventDate))
    }

    function buildName(shows) {
        const ordered = sortByDate(shows)
        if (ordered.length === 1) {
            const s = ordered[0]
            return (s.artist + " - " + s.venue + " (" + s.eventDate + ")").slice(0, maxNameLength)
        }
        const artists = []
        for (const s of ordered) if (artists.indexOf(s.artist) < 0) artists.push(s.artist)
        // Com poucas bandas, listar quem toca diz mais; acima disso vira uma parede de texto.
        const who = artists.length === 1 ? artists[0]
                  : artists.length <= 3 ? artists.slice(0, -1).join(", ") + " e " + artists[artists.length - 1]
                  : qsTr("%1 bandas").arg(artists.length)
        return (who + " - " + qsTr("%1 shows").arg(ordered.length)).slice(0, maxNameLength)
    }

    function buildDescription(shows) {
        const ordered = sortByDate(shows)
        const signature = " " + qsTr("Criada no Tonal a partir do setlist.fm.")
        const place = s => s.artist + " " + qsTr("em") + " " + s.venue + ", " + s.city + " (" + s.eventDate + ")"
        const body = ordered.length === 1
            ? qsTr("Setlist de %1.").arg(place(ordered[0]))
            : qsTr("Setlists de %1.").arg(ordered.map(place).join("; "))
        if ((body + signature).length <= maxDescription) return body + signature
        return body.slice(0, maxDescription - signature.length - 1).trim() + "…" + signature
    }

    // Uma busca por música, na ordem da playlist, sem repetir a mesma música de shows diferentes. A chave inclui
    // o artista: bandas diferentes com música de mesmo nome (ou covers) não se anulam.
    function dedupe(shows) {
        const seen = {}
        const queries = []
        for (const show of sortByDate(shows)) {
            for (const name of show.songs) {
                const key = (show.artist + "::" + name).toLowerCase()
                if (seen[key]) continue
                seen[key] = true
                queries.push({ name: name, artist: show.artist })
            }
        }
        return queries
    }

    // ------------------------------------------------------------ criar a playlist
    function create() {
        const shows = sortByDate(selectedList())
        if (shows.length === 0 || creating) return
        const name = playlistName.trim().slice(0, maxNameLength) || buildName(shows)
        const description = buildDescription(shows)
        const queries = dedupe(shows)
        const songsTotal = shows.reduce((total, s) => total + s.songs.length, 0)

        Playlists.withPermission(function() {
            setlists.creating = true
            setlists.createError = ""
            setlists.result = null
            const uris = new Array(queries.length)
            let started = 0
            let finished = 0
            const report = function() {
                setlists.progress = qsTr("Procurando as músicas no Spotify: %1 de %2").arg(finished).arg(queries.length)
            }
            // Pool fixo de buscas: várias dezenas de uma vez estourariam a cota do app.
            const next = function() {
                if (started >= queries.length) return
                const index = started++
                setlists.findTrack(queries[index], function(uri) {
                    uris[index] = uri
                    ++finished
                    report()
                    if (finished === queries.length)
                        setlists.finishCreate(name, description, uris.filter(u => !!u), queries.length, songsTotal, shows.length)
                    else
                        next()
                })
            }
            report()
            for (let i = 0; i < Math.min(searchConcurrency, queries.length); ++i) next()
        })
    }

    function firstUri(status, data) {
        if (status !== 200 || !data || !data.tracks || !data.tracks.items || data.tracks.items.length === 0)
            return ""
        return data.tracks.items[0] ? data.tracks.items[0].uri : ""
    }

    // Busca com filtros de faixa e artista e, se não achar, em texto livre.
    function findTrack(query, done) {
        App.api.call("GET", "/search", { q: "track:" + query.name + " artist:" + query.artist, type: "track", limit: 1 },
                     null, function(status, data) {
            const uri = setlists.firstUri(status, data)
            if (uri) {
                done(uri)
                return
            }
            App.api.call("GET", "/search", { q: query.name + " " + query.artist, type: "track", limit: 1 }, null,
                         function(status2, data2) { done(setlists.firstUri(status2, data2)) })
        })
    }

    function finishCreate(name, description, uris, total, songsTotal, showsCount) {
        if (uris.length === 0) {
            fail(showsCount === 1 ? qsTr("Nenhuma música do setlist foi encontrada no Spotify.")
                                  : qsTr("Nenhuma música dos setlists marcados foi encontrada no Spotify."))
            return
        }
        progress = qsTr("Criando a playlist…")
        App.api.call("POST", "/me/playlists", {}, { name: name, description: description, "public": false },
                     function(status, p) {
            if (status < 200 || status >= 300 || !p || !p.id) {
                setlists.fail(qsTr("O Spotify recusou a criação da playlist (erro %1).").arg(status))
                return
            }
            setlists.addTracks(p, uris, 0, total, songsTotal, showsCount)
        })
    }

    // Em lotes de 100, o máximo da API.
    function addTracks(playlist, uris, from, total, songsTotal, showsCount) {
        if (from >= uris.length) {
            creating = false
            progress = ""
            result = { id: playlist.id, name: playlist.name, found: uris.length, total: total,
                       songsTotal: songsTotal, showsCount: showsCount }
            Playlists.listChanged()
            return
        }
        App.api.call("POST", "/playlists/" + playlist.id + "/items", {}, { uris: uris.slice(from, from + 100) },
                     function(status) {
            if (status < 200 || status >= 300) {
                setlists.fail(qsTr("A playlist foi criada, mas o Spotify recusou as músicas (erro %1).").arg(status))
                Playlists.listChanged()
                return
            }
            setlists.addTracks(playlist, uris, from + 100, total, songsTotal, showsCount)
        })
    }

    function fail(message) {
        creating = false
        progress = ""
        createError = message
    }
}
