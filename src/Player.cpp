#include "Player.h"

#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "SpotifyApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

#include <climits>

namespace {

// Escolhe a menor imagem com largura >= minWidth (as imagens vêm da maior para a menor).
QString pickImage(const QJsonArray &images, int minWidth)
{
    QString best;
    int bestWidth = INT_MAX;
    for (const QJsonValue &v : images) {
        const QJsonObject img = v.toObject();
        const int w = img.value(QStringLiteral("width")).toInt(0);
        const QString url = img.value(QStringLiteral("url")).toString();
        if (w == 0 && best.isEmpty())
            best = url; // imagens de playlist às vezes vêm sem tamanho
        if (w >= minWidth && w < bestWidth) {
            best = url;
            bestWidth = w;
        }
    }
    if (best.isEmpty() && !images.isEmpty())
        best = images.first().toObject().value(QStringLiteral("url")).toString();
    return best;
}

QString joinArtists(const QJsonArray &artists)
{
    QStringList names;
    for (const QJsonValue &a : artists)
        names << a.toObject().value(QStringLiteral("name")).toString();
    return names.join(QStringLiteral(", "));
}

QVariantMap simplifyItem(const QJsonObject &item)
{
    QVariantMap m;
    m.insert(QStringLiteral("name"), item.value(QStringLiteral("name")).toString());
    m.insert(QStringLiteral("uri"), item.value(QStringLiteral("uri")).toString());
    m.insert(QStringLiteral("durationMs"), item.value(QStringLiteral("duration_ms")).toInt());
    if (item.contains(QStringLiteral("album"))) {
        const QJsonObject album = item.value(QStringLiteral("album")).toObject();
        m.insert(QStringLiteral("artists"), joinArtists(item.value(QStringLiteral("artists")).toArray()));
        m.insert(QStringLiteral("coverUrl"), pickImage(album.value(QStringLiteral("images")).toArray(), 64));
    } else { // episódio de podcast
        const QJsonObject show = item.value(QStringLiteral("show")).toObject();
        m.insert(QStringLiteral("artists"), show.value(QStringLiteral("name")).toString());
        m.insert(QStringLiteral("coverUrl"), pickImage(item.value(QStringLiteral("images")).toArray(), 64));
    }
    return m;
}

} // namespace

Player::Player(SpotifyApi *api, LibrespotProcess *librespot, AuthManager *auth, QObject *parent)
    : QObject(parent), m_api(api), m_librespot(librespot), m_auth(auth)
{
    m_volume = QSettings().value(QStringLiteral("player/volume"), 60).toInt();

    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &Player::refresh);

    // Só redesenha a barra de progresso enquanto toca.
    m_tickTimer.setInterval(500);
    connect(&m_tickTimer, &QTimer::timeout, this, [this] {
        emit positionChanged();
        if (m_durationMs > 0 && positionMs() >= m_durationMs)
            refreshSoon(300);
    });

    // Volume: só manda para a API quando o usuário para de arrastar.
    m_volumeTimer.setSingleShot(true);
    m_volumeTimer.setInterval(250);
    connect(&m_volumeTimer, &QTimer::timeout, this, [this] {
        if (m_pendingVolume < 0)
            return;
        QVariantMap q { { QStringLiteral("volume_percent"), m_pendingVolume } };
        if (!m_activeDeviceId.isEmpty())
            q.insert(QStringLiteral("device_id"), m_activeDeviceId);
        m_api->put(QStringLiteral("/me/player/volume"), q, {});
        QSettings().setValue(QStringLiteral("player/volume"), m_pendingVolume);
        m_pendingVolume = -1;
    });

    connect(m_librespot, &LibrespotProcess::playbackEvent, this, [this] { refreshSoon(250); });
    connect(m_librespot, &LibrespotProcess::stateChanged, this, [this] {
        if (m_librespot->state() == LibrespotProcess::Ready)
            findLocalDevice(8);
        else if (!m_localDeviceId.isEmpty() && m_librespot->state() != LibrespotProcess::NeedsLogin) {
            m_localDeviceId.clear();
            emit localDeviceChanged();
            emit stateChanged();
        }
    });
    connect(m_auth, &AuthManager::authenticatedChanged, this, [this] {
        if (m_auth->isAuthenticated()) {
            refresh();
            if (m_librespot->state() == LibrespotProcess::Ready)
                findLocalDevice(8);
        } else {
            m_pollTimer.stop();
            clearState();
        }
    });

    if (m_auth->isAuthenticated())
        QTimer::singleShot(0, this, &Player::refresh);
}

QString Player::artistsText() const
{
    QStringList names;
    for (const QVariant &a : m_artists)
        names << a.toMap().value(QStringLiteral("name")).toString();
    return names.join(QStringLiteral(", "));
}

int Player::positionMs() const
{
    if (!m_isPlaying || !m_positionClock.isValid())
        return m_positionBase;
    const qint64 pos = m_positionBase + m_positionClock.elapsed();
    return int(m_durationMs > 0 ? qMin<qint64>(pos, m_durationMs) : pos);
}

void Player::setPositionBase(int ms)
{
    m_positionBase = ms;
    m_positionClock.restart();
    emit positionChanged();
}

// ---------------------------------------------------------------- estado

void Player::refresh()
{
    if (!m_auth->isAuthenticated() || m_requestInFlight)
        return;
    m_requestInFlight = true;
    m_api->get(QStringLiteral("/me/player"), { { QStringLiteral("additional_types"), QStringLiteral("episode") } },
               [this](int status, const QJsonDocument &doc) {
        m_requestInFlight = false;
        if (status == 200 && doc.isObject())
            applyState(doc.object());
        else if (status == 204)
            clearState(); // nada tocando em nenhum dispositivo
        scheduleNextPoll();
    });
}

void Player::scheduleNextPoll()
{
    int interval = m_isPlaying ? 5000 : 15000;
    if (m_isPlaying && m_durationMs > 0) {
        const int remaining = m_durationMs - positionMs();
        if (remaining > 0 && remaining < interval)
            interval = remaining + 600; // pega a troca de faixa na hora
    }
    m_pollTimer.start(interval);
}

void Player::refreshSoon(int ms)
{
    if (!m_pollTimer.isActive() || m_pollTimer.remainingTime() > ms)
        m_pollTimer.start(ms);
}

void Player::applyState(const QJsonObject &obj)
{
    const QJsonObject item = obj.value(QStringLiteral("item")).toObject();
    const QString uri = item.value(QStringLiteral("uri")).toString();

    if (uri != m_trackUri) {
        m_trackUri = uri;
        m_trackName = item.value(QStringLiteral("name")).toString();
        m_durationMs = item.value(QStringLiteral("duration_ms")).toInt();
        m_artists.clear();

        QJsonArray images;
        if (item.contains(QStringLiteral("album"))) {
            const QJsonObject album = item.value(QStringLiteral("album")).toObject();
            m_albumName = album.value(QStringLiteral("name")).toString();
            m_albumUri = album.value(QStringLiteral("uri")).toString();
            images = album.value(QStringLiteral("images")).toArray();
            for (const QJsonValue &a : item.value(QStringLiteral("artists")).toArray()) {
                const QJsonObject artist = a.toObject();
                m_artists << QVariantMap {
                    { QStringLiteral("name"), artist.value(QStringLiteral("name")).toString() },
                    { QStringLiteral("id"), artist.value(QStringLiteral("id")).toString() },
                    { QStringLiteral("uri"), artist.value(QStringLiteral("uri")).toString() },
                };
            }
        } else {
            const QJsonObject show = item.value(QStringLiteral("show")).toObject();
            m_albumName = show.value(QStringLiteral("name")).toString();
            m_albumUri = show.value(QStringLiteral("uri")).toString();
            images = item.value(QStringLiteral("images")).toArray();
            m_artists << QVariantMap { { QStringLiteral("name"), m_albumName } };
        }
        m_coverUrl = pickImage(images, 64);
        m_coverUrlLarge = pickImage(images, 300);
        emit trackChanged();
        checkLiked();
        refreshQueue();
    }

    const QJsonObject device = obj.value(QStringLiteral("device")).toObject();
    const QJsonObject context = obj.value(QStringLiteral("context")).toObject();
    m_isPlaying = obj.value(QStringLiteral("is_playing")).toBool();
    m_shuffle = obj.value(QStringLiteral("shuffle_state")).toBool();
    m_repeatMode = obj.value(QStringLiteral("repeat_state")).toString(QStringLiteral("off"));
    m_contextUri = context.value(QStringLiteral("uri")).toString();
    m_activeDeviceId = device.value(QStringLiteral("id")).toString();
    m_activeDeviceName = device.value(QStringLiteral("name")).toString();
    emit stateChanged();

    if (device.contains(QStringLiteral("volume_percent")) && !device.value(QStringLiteral("volume_percent")).isNull()
        && m_pendingVolume < 0) {
        const int v = device.value(QStringLiteral("volume_percent")).toInt();
        if (v != m_volume) {
            m_volume = v;
            emit volumeChanged();
        }
    }

    setPositionBase(obj.value(QStringLiteral("progress_ms")).toInt());
    if (m_isPlaying)
        m_tickTimer.start();
    else
        m_tickTimer.stop();
}

void Player::clearState()
{
    const bool hadTrack = hasTrack();
    m_isPlaying = false;
    m_tickTimer.stop();
    m_activeDeviceId.clear();
    m_activeDeviceName.clear();
    emit stateChanged();
    if (hadTrack && !m_auth->isAuthenticated()) {
        m_trackUri.clear();
        m_trackName.clear();
        m_artists.clear();
        m_coverUrl.clear();
        m_coverUrlLarge.clear();
        m_durationMs = 0;
        emit trackChanged();
    }
}

void Player::checkLiked()
{
    if (!m_trackUri.startsWith(QLatin1String("spotify:track:"))) {
        if (m_liked) {
            m_liked = false;
            emit likedChanged();
        }
        return;
    }
    const QString uri = m_trackUri;
    m_api->get(QStringLiteral("/me/library/contains"), { { QStringLiteral("uris"), uri } },
               [this, uri](int status, const QJsonDocument &doc) {
        if (status != 200 || uri != m_trackUri)
            return;
        const bool liked = doc.isArray() && doc.array().at(0).toBool();
        if (liked != m_liked) {
            m_liked = liked;
            emit likedChanged();
        }
    });
}

void Player::refreshQueue()
{
    m_api->get(QStringLiteral("/me/player/queue"), {}, [this](int status, const QJsonDocument &doc) {
        if (status != 200)
            return;
        m_queue.clear();
        for (const QJsonValue &v : doc.object().value(QStringLiteral("queue")).toArray())
            m_queue << simplifyItem(v.toObject());
        emit queueChanged();
    });
}

void Player::refreshDevices()
{
    m_api->get(QStringLiteral("/me/player/devices"), {}, [this](int status, const QJsonDocument &doc) {
        if (status != 200)
            return;
        m_devices.clear();
        for (const QJsonValue &v : doc.object().value(QStringLiteral("devices")).toArray()) {
            const QJsonObject d = v.toObject();
            m_devices << QVariantMap {
                { QStringLiteral("id"), d.value(QStringLiteral("id")).toString() },
                { QStringLiteral("name"), d.value(QStringLiteral("name")).toString() },
                { QStringLiteral("type"), d.value(QStringLiteral("type")).toString() },
                { QStringLiteral("active"), d.value(QStringLiteral("is_active")).toBool() },
                { QStringLiteral("local"), d.value(QStringLiteral("name")).toString() == m_librespot->deviceName() },
            };
        }
        emit devicesChanged();
    });
}

void Player::findLocalDevice(int attemptsLeft)
{
    if (!m_auth->isAuthenticated() || attemptsLeft <= 0)
        return;
    // O dispositivo do librespot demora alguns segundos para aparecer na API.
    m_api->get(QStringLiteral("/me/player/devices"), {}, [this, attemptsLeft](int status, const QJsonDocument &doc) {
        if (status == 200) {
            for (const QJsonValue &v : doc.object().value(QStringLiteral("devices")).toArray()) {
                const QJsonObject d = v.toObject();
                if (d.value(QStringLiteral("name")).toString() == m_librespot->deviceName()) {
                    const QString id = d.value(QStringLiteral("id")).toString();
                    if (id != m_localDeviceId) {
                        m_localDeviceId = id;
                        emit localDeviceChanged();
                        emit stateChanged();
                    }
                    return;
                }
            }
        }
        QTimer::singleShot(1500, this, [this, attemptsLeft] { findLocalDevice(attemptsLeft - 1); });
    });
}

// ---------------------------------------------------------------- comandos

QString Player::targetDeviceForPlay() const
{
    // Igual ao app oficial: se algo já está ativo (celular, outro PC), toca lá.
    // Se nada está ativo, toca aqui no librespot.
    if (!m_activeDeviceId.isEmpty())
        return m_activeDeviceId;
    return m_localDeviceId;
}

void Player::sendPlay(const QJsonObject &body, const QString &deviceId, bool retried)
{
    QVariantMap q;
    if (!deviceId.isEmpty())
        q.insert(QStringLiteral("device_id"), deviceId);
    m_api->put(QStringLiteral("/me/player/play"), q, body.isEmpty() ? QJsonDocument() : QJsonDocument(body),
               [this, body, retried](int status, const QJsonDocument &) {
        if (status == 404 && !retried && !m_localDeviceId.isEmpty()) {
            // NO_ACTIVE_DEVICE: o dispositivo ativo sumiu, toca aqui.
            sendPlay(body, m_localDeviceId, true);
            return;
        }
        if (status == 404)
            emit message(tr("Nenhum dispositivo disponível. O motor de áudio está rodando?"));
        else if (status == 403)
            emit message(tr("O Spotify recusou o comando (precisa de Premium)."));
        refreshSoon(500);
    });
}

void Player::togglePlay()
{
    if (m_isPlaying) {
        m_isPlaying = false; // otimista
        m_tickTimer.stop();
        setPositionBase(positionMs());
        emit stateChanged();
        m_api->put(QStringLiteral("/me/player/pause"), {}, {}, [this](int, const QJsonDocument &) { refreshSoon(500); });
        return;
    }

    if (m_activeDeviceId.isEmpty() && !m_localDeviceId.isEmpty()) {
        // Nada ativo: transfere a sessão para cá e retoma de onde parou.
        transferTo(m_localDeviceId);
        return;
    }
    m_isPlaying = true;
    m_positionClock.restart();
    m_tickTimer.start();
    emit stateChanged();
    sendPlay({}, targetDeviceForPlay());
}

void Player::next()
{
    m_api->post(QStringLiteral("/me/player/next"), {}, {}, [this](int, const QJsonDocument &) { refreshSoon(400); });
}

void Player::previous()
{
    // Igual ao app oficial: depois de 3 s volta ao início da faixa.
    if (positionMs() > 3000) {
        seek(0);
        return;
    }
    m_api->post(QStringLiteral("/me/player/previous"), {}, {}, [this](int, const QJsonDocument &) { refreshSoon(400); });
}

void Player::seek(int ms)
{
    ms = qBound(0, ms, qMax(0, m_durationMs));
    setPositionBase(ms);
    m_api->put(QStringLiteral("/me/player/seek"), { { QStringLiteral("position_ms"), ms } }, {},
               [this](int, const QJsonDocument &) { refreshSoon(800); });
}

void Player::setVolume(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent == m_volume)
        return;
    m_volume = percent;
    m_pendingVolume = percent;
    emit volumeChanged();
    m_volumeTimer.start();
}

void Player::toggleShuffle()
{
    m_shuffle = !m_shuffle;
    emit stateChanged();
    m_api->put(QStringLiteral("/me/player/shuffle"), { { QStringLiteral("state"), m_shuffle } }, {},
               [this](int, const QJsonDocument &) { refreshSoon(600); refreshQueue(); });
}

void Player::cycleRepeat()
{
    if (m_repeatMode == QLatin1String("off"))
        m_repeatMode = QStringLiteral("context");
    else if (m_repeatMode == QLatin1String("context"))
        m_repeatMode = QStringLiteral("track");
    else
        m_repeatMode = QStringLiteral("off");
    emit stateChanged();
    m_api->put(QStringLiteral("/me/player/repeat"), { { QStringLiteral("state"), m_repeatMode } }, {},
               [this](int, const QJsonDocument &) { refreshSoon(600); });
}

void Player::toggleLike()
{
    if (!m_trackUri.startsWith(QLatin1String("spotify:track:")))
        return;
    const bool newState = !m_liked;
    m_liked = newState;
    emit likedChanged();
    const QVariantMap q { { QStringLiteral("uris"), m_trackUri } };
    auto done = [this, newState](int status, const QJsonDocument &) {
        if (status >= 300) { // desfaz o otimismo
            m_liked = !newState;
            emit likedChanged();
            emit message(tr("Não consegui atualizar suas Músicas Curtidas."));
        } else {
            emit message(newState ? tr("Adicionada às Músicas Curtidas") : tr("Removida das Músicas Curtidas"));
        }
    };
    if (newState)
        m_api->put(QStringLiteral("/me/library"), q, {}, done);
    else
        m_api->del(QStringLiteral("/me/library"), q, {}, done);
}

void Player::playContext(const QString &contextUri, const QString &offsetUri, int offsetIndex)
{
    QJsonObject body { { QStringLiteral("context_uri"), contextUri } };
    if (!offsetUri.isEmpty())
        body.insert(QStringLiteral("offset"), QJsonObject { { QStringLiteral("uri"), offsetUri } });
    else if (offsetIndex >= 0)
        body.insert(QStringLiteral("offset"), QJsonObject { { QStringLiteral("position"), offsetIndex } });
    sendPlay(body, targetDeviceForPlay());
}

void Player::playUris(const QStringList &uris, int offsetIndex)
{
    if (uris.isEmpty())
        return;
    // A API aceita no máximo algumas centenas de URIs; mandamos uma janela em volta da faixa escolhida.
    const int start = qMax(0, offsetIndex - 50);
    const QStringList window = uris.mid(start, 300);
    QJsonObject body {
        { QStringLiteral("uris"), QJsonArray::fromStringList(window) },
        { QStringLiteral("offset"), QJsonObject { { QStringLiteral("position"), offsetIndex - start } } },
    };
    sendPlay(body, targetDeviceForPlay());
}

void Player::addToQueue(const QString &uri)
{
    m_api->post(QStringLiteral("/me/player/queue"), { { QStringLiteral("uri"), uri } }, {},
                [this](int status, const QJsonDocument &) {
        emit message(status < 300 ? tr("Adicionada à fila") : tr("Não consegui adicionar à fila"));
        refreshQueue();
    });
}

void Player::transferTo(const QString &deviceId)
{
    if (deviceId.isEmpty())
        return;
    const QJsonObject body {
        { QStringLiteral("device_ids"), QJsonArray { deviceId } },
        { QStringLiteral("play"), true },
    };
    m_api->put(QStringLiteral("/me/player"), {}, QJsonDocument(body), [this](int status, const QJsonDocument &) {
        if (status >= 300)
            emit message(tr("Não consegui trocar de dispositivo."));
        refreshSoon(700);
        QTimer::singleShot(1500, this, &Player::refreshDevices);
    });
}
