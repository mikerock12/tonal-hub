pragma Singleton
import QtQuick

// Criar e editar playlists, e a permissão que isso exige. As telas só chamam estas funções; Sua Biblioteca e a
// página aberta se atualizam pelos sinais. Endpoints de fev/2026: POST /me/playlists, PUT /playlists/{id},
// POST/DELETE/PUT /playlists/{id}/items e DELETE /me/library (excluir).
QtObject {
    id: playlists

    // Playlists em que dá para pôr músicas (suas ou colaborativas): [{ id, uri, name }].
    // Quem preenche é a LibraryPanel, que já busca /me/playlists.
    property var editable: []

    // Ação que espera a pessoa autorizar o Tonal de novo (ver withPermission).
    property var pending: null

    // A lista de playlists mudou (criou, renomeou, excluiu): a LibraryPanel busca de novo.
    signal listChanged()
    // As faixas de uma playlist mudaram fora da página dela (ex.: "Adicionar à playlist").
    signal contentChanged(string playlistId)
    // Falta a permissão de editar playlists: o Main.qml mostra o aviso.
    signal permissionNeeded()
    signal textImportRequested()

    // Quem entrou antes de o Tonal pedir playlist-modify-private não pode editar. A ação fica guardada e roda
    // quando a nova autorização chegar (o Main.qml chama runPending).
    function withPermission(action) {
        if (App.auth.canEditPlaylists) {
            action()
            return
        }
        pending = action
        permissionNeeded()
    }

    function runPending() {
        const action = pending
        pending = null
        if (action) action()
    }

    function ok(status) { return status >= 200 && status < 300 }

    // Cria "Minha playlist nº N", privada, e devolve a playlist ao callback.
    function create(callback) {
        withPermission(function() {
            const name = qsTr("Minha playlist nº %1").arg(playlists.editable.length + 1)
            App.api.call("POST", "/me/playlists", {}, { name: name, "public": false }, function(status, p) {
                if (!playlists.ok(status) || !p || !p.id) {
                    Nav.toast(qsTr("Não consegui criar a playlist"))
                    return
                }
                playlists.listChanged()
                if (callback) callback(p)
            })
        })
    }

    // fields: { name, description }. Sem "public": a API aceita tornar pública, mas ignora (com 200) o
    // caminho de volta, então o Tonal não oferece a troca. As playlists nascem privadas.
    function update(id, fields, callback) {
        withPermission(function() {
            App.api.call("PUT", "/playlists/" + id, {}, fields, function(status) {
                const done = playlists.ok(status)
                if (done) playlists.listChanged()
                else Nav.toast(qsTr("Não consegui salvar a playlist"))
                if (callback) callback(done)
            })
        })
    }

    // Excluir a própria playlist é tirá-la da biblioteca, como no app oficial.
    function remove(uri, callback) {
        withPermission(function() {
            App.api.call("DELETE", "/me/library", { uris: uri }, null, function(status) {
                const done = playlists.ok(status)
                Nav.toast(done ? qsTr("Playlist excluída") : qsTr("Não consegui excluir a playlist"))
                if (done) playlists.listChanged()
                if (callback) callback(done)
            })
        })
    }

    // playlist: { id, name }
    function addTracks(playlist, uris) {
        withPermission(function() {
            App.api.call("POST", "/playlists/" + playlist.id + "/items", {}, { uris: uris }, function(status) {
                const done = playlists.ok(status)
                Nav.toast(done ? qsTr("Adicionada a %1").arg(playlist.name) : qsTr("Não consegui adicionar à playlist"))
                if (done) playlists.contentChanged(playlist.id)
            })
        })
    }

    function createWithTracks(uris) {
        create(function(p) { playlists.addTracks({ id: p.id, name: p.name }, uris) })
    }

    // Tira todas as ocorrências da faixa. callback(ok, snapshotId)
    function removeTrack(id, uri, snapshotId, callback) {
        withPermission(function() {
            const body = { items: [{ uri: uri }] }
            if (snapshotId) body.snapshot_id = snapshotId
            App.api.call("DELETE", "/playlists/" + id + "/items", {}, body, function(status, data) {
                const done = playlists.ok(status)
                Nav.toast(done ? qsTr("Removida desta playlist") : qsTr("Não consegui remover a música"))
                if (callback) callback(done, done && data ? (data.snapshot_id || "") : "")
            })
        })
    }

    // insertBefore conta as posições de antes da mudança (mover a 1ª para o fim de 10 faixas: 0 → 10).
    // callback(ok, snapshotId)
    function moveTrack(id, from, insertBefore, snapshotId, callback) {
        withPermission(function() {
            const body = { range_start: from, insert_before: insertBefore, range_length: 1 }
            if (snapshotId) body.snapshot_id = snapshotId
            App.api.call("PUT", "/playlists/" + id + "/items", {}, body, function(status, data) {
                const done = playlists.ok(status)
                if (!done) Nav.toast(qsTr("Não consegui mover a música"))
                if (callback) callback(done, done && data ? (data.snapshot_id || "") : "")
            })
        })
    }
}
