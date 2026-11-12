#include "Player.h"

#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "SpotifyApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QRegularExpression>
#include <QSet>

#include <climits>
#include <algorithm>
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
    m.insert(QStringLiteral("artistCredits"), artists.toVariantList());
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
  m_volume = qBound(0, QSettings().value(QStringLiteral("player/volume"), 60).toInt(), 100);

  m_pollTimer.setSingleShot(true);
  connect(&m_pollTimer, &QTimer::timeout, this, &Player::refresh);

  // Só redesenha a barra de progresso enquanto toca.
  m_tickTimer.setInterval(100);
  connect(&m_tickTimer, &QTimer::timeout, this, [this] {
    emit positionChanged();
    if (m_durationMs > 0 && positionMs() >= m_durationMs)
      refreshSoon(300);
  });

  // Throttle (not debounce): dragging continuously still updates the device.
  // One request in flight + one latest value; never a queue of mouse pixels.
  m_volumeTimer.setSingleShot(true);
  m_volumeTimer.setInterval(150);
  connect(&m_volumeTimer, &QTimer::timeout, this, [this] {
    QSettings().setValue(QStringLiteral("player/volume"), m_volume);
    dispatchVolume();
  });

  m_seekTimer.setSingleShot(true);
  m_seekTimer.setInterval(60);
  connect(&m_seekTimer, &QTimer::timeout, this, [this] {
    if (m_seekTarget < 0 || localPlayback())
      return;
    m_api->put(QStringLiteral("/me/player/seek"),
               {{QStringLiteral("position_ms"), m_seekTarget}}, {},
               [this](int, const QJsonDocument &) { refreshSoon(900); });
  });

  connect(m_librespot, &LibrespotProcess::playbackEvent, this,
          [this] { refreshSoon(250); });
  connect(m_librespot, &LibrespotProcess::queueEditFinished, this, &Player::finishQueueEdit);
  connect(m_librespot, &LibrespotProcess::stateChanged, this, &Player::queueChanged);
  connect(m_librespot, &LibrespotProcess::sessionLost, this,
          &Player::rememberForResume);
  connect(m_librespot, &LibrespotProcess::stateChanged, this, [this] {
    if (m_librespot->state() == LibrespotProcess::Ready) {
      m_engineReadyClock.restart();
      findLocalDevice(8);
    }
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
    m_queueCatalogUri.clear();
    m_queueCatalogRows.clear();
    m_queueCatalogAge.invalidate();
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
      cancelVolume();
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
  const auto volumeGeneration = m_volumeGeneration;
  const auto volumeRouteGeneration = m_volumeRouteGeneration;
  m_api->get(QStringLiteral("/me/player"),
             {{QStringLiteral("additional_types"), QStringLiteral("episode")}},
             [this, transportGeneration, seekGeneration, volumeGeneration, volumeRouteGeneration](int status, const QJsonDocument &doc) {
               m_requestInFlight = false;
               if (transportGeneration != m_transportGeneration ||
                   seekGeneration != m_seekGeneration ||
                   volumeRouteGeneration != m_volumeRouteGeneration) {
                 refreshSoon(250);
                 return;
               }
               if (status == 200 && doc.isObject())
                 applyState(doc.object(), volumeGeneration == m_volumeGeneration);
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

void Player::applyState(const QJsonObject &obj, bool acceptVolume) {
  if (localPlayback())
    return;
  const QJsonObject item = obj.value(QStringLiteral("item")).toObject();
  const QString uri = item.value(QStringLiteral("uri")).toString();

  const bool sameTrack = uri == m_trackUri;
  const bool wasPlaying = m_isPlaying;
  const int displayedPosition = positionMs();
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
  }

  const QJsonObject device = obj.value(QStringLiteral("device")).toObject();
  const QJsonObject context = obj.value(QStringLiteral("context")).toObject();
  m_isPlaying = obj.value(QStringLiteral("is_playing")).toBool();
  if (obj.contains(QStringLiteral("shuffle_state")))
    m_shuffle = obj.value(QStringLiteral("shuffle_state")).toBool();
  // Repetição: logo depois de uma troca do usuário ou de uma nova seleção (que
  // o librespot zera até reapplyRepeat chegar), um GET antigo não manda.
  const QString remoteRepeat =
      obj.value(QStringLiteral("repeat_state")).toString(QStringLiteral("off"));
  const bool repeatSettling = m_repeatClock.isValid() && m_repeatClock.elapsed() < 3000;
  const bool selectionLoading = m_selectionClock.isValid() && m_selectionClock.elapsed() < 4000;
  if (!repeatSettling && !(selectionLoading && m_repeatWanted != QLatin1String("off")))
    m_repeatMode = m_repeatWanted = remoteRepeat;
  const auto previousContext = m_contextUri;
  const auto reportedContext = context.value(QStringLiteral("uri")).toString();
  const QString deviceId = device.value(QStringLiteral("id")).toString();
  const bool volumeRouteChanged = deviceId != m_activeDeviceId;
  if (!reportedContext.isEmpty() || volumeRouteChanged)
    m_contextUri = reportedContext;
  if (m_contextUri.startsWith("spotify:user:") && m_contextUri.contains(":playlist:"))
    m_contextUri = "spotify:playlist:" + m_contextUri.section(':', -1);
  if (volumeRouteChanged)
    cancelVolume();
  m_activeDeviceId = deviceId;
  m_activeDeviceName = device.value(QStringLiteral("name")).toString();
  if (!sameTrack || previousContext != m_contextUri || volumeRouteChanged)
    refreshQueue();
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
      (acceptVolume || volumeRouteChanged) && m_pendingVolume < 0 &&
      !m_volumeInFlight && (!m_volumeIntentClock.isValid() ||
                            m_volumeIntentClock.elapsed() >= 3000)) {
    const int v = qBound(0, device.value(QStringLiteral("volume_percent")).toInt(), 100);
    // Logo após o motor voltar (reinício automático, nova sessão), o Spotify
    // informa o volume inicial do motor; um salto grande não foi pedido aqui.
    if (v != m_volume && playingHere() && m_engineReadyClock.isValid() &&
        m_engineReadyClock.elapsed() < 15000 && qAbs(v - m_volume) > 10) {
      reassertVolume();
    } else if (v != m_volume) {
      m_volume = v;
      if (playingHere())
        QSettings().setValue(QStringLiteral("player/volume"), m_volume);
      emit volumeChanged();
    }
  }

  const int remote = obj.value(QStringLiteral("progress_ms")).toInt();
  if (!seekSettling(remote)) {
    const bool smallBackward = sameTrack && wasPlaying && m_isPlaying && remote < displayedPosition && displayedPosition - remote <= 1500;
    setPositionBase(smallBackward ? displayedPosition : remote);
  }
  if (m_isPlaying)
    m_tickTimer.start();
  else
    m_tickTimer.stop();
}

void Player::clearState() {
  cancelVolume();
  const bool hadTrack = hasTrack();
  m_isPlaying = false;
  m_tickTimer.stop();
  m_activeDeviceId.clear();
  m_activeDeviceName.clear();
  emit stateChanged();
  if (hadTrack && !m_auth->isAuthenticated()) {
    m_contextUri.clear();
    m_queueContext.clear();
    m_queue.clear();
    m_orderedRows.clear();
    ++m_queueRevision;
    emit queueChanged();
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
  const auto device = targetDeviceForPlay();
  const auto context = m_contextUri;
  const bool catalog = context.startsWith("spotify:album:") || context.startsWith("spotify:playlist:");
  const bool shuffled = m_shuffle;
  const auto key = QStringList{current, device, context, QString::number(generation), QString::number(shuffled)};
  if (key == m_queueReadKey)
    return;
  m_queueReadKey = key;
  const auto serial = ++m_queueReadSerial;
  auto valid = [this, generation, current, device, context, shuffled] {
    return generation == m_transportGeneration && current == m_trackUri &&
           device == targetDeviceForPlay() && context == m_contextUri && shuffled == m_shuffle;
  };
  auto finish = [this, serial, valid](int status, QVariantList rows, bool partial) {
    if (serial != m_queueReadSerial)
      return;
    m_queueReadKey.clear();
    if (!valid()) {
      emit queueRefreshFinished(409);
      return;
    }
    if (status == 200)
      publishQueue(std::move(rows), partial);
    emit queueRefreshFinished(status);
  };
  m_api->get("/me/player/queue", {}, [this, serial, valid, finish, current, context, catalog, shuffled]
             (int status, const QJsonDocument &doc) {
    if (serial != m_queueReadSerial)
      return;
    const auto object = doc.object();
    const auto reported = object["currently_playing"].toObject()["uri"].toString();
    if (!valid() || (status == 200 && !reported.isEmpty() && reported != current)) {
      finish(409, {}, true);
      return;
    }
    if (status != 200) {
      finish(status, {}, true);
      return;
    }
    QVariantList observed;
    for (const auto &value : object["queue"].toArray())
      if (value.isObject()) observed << simplifyItem(value.toObject());
    if (!catalog || shuffled) {
      const bool partial = catalog || observed.size() >= 20;
      finish(200, std::move(observed), partial);
      return;
    }

    auto merge = [finish, observed, current](const QVariantList &rows, bool complete) mutable {
      // Keep the service's exact front (including manual additions), then append
      // the catalog tail only after a unique, consecutive match.
      qsizetype best = -1, suffix = -1;
      for (qsizetype i = 0; i < rows.size(); ++i) {
        if (rows[i].toMap().value("uri") != current)
          continue;
        for (qsizetype count = qMin(observed.size(), rows.size() - i - 1); count >= 0; --count) {
          if (count == 0 && !observed.isEmpty())
            break;
          bool matches = true;
          for (qsizetype j = 0; j < count && matches; ++j)
            matches = observed[observed.size()-count+j].toMap().value("uri") == rows[i+1+j].toMap().value("uri");
          if (!matches)
            continue;
          if (count > best) {
            best = count;
            suffix = i + 1 + count;
          } else if (count == best) {
            suffix = -1;
          }
          break;
        }
      }
      if (suffix < 0 || (best < observed.size() && best < 2)) {
        finish(200, std::move(observed), true);
        return;
      }
      const auto total = observed.size() + rows.size() - suffix;
      observed += rows.mid(suffix, qMax<qsizetype>(0, 499 - observed.size()));
      finish(200, observed.mid(0, 499), !complete || total > 499);
    };

    if (m_queueCatalogUri == context && m_queueCatalogAge.isValid() && m_queueCatalogAge.elapsed() < 60000) {
      merge(m_queueCatalogRows, true);
      return;
    }
    const auto id = context.section(':', -1);
    const auto path = context.startsWith("spotify:album:") ? "/albums/" + id : "/playlists/" + id + "/items?limit=50";
    loadContextQueue(path, context, [this, serial, valid]{return serial == m_queueReadSerial && valid();},
      [this, context, serial, valid, merge](QVariantList rows, bool complete) mutable {
        if (serial == m_queueReadSerial && valid() && complete) {
          m_queueCatalogUri = context;
          m_queueCatalogRows = rows;
          m_queueCatalogAge.restart();
        }
        merge(rows, complete);
      });
  });
}

void Player::loadContextQueue(const QString &path, const QString &context,
                              std::function<bool()> valid, std::function<void(QVariantList,bool)> done,
                              QVariantList rows, QJsonObject album, int page) {
  m_api->get(path, {}, [this, path, context, valid, done, rows=std::move(rows), album, page]
             (int status, const QJsonDocument &doc) mutable {
    if (!valid() || status != 200) {
      done(std::move(rows), false);
      return;
    }
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
      if (!item["uri"].toString().startsWith("spotify:track:") || item["is_local"].toBool() ||
          (item.contains("is_playable") && !item["is_playable"].toBool())) continue;
      if (!item.contains("album") && !album.isEmpty()) item["album"] = album;
      if (rows.size() < 2000) rows << simplifyItem(item);
    }
    const auto next = data["next"].toString();
    if (!next.isEmpty() && next != path && page < 39 && rows.size() < 2000) {
      loadContextQueue(next, context, valid, done, std::move(rows), album, page + 1);
      return;
    }
    done(std::move(rows), next.isEmpty());
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
  // Faixas puladas por falta de chave: retoma pela primeira, do começo.
  const QString failed = m_librespot->takeFailedTrack();
  if (!m_isPlaying || !playingHere() || m_trackUri.isEmpty())
    return;
  const QString track = failed.isEmpty() ? m_trackUri : failed;
  QJsonObject body{{QStringLiteral("position_ms"), failed.isEmpty() ? positionMs() : 0}};
  const QJsonObject offset{{QStringLiteral("uri"), track}};
  const bool knownOrder = m_orderedQueueDevice == m_localDeviceId &&
                          !m_orderedRows.isEmpty() && m_lastUris.contains(track);
  if (!knownOrder && !m_contextUri.isEmpty() && m_contextUri != "spotify:web-api") {
    body.insert(QStringLiteral("context_uri"), m_contextUri);
    body.insert(QStringLiteral("offset"), offset);
  } else if (m_lastUris.contains(track)) {
    // Lista solta (busca, populares): manda a lista toda para manter o que vem
    // depois. Com "uris" usamos offset por posição, o mesmo formato do
    // playUris.
    body.insert(QStringLiteral("uris"), QJsonArray::fromStringList(m_lastUris));
    body.insert(QStringLiteral("offset"),
                QJsonObject{{QStringLiteral("position"),
                             int(m_lastUris.indexOf(track))}});
  } else {
    body.insert(QStringLiteral("uris"), QJsonArray{track});
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
    m_queueEditPending = false;
    m_queueEditInFlight = 0;
    cancelSeek();
    m_selectionClock.restart();
    if (!ordered) {
      // Seleção comum (álbum, playlist, busca): a fila do mix deixa de valer.
      m_orderedRows.clear();
      m_contextUri.clear();
      m_queueTrack.clear();
      ++m_queueRevision;
      emit queueChanged();
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
  ++m_volumeGeneration;
  m_volumeIntentClock.restart();
  if (localPlayback()) {
    m_audio->setLocalVolume(percent);
  } else {
    m_pendingVolume = percent;
    m_volumeDevice = m_activeDeviceId;
  }
  emit volumeChanged();
  if (!m_volumeTimer.isActive())
    m_volumeTimer.start(150);
}

void Player::toggleMute() {
  if (m_volume > 0) {
    QSettings().setValue(QStringLiteral("player/volumeBeforeMute"), m_volume);
    setVolume(0);
  } else {
    setVolume(qBound(1, QSettings().value(QStringLiteral("player/volumeBeforeMute"), 30).toInt(), 100));
  }
}

// Reenvia ao aparelho local o volume que o usuário escolheu.
void Player::reassertVolume() {
  if (localPlayback() || m_activeDeviceId.isEmpty())
    return;
  m_engineReadyClock.invalidate();
  ++m_volumeGeneration;
  m_volumeIntentClock.restart();
  m_pendingVolume = m_volume;
  m_volumeDevice = m_activeDeviceId;
  if (!m_volumeTimer.isActive())
    m_volumeTimer.start(0);
}

void Player::cancelVolume() {
  if (m_volumeTimer.isActive())
    QSettings().setValue(QStringLiteral("player/volume"), m_volume);
  m_volumeTimer.stop();
  m_pendingVolume = -1;
  m_volumeInFlight = false;
  m_volumeDevice.clear();
  m_volumeIntentClock.invalidate();
  ++m_volumeGeneration;
  ++m_volumeRouteGeneration;
}

void Player::dispatchVolume() {
  if (localPlayback() || m_pendingVolume < 0 || m_volumeInFlight)
    return;
  if (m_volumeDevice.isEmpty() || m_volumeDevice != m_activeDeviceId) {
    cancelVolume();
    emit message(tr("Nenhum dispositivo ativo para ajustar o volume."));
    refreshSoon(0);
    return;
  }
  const int delay = m_volumeSentClock.isValid() ? 150 - int(m_volumeSentClock.elapsed()) : 0;
  if (delay > 0) {
    m_volumeTimer.start(delay);
    return;
  }
  const int wanted = std::exchange(m_pendingVolume, -1);
  const auto generation = m_volumeGeneration;
  const auto route = m_volumeRouteGeneration;
  const auto device = m_volumeDevice;
  const QPointer<Player> self(this);
  m_volumeInFlight = true;
  m_volumeSentClock.restart();
  m_api->put(QStringLiteral("/me/player/volume"),
      {{QStringLiteral("volume_percent"), wanted}, {QStringLiteral("device_id"), device}}, {},
      [this, self, generation, route](int status, const QJsonDocument &) {
        if (!self || route != m_volumeRouteGeneration)
          return;
        m_volumeInFlight = false;
        if (m_pendingVolume >= 0) {
          dispatchVolume();
          return;
        }
        if (generation != m_volumeGeneration)
          return;
        if (status >= 200 && status < 300) {
          ++m_volumeGeneration; // invalidate GETs started before this write completed
          m_volumeIntentClock.restart(); // bounded eventual-consistency guard
          refreshSoon(350);
        } else if (status != 499) {
          m_volumeIntentClock.invalidate();
          emit message(tr("Não foi possível ajustar o volume no dispositivo. Tente novamente."));
          refreshSoon(0);
        }
      },
      [this, self, generation, route, device] {
        return self && generation == m_volumeGeneration && route == m_volumeRouteGeneration &&
               !localPlayback() && device == m_activeDeviceId;
      }, true);
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
  m_orderedQueueDevice = targetDeviceForPlay();
}

// Fila conhecida pelo próprio app (mix aplicado): mostra as próximas na hora,
// inclusive quando a faixa atual não muda e o Spotify ainda não atualizou.
bool Player::queueFromOrderedRows() {
  if (m_orderedRows.isEmpty() || m_orderedQueueDevice != targetDeviceForPlay())
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
  QVariantList upcoming;
  for (int i = index + 1; i < m_orderedRows.size(); ++i) {
    const QVariantMap row = m_orderedRows[i].toMap();
    QVariantMap item;
    item.insert(QStringLiteral("uri"), row.value(QStringLiteral("uri")));
    item.insert(QStringLiteral("name"), row.value(QStringLiteral("title"), row.value(QStringLiteral("name"))));
    item.insert(QStringLiteral("artists"), row.value(QStringLiteral("artist"), row.value(QStringLiteral("artists"))));
    item.insert(QStringLiteral("artistId"), row.value(QStringLiteral("artistId")));
    item.insert(QStringLiteral("artistCredits"), row.value(QStringLiteral("artistCredits")));
    item.insert(QStringLiteral("album"), row.value(QStringLiteral("album")));
    item.insert(QStringLiteral("albumId"), row.value(QStringLiteral("albumId")));
    item.insert(QStringLiteral("year"), row.value(QStringLiteral("year")));
    item.insert(QStringLiteral("coverUrl"), row.value(QStringLiteral("image"), row.value(QStringLiteral("coverUrl"))));
    item.insert(QStringLiteral("durationMs"), row.value(QStringLiteral("durationMs")));
    item.insert(QStringLiteral("mixExternal"), row.value(QStringLiteral("mixExternal")).toBool());
    item.insert(QStringLiteral("queueAdded"), row.value(QStringLiteral("queueAdded")).toLongLong());
    upcoming << item;
  }
  publishQueue(std::move(upcoming));
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

void Player::playOrderedUris(const QStringList &uris) {
  if (uris.isEmpty())
    return;
  emit playbackRequested();
  cancelResume();
  if (localPlayback())
    m_audio->returnToSpotify();
  m_lastUris = uris.mid(0, 500);
  m_shuffle = false;
  emit stateChanged();
  const auto device = targetDeviceForPlay();
  if (!device.isEmpty() && device == m_localDeviceId && m_librespot->playQueue(m_lastUris)) {
    ++m_transportGeneration;
    m_transportIntentClock.restart();
    m_selectionClock.restart();
    cancelSeek();
    m_queueEditPending = false;
    m_queueEditInFlight = 0;
    QTimer::singleShot(650, this, [this, device, generation=m_transportGeneration]{
      if (generation != m_transportGeneration) return;
      reapplyRepeat(device);
      refresh();
    });
    return;
  }
  sendPlay({{"uris", QJsonArray::fromStringList(m_lastUris)},
            {"position_ms", 0}, {"offset", QJsonObject{{"position", 0}}}},
           device, false, true);
}

void Player::playSavedQueue(const QVariantList &tracks, int index) {
  emit selectionReplaced();
  QVariantList rows;
  QStringList uris;
  for (qsizetype i = index; i < tracks.size() && rows.size() < 500; ++i) {
    const auto row = tracks[i].toMap();
    const auto uri = row.value("uri").toString();
    if (!uri.startsWith("spotify:track:"))
      continue;
    rows << row;
    uris << uri;
  }
  if (rows.isEmpty())
    return;
  setOrderedQueue(rows);
  playOrderedUris(uris);
}

bool Player::applyQueueEdit(const QVariantList &rows) {
  if (!queueEditingAvailable()) {
    emit message(tr("A edição da fila requer o motor local do Tonalify."));
    return false;
  }
  if (m_queuePartial && m_queueTrack == m_trackUri) {
    emit message(tr("A fila disponível é parcial. A ordem atual foi preservada."));
    return false;
  }
  return commitQueueEdit(rows);
}

void Player::addToQueue(const QString &uri) {
  m_api->post(
      QStringLiteral("/me/player/queue"), {{QStringLiteral("uri"), uri}}, {},
      [this](int status, const QJsonDocument &) {
        emit message(status < 300 ? tr("Adicionada à fila")
                                  : tr("Não consegui adicionar à fila"));
        refreshQueue();
      });
}

void Player::publishQueue(QVariantList rows, bool partial) {
  const auto device = targetDeviceForPlay();
  if (partial && !m_shuffle && m_queueTrack == m_trackUri && m_queueDevice == device &&
      !m_contextUri.isEmpty() && m_queueContext == m_contextUri && rows.size() >= 2 &&
      rows.size() < m_queue.size()) {
    const bool matching = std::equal(rows.cbegin(), rows.cend(), m_queue.cbegin(),
      [](const QVariant &left, const QVariant &right) {
        return left.toMap().value("uri") == right.toMap().value("uri");
      });
    if (matching)
      rows = m_queue;
  }
  if (rows == m_queue && m_queueTrack == m_trackUri && m_queueDevice == device &&
      m_queueContext == m_contextUri && m_queuePartial == partial)
    return;
  m_queue = std::move(rows);
  m_queuePartial = partial;
  m_queueTrack = m_trackUri;
  m_queueContext = m_contextUri;
  m_queueDevice = device;
  ++m_queueRevision;
  emit queueChanged();
}

bool Player::queueEditingAvailable() const {
  return !localPlayback() && playingHere() && m_librespot->queueEditingAvailable();
}

QString Player::queueEditToken() const {
  if (m_queuePartial || !queueEditingAvailable() || !m_trackUri.startsWith("spotify:track:") ||
      m_queueTrack != m_trackUri || m_queueDevice.isEmpty() ||
      m_queueDevice != targetDeviceForPlay() || m_queue.size() >= 500 || m_queueCapacityExceeded)
    return {};
  return QString::number(m_queueRevision);
}

QVariantList Player::queueForEditing() const {
  for (int i = 0; i < m_orderedRows.size(); ++i)
    if (m_orderedQueueDevice == targetDeviceForPlay() && m_orderedRows[i].toMap().value("uri") == m_trackUri)
      return m_orderedRows.mid(i);

  QVariantList rows{QVariantMap{{"provider","Spotify"},{"id",m_trackUri},{"uri",m_trackUri},
    {"title",m_trackName},{"artist",artistsText()},{"artistCredits",m_artists},
    {"album",m_albumName},{"albumId",m_albumUri.section(':',-1)},{"image",m_coverUrl},{"durationMs",m_durationMs}}};
  for (const auto &entry : m_queue) {
    auto row = entry.toMap();
    row["provider"] = "Spotify";
    row["id"] = row.value("uri");
    row["title"] = row.value("name");
    row["artist"] = row.value("artists");
    row["image"] = row.value("coverUrl");
    rows << row;
  }
  return rows;
}

bool Player::editQueue(const QString &token, const QVariantList &order) {
  if (token.isEmpty() || token != queueEditToken()) {
    emit message(tr("A fila ou o dispositivo mudou. Tente novamente."));
    return false;
  }
  const auto current = queueForEditing();
  QVariantList rows{current.first()};
  QSet<int> used;
  for (const auto &entry : order) {
    bool ok = false;
    const int index = entry.toInt(&ok);
    if (!ok || index < 0 || index >= current.size() - 1 || used.contains(index))
      return false;
    used.insert(index);
    rows << current.at(index + 1);
  }
  return commitQueueEdit(rows);
}

bool Player::commitQueueEdit(const QVariantList &rows) {
  static const QRegularExpression valid("^spotify:track:[A-Za-z0-9]{22}$");
  if (!queueEditingAvailable() || m_queueCapacityExceeded || rows.isEmpty() || rows.size() > 500 || rows.first().toMap().value("uri") != m_trackUri)
    return false;
  for (const auto &row : rows) {
    if (!valid.match(row.toMap().value("uri").toString()).hasMatch()) {
      emit message(tr("A fila contém itens incompatíveis com esta edição."));
      return false;
    }
  }
  setOrderedQueue(rows);
  m_queueEditAnchor = m_trackUri;
  m_queueEditDevice = targetDeviceForPlay();
  m_queueEditPending = true;
  emit orderedQueueEdited(rows);
  queueFromOrderedRows();
  dispatchQueueEdit();
  return queueEditPending();
}

void Player::setQueueCapacityExceeded(bool exceeded) {
  if (m_queueCapacityExceeded == exceeded)
    return;
  m_queueCapacityExceeded = exceeded;
  ++m_queueRevision;
  emit queueChanged();
}

void Player::dispatchQueueEdit() {
  if (!m_queueEditPending || m_queueEditInFlight != 0)
    return;
  m_queueEditPending = false;
  if (!queueEditingAvailable() || m_trackUri != m_queueEditAnchor ||
      targetDeviceForPlay() != m_queueEditDevice)
    return;
  m_sentQueueUris.clear();
  for (const auto &row : m_orderedRows)
    m_sentQueueUris << row.toMap().value("uri").toString();
  m_sentQueueDevice = m_queueEditDevice;
  const auto serial = ++m_queueEditSerial;
  m_queueEditInFlight = serial;
  if (!m_librespot->writeQueueEdit(serial, m_sentQueueUris)) {
    finishQueueEdit(serial, false);
    return;
  }
  QTimer::singleShot(4000, this, [this, serial] { finishQueueEdit(serial, false); });
}

void Player::finishQueueEdit(quint64 serial, bool accepted) {
  if (serial != m_queueEditInFlight)
    return;
  m_queueEditInFlight = 0;
  if (accepted && m_sentQueueDevice == targetDeviceForPlay()) {
    m_lastUris = m_sentQueueUris;
    m_shuffle = false;
    emit stateChanged();
  } else if (!m_queueEditPending && m_sentQueueDevice == targetDeviceForPlay()) {
    m_orderedRows.clear();
    emit selectionReplaced();
    m_queueTrack.clear();
    emit queueChanged();
    emit message(tr("O motor não confirmou a edição da fila. Tente novamente."));
    refreshQueue();
  }
  dispatchQueueEdit();
}

void Player::insertInQueue(const QVariantMap &track, int placement) {
  if (!queueEditingAvailable()) {
    emit message(tr("A edição da fila requer o motor local do Tonalify."));
    return;
  }
  static const QRegularExpression valid("^spotify:track:[A-Za-z0-9]{22}$");
  if (localPlayback() || m_trackUri.isEmpty() || targetDeviceForPlay().isEmpty() ||
      !valid.match(track.value("uri").toString()).hasMatch() || placement < 0 || placement > 2) {
    emit message(tr("Inicie uma seleção no Tonalify antes de editar a fila."));
    return;
  }
  if (m_queueCapacityExceeded) {
    emit message(tr("A fila do Jam excede 500 músicas. Aguarde espaço para editar."));
    return;
  }
  if (m_queueInsertions.size() >= 500) {
    emit message(tr("A fila atingiu o limite de 500 músicas."));
    return;
  }
  m_queueInsertions.append({track, placement});
  if (m_queueInsertions.size() > 1)
    return;
  const auto anchor = m_trackUri, device = targetDeviceForPlay();
  auto link = std::make_shared<QMetaObject::Connection>();
  *link = connect(this, &Player::queueRefreshFinished, this, [this,link,anchor,device](int status) {
    disconnect(*link);
    const auto insertions = std::exchange(m_queueInsertions, {});
    if (status != 200 || anchor != m_trackUri || device != targetDeviceForPlay() || localPlayback()) {
      emit message(tr("Não foi possível carregar a fila desta música. Tente novamente."));
      return;
    }
    if (m_queuePartial) {
      emit message(tr("A fila disponível é parcial. A ordem atual foi preservada."));
      return;
    }
    auto rows = queueForEditing();
    const auto initialSize = rows.size();
    for (const auto &[track, placement] : insertions) {
      if (rows.size() >= 500)
        break;
      const auto json = QJsonObject::fromVariantMap(track);
      auto row = json.value("album").isObject() ? simplifyItem(json) : track;
      if (json.value("artists").isArray()) {
        row["artistCredits"] = json.value("artists").toArray().toVariantList();
        row["artistId"] = json.value("artists").toArray().at(0).toObject().value("id").toString();
        row["artist"] = joinArtists(json.value("artists").toArray());
      } else if (!row.contains("artist")) {
        row["artist"] = row.value("artists");
      }
      row["provider"] = "Spotify";
      row["id"] = row.value("uri");
      if (!row.contains("title")) row["title"] = row.value("name");
      if (!row.contains("image")) row["image"] = row.value("coverUrl");
      qsizetype at = placement == 0 ? rows.size() : 1;
      qint64 lastAdded = 0;
      if (placement == 2) {
        for (qsizetype i = 1; i < rows.size(); ++i) {
          const auto sequence = rows[i].toMap().value("queueAdded").toLongLong();
          if (sequence > lastAdded) {
            lastAdded = sequence;
            at = i + 1;
          }
        }
      }
      row["queueAdded"] = placement == 2 ? lastAdded + 1 : 0;
      rows.insert(at, row);
    }
    if (rows.size() > initialSize && commitQueueEdit(rows) && m_isPlaying)
      emit message(tr("Música adicionada à fila."));
    if (rows.size() - initialSize < insertions.size())
      emit message(tr("A fila atingiu o limite de 500 músicas."));
  });
  QTimer::singleShot(10000, this, [this,link] {
    if (disconnect(*link)) {
      m_queueInsertions.clear();
      emit message(tr("Não foi possível carregar a fila a tempo."));
    }
  });
  refreshQueue();
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
