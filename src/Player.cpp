#include "Player.h"

#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "SpotifyApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>

#include <climits>
#include <utility>

namespace {

// Escolhe a menor imagem com largura >= minWidth (as imagens vêm da maior para
// a menor).
QString pickImage(const QJsonArray &images, int minWidth) {
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

QString joinArtists(const QJsonArray &artists) {
  QStringList names;
  for (const QJsonValue &a : artists)
    names << a.toObject().value(QStringLiteral("name")).toString();
  return names.join(QStringLiteral(", "));
}

QVariantMap simplifyItem(const QJsonObject &item) {
  QVariantMap m;
  m.insert(QStringLiteral("name"),
           item.value(QStringLiteral("name")).toString());
  m.insert(QStringLiteral("uri"), item.value(QStringLiteral("uri")).toString());
  m.insert(QStringLiteral("durationMs"),
           item.value(QStringLiteral("duration_ms")).toInt());
  if (item.contains(QStringLiteral("album"))) {
    const QJsonObject album = item.value(QStringLiteral("album")).toObject();
    const auto artists = item.value(QStringLiteral("artists")).toArray();
    m.insert(QStringLiteral("artistId"), artists.isEmpty() ? QString() : artists.first().toObject().value(QStringLiteral("id")).toString());
    m.insert(QStringLiteral("albumId"), album.value(QStringLiteral("id")).toString());
    m.insert(QStringLiteral("album"), album.value(QStringLiteral("name")).toString());
    m.insert(QStringLiteral("year"), album.value(QStringLiteral("release_date")).toString().left(4).toInt());
    m.insert(QStringLiteral("artists"),
             joinArtists(item.value(QStringLiteral("artists")).toArray()));
    m.insert(QStringLiteral("coverUrl"),
             pickImage(album.value(QStringLiteral("images")).toArray(), 64));
  } else { // episódio de podcast
    const QJsonObject show = item.value(QStringLiteral("show")).toObject();
    m.insert(QStringLiteral("artists"),
             show.value(QStringLiteral("name")).toString());
    m.insert(QStringLiteral("coverUrl"),
             pickImage(item.value(QStringLiteral("images")).toArray(), 64));
  }
  return m;
}

} // namespace

Player::Player(SpotifyApi *api, LibrespotProcess *librespot, AuthManager *auth,
               QObject *parent)
    : QObject(parent), m_api(api), m_librespot(librespot), m_auth(auth) {
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
    QVariantMap q{{QStringLiteral("volume_percent"), m_pendingVolume}};
    if (!m_activeDeviceId.isEmpty())
      q.insert(QStringLiteral("device_id"), m_activeDeviceId);
    m_api->put(QStringLiteral("/me/player/volume"), q, {});
    QSettings().setValue(QStringLiteral("player/volume"), m_pendingVolume);
    m_pendingVolume = -1;
  });

  m_seekTimer.setSingleShot(true);
  m_seekTimer.setInterval(60);
  connect(&m_seekTimer, &QTimer::timeout, this, [this] {
    if (m_seekTarget < 0 || localPlayback())
      return;
    QElapsedTimer sent;
    sent.start();
    m_api->put(QStringLiteral("/me/player/seek"),
               {{QStringLiteral("position_ms"), m_seekTarget}}, {},
               [this, sent](int status, const QJsonDocument &) {
                 if (status >= 200 && status < 300)
                   m_commandLatencyMs = qBound(80, int(m_commandLatencyMs * 0.7 + sent.elapsed() * 0.3), 1500);
                 refreshSoon(900);
               });
  });

  connect(m_librespot, &LibrespotProcess::playbackEvent, this,
          [this] { refreshSoon(250); });
  connect(m_librespot, &LibrespotProcess::sessionLost, this,
          &Player::rememberForResume);
  connect(m_librespot, &LibrespotProcess::stateChanged, this, [this] {
    if (m_librespot->state() == LibrespotProcess::Ready)
      findLocalDevice(8);
    else if (!m_localDeviceId.isEmpty() &&
             m_librespot->state() != LibrespotProcess::NeedsLogin) {
      if (m_activeDeviceId == m_localDeviceId) {
        m_activeDeviceId.clear();
        m_activeDeviceName.clear();
      }
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
      cancelResume();
      m_pollTimer.stop();
      clearState();
    }
  });

  if (m_auth->isAuthenticated())
    QTimer::singleShot(0, this, &Player::refresh);
}

void Player::setAudioEngine(tonal::audio::AudioEngine *engine) {
  m_audio = engine;
  connect(engine, &tonal::audio::AudioEngine::localChanged, this, [this] {
    if (localPlayback()) {
      cancelResume();
      m_librespot->setPlaybackExpected(false);
      m_tickTimer.stop();
      m_volumeTimer.stop();
      m_pendingVolume = -1;
      m_audio->setLocalVolume(m_volume);
    }
    emit trackChanged();
    emit stateChanged();
    emit positionChanged();
    emit likedChanged();
    if (!localPlayback())
      refreshSoon(300);
  });
  connect(engine, &tonal::audio::AudioEngine::positionChanged, this,
          &Player::positionChanged);
  connect(engine, &tonal::audio::AudioEngine::backendFailed, this,
          &Player::message);
}

QString Player::artistsText() const {
  if (localPlayback())
    return tr("Arquivo local");
  QStringList names;
  for (const QVariant &a : m_artists)
    names << a.toMap().value(QStringLiteral("name")).toString();
  return names.join(QStringLiteral(", "));
}

int Player::positionMs() const {
  if (localPlayback())
    return m_audio->positionMs();
  if (!m_isPlaying || !m_positionClock.isValid())
    return m_positionBase;
  const qint64 pos = m_positionBase + m_positionClock.elapsed();
  return int(m_durationMs > 0 ? qMin<qint64>(pos, m_durationMs) : pos);
}

void Player::setPositionBase(int ms) {
  m_positionBase = ms;
  m_positionClock.restart();
  emit positionChanged();
}

// ---------------------------------------------------------------- estado

void Player::refresh() {
  if (localPlayback())
    return;
  if (!m_auth->isAuthenticated() || m_requestInFlight)
    return;
  m_requestInFlight = true;
  const auto transportGeneration = m_transportGeneration;
  const auto seekGeneration = m_seekGeneration;
  m_api->get(QStringLiteral("/me/player"),
             {{QStringLiteral("additional_types"), QStringLiteral("episode")}},
             [this, transportGeneration, seekGeneration](int status, const QJsonDocument &doc) {
               m_requestInFlight = false;
               if (transportGeneration != m_transportGeneration ||
                   seekGeneration != m_seekGeneration) {
                 refreshSoon(250);
                 return;
               }
               if (status == 200 && doc.isObject())
                 applyState(doc.object());
               else if (status == 204)
                 clearState(); // nada tocando em nenhum dispositivo
               scheduleNextPoll();
             });
}

void Player::scheduleNextPoll() {
  int interval = m_isPlaying ? 5000 : 15000;
  if (m_isPlaying && m_durationMs > 0) {
    const int remaining = m_durationMs - positionMs();
    if (remaining > 0 && remaining < interval)
      interval = remaining + 600; // pega a troca de faixa na hora
  }
  m_pollTimer.start(interval);
}

void Player::refreshSoon(int ms) {
  if (!m_pollTimer.isActive() || m_pollTimer.remainingTime() > ms)
    m_pollTimer.start(ms);
}

void Player::applyState(const QJsonObject &obj) {
  if (localPlayback())
    return;
  const QJsonObject item = obj.value(QStringLiteral("item")).toObject();
  const QString uri = item.value(QStringLiteral("uri")).toString();

  if (uri != m_trackUri) {
    cancelSeek();
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
      for (const QJsonValue &a :
           item.value(QStringLiteral("artists")).toArray()) {
        const QJsonObject artist = a.toObject();
        m_artists << QVariantMap{
            {QStringLiteral("name"),
             artist.value(QStringLiteral("name")).toString()},
            {QStringLiteral("id"),
             artist.value(QStringLiteral("id")).toString()},
            {QStringLiteral("uri"),
             artist.value(QStringLiteral("uri")).toString()},
        };
      }
    } else {
      const QJsonObject show = item.value(QStringLiteral("show")).toObject();
      m_albumName = show.value(QStringLiteral("name")).toString();
      m_albumUri = show.value(QStringLiteral("uri")).toString();
      images = item.value(QStringLiteral("images")).toArray();
      m_artists << QVariantMap{{QStringLiteral("name"), m_albumName}};
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
  // Repetição: logo depois de uma troca do usuário ou de uma nova seleção (que
  // o librespot zera até reapplyRepeat chegar), um GET antigo não manda.
  const QString remoteRepeat =
      obj.value(QStringLiteral("repeat_state")).toString(QStringLiteral("off"));
  const bool repeatSettling = m_repeatClock.isValid() && m_repeatClock.elapsed() < 3000;
  const bool selectionLoading = m_selectionClock.isValid() && m_selectionClock.elapsed() < 4000;
  if (!repeatSettling && !(selectionLoading && m_repeatWanted != QLatin1String("off")))
    m_repeatMode = m_repeatWanted = remoteRepeat;
  m_contextUri = context.value(QStringLiteral("uri")).toString();
  m_activeDeviceId = device.value(QStringLiteral("id")).toString();
  m_activeDeviceName = device.value(QStringLiteral("name")).toString();
  m_librespot->setPlaybackExpected(m_isPlaying && playingHere());
  // Connect controls can arrive without a matching librespot debug command.
  // Reconcile the physical sink with confirmed session state as a fallback.
  // Give recent local controls time to settle; a stale GET must not undo pause.
  if (m_audio && (!m_transportIntentClock.isValid() ||
                  m_transportIntentClock.elapsed() >= 2000))
    m_audio->setSpotifyPaused(!m_isPlaying || !playingHere());
  emit stateChanged();

  if (device.contains(QStringLiteral("volume_percent")) &&
      !device.value(QStringLiteral("volume_percent")).isNull() &&
      m_pendingVolume < 0) {
    const int v = device.value(QStringLiteral("volume_percent")).toInt();
    if (v != m_volume) {
      m_volume = v;
      emit volumeChanged();
    }
  }

  const int remote = obj.value(QStringLiteral("progress_ms")).toInt();
  if (!seekSettling(remote))
    setPositionBase(remote);
  if (m_isPlaying)
    m_tickTimer.start();
  else
    m_tickTimer.stop();
}

void Player::clearState() {
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

void Player::checkLiked() {
  if (!m_trackUri.startsWith(QLatin1String("spotify:track:"))) {
    if (m_liked) {
      m_liked = false;
      emit likedChanged();
    }
    return;
  }
  const QString uri = m_trackUri;
  m_api->get(QStringLiteral("/me/library/contains"),
             {{QStringLiteral("uris"), uri}},
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

void Player::refreshQueue() {
  if (queueFromOrderedRows())
    return;
  const auto generation = m_transportGeneration;
  const auto current = m_trackUri;
  m_api->get(QStringLiteral("/me/player/queue"), {},
             [this, generation, current](int status, const QJsonDocument &doc) {
                 if (generation != m_transportGeneration || current != m_trackUri) {
                   emit queueRefreshFinished(409);
                   return;
                 }
                 if (status != 200) {
                   emit queueRefreshFinished(status);
                   return;
                 }
               m_queue.clear();
               for (const QJsonValue &v :
                    doc.object().value(QStringLiteral("queue")).toArray())
                 m_queue << simplifyItem(v.toObject());
               // Connect devices may return no queue even for an album/playlist.
               // Resolve that exact context, never invent a second source song.
               if (m_queue.isEmpty() && (m_contextUri.startsWith("spotify:album:") ||
                                         m_contextUri.startsWith("spotify:playlist:"))) {
                 const auto id = m_contextUri.section(':', -1);
                 const auto path = m_contextUri.startsWith("spotify:album:")
                     ? "/albums/" + id : "/playlists/" + id + "/items?limit=50";
                 loadContextQueue(path, m_contextUri, current, generation);
                 return;
               }
               emit queueChanged();
               emit queueRefreshFinished(status);
             });
}

void Player::loadContextQueue(const QString &path, const QString &context,
                              const QString &track, uint64_t generation,
                              QVariantList rows, QJsonObject album, int page) {
  m_api->get(path, {}, [this, context, track, generation, rows, album, page]
             (int status, const QJsonDocument &doc) mutable {
    if (generation != m_transportGeneration || track != m_trackUri || context != m_contextUri) {
      emit queueRefreshFinished(409);
      return;
    }
    if (status != 200) { emit queueRefreshFinished(status); return; }
    auto data = doc.object();
    if (data.contains("tracks") && context.startsWith("spotify:album:")) {
      album = data;
      data = data["tracks"].toObject();
      album.remove("tracks");
    }
    for (const auto &value : data["items"].toArray()) {
      auto item = value.toObject();
      if (item.contains("item")) item = item["item"].toObject();
      else if (item.contains("track")) item = item["track"].toObject();
      if (!item["uri"].toString().startsWith("spotify:track:") || item["is_local"].toBool()) continue;
      if (!item.contains("album") && !album.isEmpty()) item["album"] = album;
      if (rows.size() < 500) rows << simplifyItem(item);
    }
    int at = -1;
    for (int i = 0; i < rows.size(); ++i)
      if (rows[i].toMap()["uri"].toString() == track) { at = i; break; }
    const auto next = data["next"].toString();
    if (!next.isEmpty() && rows.size() < 500 && page < 9 && (at < 0 || rows.size() - at < 20)) {
      loadContextQueue(next, context, track, generation, rows, album, page + 1);
      return;
    }
    m_queue = at < 0 ? QVariantList{} : rows.mid(at + 1);
    emit queueChanged();
    emit queueRefreshFinished(200);
  });
}

void Player::refreshDevices() {
  m_api->get(
      QStringLiteral("/me/player/devices"), {},
      [this](int status, const QJsonDocument &doc) {
        if (status != 200)
          return;
        m_devices.clear();
        for (const QJsonValue &v :
             doc.object().value(QStringLiteral("devices")).toArray()) {
          const QJsonObject d = v.toObject();
          m_devices << QVariantMap{
              {QStringLiteral("id"), d.value(QStringLiteral("id")).toString()},
              {QStringLiteral("name"),
               d.value(QStringLiteral("name")).toString()},
              {QStringLiteral("type"),
               d.value(QStringLiteral("type")).toString()},
              {QStringLiteral("active"),
               d.value(QStringLiteral("is_active")).toBool()},
              {QStringLiteral("local"),
               d.value(QStringLiteral("name")).toString() ==
                   m_librespot->deviceName()},
          };
        }
        emit devicesChanged();
      });
}

void Player::findLocalDevice(int attemptsLeft) {
  if (!m_auth->isAuthenticated() || attemptsLeft <= 0)
    return;
  // O dispositivo do librespot demora alguns segundos para aparecer na API.
  m_api->get(QStringLiteral("/me/player/devices"), {},
             [this, attemptsLeft](int status, const QJsonDocument &doc) {
               if (status == 200) {
                 for (const QJsonValue &v :
                      doc.object().value(QStringLiteral("devices")).toArray()) {
                   const QJsonObject d = v.toObject();
                   if (d.value(QStringLiteral("name")).toString() ==
                       m_librespot->deviceName()) {
                     const QString id =
                         d.value(QStringLiteral("id")).toString();
                     if (id != m_localDeviceId) {
                       m_localDeviceId = id;
                       emit localDeviceChanged();
                       emit stateChanged();
                     }
                     if (!m_resumeBody.isEmpty())
                       resumeAfterEngineRestart();
                     return;
                   }
                 }
               }
               QTimer::singleShot(1500, this, [this, attemptsLeft] {
                 findLocalDevice(attemptsLeft - 1);
               });
             });
}

// ---------------------------------------------------------------- queda do
// motor

// Quando a sessão do librespot cai, o Spotify perde a reprodução deste
// dispositivo e o motor novo sobe parado. Guardamos onde estava para retomar
// assim que o dispositivo reaparecer.
void Player::rememberForResume() {
  if (!m_resumeBody.isEmpty())
    return;
  if (!m_isPlaying || !playingHere() || m_trackUri.isEmpty())
    return;
  QJsonObject body{{QStringLiteral("position_ms"), positionMs()}};
  const QJsonObject offset{{QStringLiteral("uri"), m_trackUri}};
  if (!m_contextUri.isEmpty()) {
    body.insert(QStringLiteral("context_uri"), m_contextUri);
    body.insert(QStringLiteral("offset"), offset);
  } else if (m_lastUris.contains(m_trackUri)) {
    // Lista solta (busca, populares): manda a lista toda para manter o que vem
    // depois. Com "uris" usamos offset por posição, o mesmo formato do
    // playUris.
    body.insert(QStringLiteral("uris"), QJsonArray::fromStringList(m_lastUris));
    body.insert(QStringLiteral("offset"),
                QJsonObject{{QStringLiteral("position"),
                             int(m_lastUris.indexOf(m_trackUri))}});
  } else {
    body.insert(QStringLiteral("uris"), QJsonArray{m_trackUri});
  }
  m_resumeBody = body;
  ++m_resumeGeneration;
  m_resumeInFlight = false;
  m_resumeAttempts = 0;
  m_resumeShuffle = m_shuffle;
  m_resumeRepeat = m_repeatMode;
}

void Player::resumeAfterEngineRestart() {
  if (m_resumeBody.isEmpty() || m_resumeInFlight || m_localDeviceId.isEmpty())
    return;
  const QJsonObject body = m_resumeBody;
  const auto generation = m_resumeGeneration;
  m_resumeInFlight = true;
  ++m_resumeAttempts;
  const bool shuffle = m_resumeShuffle;
  const QString repeat = m_resumeRepeat;
  m_api->put(
      QStringLiteral("/me/player/play"),
      {{QStringLiteral("device_id"), m_localDeviceId}}, QJsonDocument(body),
      [this, shuffle, repeat, generation](int status, const QJsonDocument &) {
        if (generation != m_resumeGeneration)
          return;
        m_resumeInFlight = false;
        if (status >= 200 && status < 300) {
          m_resumeBody = {};
          m_resumeAttempts = 0;
          // O motor novo começa com aleatório e repetição desligados.
          if (shuffle)
            m_api->put(QStringLiteral("/me/player/shuffle"),
                       {{QStringLiteral("state"), true}}, {});
          if (repeat != QLatin1String("off"))
            m_api->put(QStringLiteral("/me/player/repeat"),
                       {{QStringLiteral("state"), repeat}}, {});
        } else if ((status == 0 || status == 404 || status >= 500) &&
                   m_resumeAttempts < 3) {
          QTimer::singleShot(1500 * m_resumeAttempts, this, [this, generation] {
            if (generation == m_resumeGeneration && !m_resumeBody.isEmpty() &&
                m_librespot->state() == LibrespotProcess::Ready)
              findLocalDevice(3);
          });
        } else
          emit message(tr("Não foi possível retomar a faixa. Sua posição foi "
                          "preservada; tente Reproduzir novamente."));
        refreshSoon(500);
      },
      [this, generation] {
        return generation == m_resumeGeneration && !localPlayback() &&
               m_auth->isAuthenticated();
      });
}

void Player::cancelResume() {
  ++m_resumeGeneration;
  m_resumeBody = {};
  m_resumeInFlight = false;
  m_resumeAttempts = 0;
}

// ---------------------------------------------------------------- comandos

QString Player::targetDeviceForPlay() const {
  // Igual ao app oficial: se algo já está ativo (celular, outro PC), toca lá.
  // Se nada está ativo, toca aqui no librespot.
  if (!m_activeDeviceId.isEmpty())
    return m_activeDeviceId;
  return m_localDeviceId;
}

void Player::sendPlay(const QJsonObject &body, const QString &deviceId,
                      bool retried, bool ordered) {
  if (deviceId.isEmpty() &&
      (m_librespot->state() == LibrespotProcess::Starting ||
       m_librespot->state() == LibrespotProcess::Crashed)) {
    if (!body.isEmpty()) {
      cancelResume();
      m_resumeBody = body;
      m_resumeShuffle = m_shuffle;
      m_resumeRepeat = m_repeatMode;
    }
    if (m_librespot->state() == LibrespotProcess::Crashed)
      m_librespot->restart();
    emit message(tr("Reconectando o áudio; a faixa escolhida tocará quando o "
                    "motor estiver pronto."));
    return;
  }
  QVariantMap q;
  if (!deviceId.isEmpty())
    q.insert(QStringLiteral("device_id"), deviceId);
  if (!body.isEmpty()) {
    cancelSeek();
    m_selectionClock.restart();
    if (!ordered) {
      // Seleção comum (álbum, playlist, busca): a fila do mix deixa de valer.
      m_orderedRows.clear();
      emit selectionReplaced();
    }
  }
  const auto intent=++m_transportGeneration;
  m_transportIntentClock.restart();
  m_api->put(
      QStringLiteral("/me/player/play"), q,
      body.isEmpty() ? QJsonDocument() : QJsonDocument(body),
      [this, body, retried, ordered, intent, deviceId](int status, const QJsonDocument &) {
        if(ordered&&intent!=m_transportGeneration)return;
        if(!body.isEmpty()&&status>=200&&status<300){
          reapplyRepeat(deviceId);
          // A faixa pode continuar a mesma ao reaplicar um mix: sem troca de
          // faixa, a fila mostrada também precisa ser atualizada.
          if(ordered)QTimer::singleShot(0,this,&Player::refreshQueue);
        }
        if(ordered&&status>=200&&status<300){QVariantMap query{{"state",false}};if(!deviceId.isEmpty())query["device_id"]=deviceId;m_api->put("/me/player/shuffle",query,{},[this,intent](int s,const QJsonDocument&){if(intent!=m_transportGeneration)return;if(s>=200&&s<300){m_shuffle=false;emit stateChanged();}else emit message(tr("Não foi possível confirmar a ordem do mix no dispositivo."));});}
        if (status == 404 && !retried && !m_localDeviceId.isEmpty()) {
          // NO_ACTIVE_DEVICE: o dispositivo ativo sumiu, toca aqui.
          sendPlay(body, m_localDeviceId, true, ordered);
          return;
        }
        if (status == 404)
          emit message(tr(
              "Nenhum dispositivo disponível. O motor de áudio está rodando?"));
        else if (status == 403)
          emit message(tr("O Spotify recusou o comando (precisa de Premium)."));
        refreshSoon(500);
      });
}

void Player::togglePlay() {
  emit playbackRequested();
  if (localPlayback()) {
    m_audio->toggleLocalPlay();
    return;
  }
  ++m_transportGeneration;
  m_transportIntentClock.restart();
  if (m_audio)
    m_audio->setSpotifyPaused(m_isPlaying);
  if (m_isPlaying) {
    pause();
    return;
  }

  if (!m_resumeBody.isEmpty() && !m_localDeviceId.isEmpty()) {
    // O motor caiu tocando e já voltou: o Spotify perdeu o estado, então
    // tocamos o que foi guardado.
    m_resumeAttempts = 0;
    resumeAfterEngineRestart();
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

void Player::pause() {
  emit playbackRequested();
  if (localPlayback()) {
    if (m_audio->localPlaying())
      m_audio->toggleLocalPlay();
    return;
  }
  cancelResume();
  ++m_transportGeneration;
  m_transportIntentClock.restart();
  m_librespot->setPlaybackExpected(false);
  if (m_audio)
    m_audio->setSpotifyPaused(true);
  const int position = positionMs();
  const bool wasPlaying = m_isPlaying;
  m_isPlaying = false;
  m_tickTimer.stop();
  setPositionBase(position);
  emit stateChanged();
  if (wasPlaying)
    m_api->put(QStringLiteral("/me/player/pause"), {}, {},
               [this](int, const QJsonDocument &) { refreshSoon(500); });
}

void Player::next() {
  emit playbackRequested();
  if (localPlayback()) {
    emit message(tr("Abra outro arquivo pelo painel de áudio."));
    return;
  }
  cancelResume();
  cancelSeek();
  skipLeavesTrackRepeat();
  if (m_audio)
    m_audio->flushSpotify();
  m_api->post(QStringLiteral("/me/player/next"), {}, {},
              [this](int, const QJsonDocument &) { refreshSoon(400); });
}

void Player::previous() {
  emit playbackRequested();
  if (localPlayback()) {
    m_audio->seekLocal(0);
    return;
  }
  cancelResume();
  // Igual ao app oficial: depois de 3 s volta ao início da faixa.
  if (positionMs() > 3000) {
    seek(0);
    return;
  }
  cancelSeek();
  skipLeavesTrackRepeat();
  m_api->post(QStringLiteral("/me/player/previous"), {}, {},
              [this](int, const QJsonDocument &) { refreshSoon(400); });
}

void Player::seek(int ms) {
  if (localPlayback()) {
    m_audio->seekLocal(ms);
    return;
  }
  // Sem flush aqui: o áudio atual continua até o librespot aplicar o seek, e
  // o próprio "command=Seek(" descarta o buffer antigo nesse instante. Limpar
  // antes só abria um silêncio e depois tocava de novo o trecho antigo.
  ms = qBound(0, ms, qMax(0, m_durationMs));
  ++m_seekGeneration;
  m_seekTarget = ms;
  m_seekClock.restart();
  setPositionBase(ms);
  m_seekTimer.start();
}

// Outra faixa ou seleção: um seek pendente ou recente não vale para ela.
void Player::cancelSeek() {
  m_seekTimer.stop();
  m_seekTarget = -1;
}

// Logo depois de um seek, o Spotify ainda pode responder a posição antiga.
// Nesses 3 s, só aceita a posição remota se ela já estiver perto da pedida.
bool Player::seekSettling(int remoteMs) const {
  if (m_seekTarget < 0 || !m_seekClock.isValid() || m_seekClock.elapsed() >= 3000)
    return false;
  const qint64 expected = m_seekTarget + (m_isPlaying ? m_seekClock.elapsed() : 0);
  return qAbs(remoteMs - expected) > 1500;
}

void Player::setVolume(int percent) {
  percent = qBound(0, percent, 100);
  if (percent == m_volume)
    return;
  m_volume = percent;
  if (localPlayback()) {
    m_audio->setLocalVolume(percent);
    QSettings().setValue(QStringLiteral("player/volume"), percent);
    emit volumeChanged();
    return;
  }
  m_pendingVolume = percent;
  emit volumeChanged();
  m_volumeTimer.start();
}

void Player::toggleShuffle() {
  if (localPlayback())
    return;
  m_shuffle = !m_shuffle;
  emit stateChanged();
  m_api->put(QStringLiteral("/me/player/shuffle"),
             {{QStringLiteral("state"), m_shuffle}}, {},
             [this](int, const QJsonDocument &) {
               refreshSoon(600);
               refreshQueue();
             });
}

void Player::cycleRepeat() {
  if (localPlayback()) {
    emit message(tr("A repetição vale para músicas do Spotify."));
    return;
  }
  const QString previous = m_repeatMode;
  if (m_repeatMode == QLatin1String("off"))
    m_repeatMode = QStringLiteral("context");
  else if (m_repeatMode == QLatin1String("context"))
    m_repeatMode = QStringLiteral("track");
  else
    m_repeatMode = QStringLiteral("off");
  m_repeatWanted = m_repeatMode;
  m_repeatClock.restart();
  emit stateChanged();
  QVariantMap query{{QStringLiteral("state"), m_repeatMode}};
  if (!m_activeDeviceId.isEmpty())
    query.insert(QStringLiteral("device_id"), m_activeDeviceId);
  const QString wanted = m_repeatMode;
  m_api->put(QStringLiteral("/me/player/repeat"), query, {},
             [this, previous, wanted](int status, const QJsonDocument &) {
               if ((status < 200 || status >= 300) && m_repeatWanted == wanted) {
                 // Antes, a falha era silenciosa e o próximo GET só desfazia o ícone.
                 m_repeatMode = m_repeatWanted = previous;
                 emit stateChanged();
                 emit message(status == 403
                     ? tr("O Spotify não permite mudar a repetição nesta faixa (reprodução automática).")
                     : tr("Não foi possível mudar a repetição (HTTP %1).").arg(status));
               }
               refreshSoon(800);
             });
}

// O librespot zera repetição e aleatório a cada nova seleção (tocar álbum,
// Shake, Descoberta 2+1). Reaplica a escolha do usuário logo depois da carga.
void Player::reapplyRepeat(const QString &deviceId) {
  if (m_repeatWanted == QLatin1String("off"))
    return;
  QVariantMap query{{QStringLiteral("state"), m_repeatWanted}};
  if (!deviceId.isEmpty())
    query.insert(QStringLiteral("device_id"), deviceId);
  m_repeatClock.restart();
  m_api->put(QStringLiteral("/me/player/repeat"), query, {},
             [this](int, const QJsonDocument &) { refreshSoon(800); });
}

// Pular faixa desliga a repetição de uma faixa no librespot, como no app
// oficial; a repetição da fila continua.
void Player::skipLeavesTrackRepeat() {
  if (m_repeatWanted != QLatin1String("track"))
    return;
  m_repeatMode = m_repeatWanted = QStringLiteral("context");
  m_repeatClock.restart();
  emit stateChanged();
}

void Player::setOrderedQueue(const QVariantList &rows) {
  m_orderedRows = rows.mid(0, 500);
}

// Fila conhecida pelo próprio app (mix aplicado): mostra as próximas na hora,
// inclusive quando a faixa atual não muda e o Spotify ainda não atualizou.
bool Player::queueFromOrderedRows() {
  if (m_orderedRows.isEmpty())
    return false;
  int index = -1;
  for (int i = 0; i < m_orderedRows.size(); ++i)
    if (m_orderedRows[i].toMap().value(QStringLiteral("uri")).toString() == m_trackUri) {
      index = i;
      break;
    }
  // Logo após a carga a faixa ainda pode ser a anterior; quem invalida a fila
  // ordenada é uma seleção comum (selectionReplaced), não esta consulta.
  if (index < 0)
    return false;
  m_queue.clear();
  for (int i = index + 1; i < m_orderedRows.size(); ++i) {
    const QVariantMap row = m_orderedRows[i].toMap();
    QVariantMap item;
    item.insert(QStringLiteral("uri"), row.value(QStringLiteral("uri")));
    item.insert(QStringLiteral("name"), row.value(QStringLiteral("title"), row.value(QStringLiteral("name"))));
    item.insert(QStringLiteral("artists"), row.value(QStringLiteral("artist"), row.value(QStringLiteral("artists"))));
    item.insert(QStringLiteral("artistId"), row.value(QStringLiteral("artistId")));
    item.insert(QStringLiteral("album"), row.value(QStringLiteral("album")));
    item.insert(QStringLiteral("albumId"), row.value(QStringLiteral("albumId")));
    item.insert(QStringLiteral("year"), row.value(QStringLiteral("year")));
    item.insert(QStringLiteral("coverUrl"), row.value(QStringLiteral("image"), row.value(QStringLiteral("coverUrl"))));
    item.insert(QStringLiteral("durationMs"), row.value(QStringLiteral("durationMs")));
    item.insert(QStringLiteral("mixExternal"), row.value(QStringLiteral("mixExternal")).toBool());
    m_queue << item;
  }
  emit queueChanged();
  emit queueRefreshFinished(200);
  return true;
}

void Player::toggleLike() {
  if (localPlayback())
    return;
  if (!m_trackUri.startsWith(QLatin1String("spotify:track:")))
    return;
  const bool newState = !m_liked;
  m_liked = newState;
  emit likedChanged();
  const QVariantMap q{{QStringLiteral("uris"), m_trackUri}};
  auto done = [this, newState, uri=m_trackUri](int status, const QJsonDocument &) {
    if(uri!=m_trackUri)return;
    if (status < 200 || status >= 300) { // desfaz o otimismo
      m_liked = !newState;
      emit likedChanged();
      emit message(tr("Não consegui atualizar suas Músicas Curtidas."));
    } else {
      emit message(newState ? tr("Adicionada às Músicas Curtidas")
                            : tr("Removida das Músicas Curtidas"));
    }
  };
  if (newState)
    m_api->put(QStringLiteral("/me/library"), q, {}, done);
  else
    m_api->del(QStringLiteral("/me/library"), q, {}, done);
}

void Player::playContext(const QString &contextUri, const QString &offsetUri,
                         int offsetIndex) {
  emit playbackRequested();
  ++m_transportGeneration;
  if (localPlayback())
    m_audio->returnToSpotify();
  cancelResume();
  m_lastUris.clear();
  QJsonObject body{{QStringLiteral("context_uri"), contextUri}};
  if (!offsetUri.isEmpty())
    body.insert(QStringLiteral("offset"),
                QJsonObject{{QStringLiteral("uri"), offsetUri}});
  else if (offsetIndex >= 0)
    body.insert(QStringLiteral("offset"),
                QJsonObject{{QStringLiteral("position"), offsetIndex}});
  sendPlay(body, targetDeviceForPlay());
}

void Player::playUris(const QStringList &uris, int offsetIndex) {
  emit playbackRequested();
  ++m_transportGeneration;
  if (localPlayback())
    m_audio->returnToSpotify();
  cancelResume();
  if (uris.isEmpty())
    return;
  // A API aceita no máximo algumas centenas de URIs; mandamos uma janela em
  // volta da faixa escolhida.
  const int start = qMax(0, offsetIndex - 50);
  const QStringList window = uris.mid(start, 300);
  m_lastUris = window;
  QJsonObject body{
      {QStringLiteral("uris"), QJsonArray::fromStringList(window)},
      {QStringLiteral("offset"),
       QJsonObject{{QStringLiteral("position"), offsetIndex - start}}},
  };
  sendPlay(body, targetDeviceForPlay());
}

void Player::playOrderedUris(const QStringList& uris){playOrdered(uris,0,false);}
void Player::playOrderedUrisAt(const QStringList& uris,int position){playOrdered(uris,position,true);}
// keepCurrent: reaplicar a fila a partir da faixa que já toca (Fila, Shake,
// Aleatório, Descoberta). Uma seleção nova começa do início pedido.
void Player::playOrdered(const QStringList& uris,int position,bool keepCurrent){if(uris.isEmpty())return;emit playbackRequested();cancelResume();
  // Reaplicar a fila começando pela faixa que já toca: o librespot reaproveita o
  // decoder e só ajusta a posição. Usa a posição de agora mais a latência medida
  // do comando, para o ajuste cair onde o áudio estará, sem voltar um trecho.
  if(keepCurrent&&uris.first()==m_trackUri&&m_isPlaying&&playingHere()&&!localPlayback()){const int ahead=positionMs()+m_commandLatencyMs;position=m_durationMs>0?qMin(m_durationMs-1,ahead):ahead;}if(localPlayback())m_audio->returnToSpotify();m_lastUris=uris.mid(0,500);m_shuffle=false;emit stateChanged();QJsonObject body{{"uris",QJsonArray::fromStringList(m_lastUris)},{"position_ms",std::max(0,position)},{"offset",QJsonObject{{"position",0}}}};sendPlay(body,targetDeviceForPlay(),false,true);}

void Player::addToQueue(const QString &uri) {
  m_api->post(
      QStringLiteral("/me/player/queue"), {{QStringLiteral("uri"), uri}}, {},
      [this](int status, const QJsonDocument &) {
        emit message(status < 300 ? tr("Adicionada à fila")
                                  : tr("Não consegui adicionar à fila"));
        refreshQueue();
      });
}

void Player::transferTo(const QString &deviceId) {
  if (localPlayback())
    m_audio->returnToSpotify();
  cancelResume();
  if (deviceId.isEmpty())
    return;
  ++m_transportGeneration;
  m_transportIntentClock.restart();
  const QJsonObject body{
      {QStringLiteral("device_ids"), QJsonArray{deviceId}},
      {QStringLiteral("play"), true},
  };
  m_api->put(QStringLiteral("/me/player"), {}, QJsonDocument(body),
             [this](int status, const QJsonDocument &) {
               if (status >= 300)
                 emit message(tr("Não consegui trocar de dispositivo."));
               refreshSoon(700);
               QTimer::singleShot(1500, this, &Player::refreshDevices);
             });
}
