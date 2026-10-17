#include "CatalogService.h"
#include "TokenStore.h"
#include <QCryptographicHash>
#include <QCollator>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QUrlQuery>
#include <QUuid>
#include <algorithm>

namespace {
constexpr int PageSize = 20;
QString text(const QJsonValue &v) {
  if (v.isArray()) {
    QStringList parts;
    for (const auto &p : v.toArray())
      parts << p.toString();
    return parts.join(", ");
  }
  if (v.isString())
    return v.toString();
  return v.isDouble() ? QString::number(v.toInteger()) : QString();
}
QString quote(QString value) {
  value.replace("\\", "\\\\");
  value.replace("\"", "\\\"");
  return "\"" + value + "\"";
}
QString routeArtist(const QVariantMap &route) {
  const auto artist = route.value("artist").toString();
  return artist.isEmpty() ? route.value("title").toString() : artist;
}
QVariantMap item(const QString &provider, const QString &id,
                 const QString &kind, const QString &title,
                 const QString &artist = {}) {
  return {{"provider", provider}, {"id", id},         {"kind", kind},
          {"title", title},       {"artist", artist}, {"playable", false},
          {"detail", ""},         {"image", ""},      {"url", ""}};
}
QString cover(const QJsonObject &o) {
  for (const auto &key : {"480x480", "150x150", "1000x1000"})
    if (o.contains(key))
      return o.value(key).toString();
  return {};
}
QString normalized(const QString &value) {
  QString result;
  for (auto ch :
       value.normalized(QString::NormalizationForm_D).toCaseFolded()) {
    if (ch.isLetterOrNumber())
      result += ch;
    else if (ch.category() != QChar::Mark_NonSpacing)
      result += ' ';
  }
  return result.simplified();
}
bool archiveProvider(const QString &p) {
  return p == "Live Music Archive" || p == "Internet Archive" || p == "Aadam Jacobs";
}
bool yes(const QJsonValue &v) { return v.toBool() || v.toString().compare("true", Qt::CaseInsensitive) == 0; }
QString recordingKind(const QJsonObject &d) {
  const auto title = normalized(d["title"].toString());
  if (QRegularExpression("\\b(demo|demos|rehearsal|rehearsals|outtakes|ensaio)\\b").match(title).hasMatch()) return "demo";
  if (d["mediatype"] == "etree" || d["identifier"].toString().startsWith("ajc") ||
      QRegularExpression("\\b(live|show|concert|gig|audience|soundboard)\\b").match(title).hasMatch() ||
      QRegularExpression("\\b(19|20)\\d{2}[- .]\\d{2}[- .]\\d{2}\\b").match(d["title"].toString()).hasMatch()) return "show";
  return "recording";
}
QString recordingLabel(const QString &kind) { return kind == "demo" ? "Demo / ensaio" : kind == "show" ? "Show" : "Gravação"; }
int match(const QString &candidate, const QString &query) {
  const auto value = normalized(candidate), q = normalized(query);
  if (q.isEmpty() || value.isEmpty())
    return 0;
  if (value == q)
    return 100;
  if (value.startsWith(q + ' '))
    return 75;
  if ((' ' + value + ' ').contains(' ' + q + ' '))
    return 65;
  const auto words = q.split(' '), found = value.split(' ');
  int hits = 0;
  for (const auto &word : words)
    if (found.contains(word))
      ++hits;
  return hits * 50 / words.size();
}
} // namespace

CatalogService::CatalogService(QObject *parent, bool loadConnections)
    : QObject(parent) {
  m_restoreConnections = loadConnections;
  reloadConnections();
  m_mbTimer.setInterval(1100);
  connect(&m_mbTimer, &QTimer::timeout, this, [this] {
    if (QDateTime::currentMSecsSinceEpoch() < m_mbNotBefore)
      return;
    if (!m_mbQueue.isEmpty())
      m_mbQueue.dequeue()();
    if (m_mbQueue.isEmpty())
      m_mbTimer.stop();
  });
}
void CatalogService::reloadConnections() {
  if (m_restoreConnections) {
    QSettings settings("Tonal", "IndieTonal");
    m_jamendo = settings.value("jamendoClientId").toString();
    m_server.clear();
    m_user.clear();
    m_password.clear();
    if (auto saved = TokenStore::readSecret("IndieTonal/OpenSubsonic")) {
      auto o = QJsonDocument::fromJson(*saved).object();
      m_server = o["server"].toString();
      m_user = o["user"].toString();
      m_password = o["password"].toString();
    }
  }
}
bool CatalogService::publicUrl(const QUrl &u) {
  return u.isValid() && u.scheme() == "https" && !u.host().isEmpty() &&
         u.userInfo().isEmpty() && u.fragment().isEmpty();
}
QUrl CatalogService::api(const QString &base, const QVariantMap &args) const {
  QUrl u(base);
  QUrlQuery q(u);
  for (auto it = args.cbegin(); it != args.cend(); ++it)
    q.addQueryItem(it.key(), it.value().toString());
  u.setQuery(q);
  return u;
}
QString CatalogService::identity(const QVariantMap &i) {
  // Keep variants and provider provenance. A title alone NEVER merges
  // recordings.
  return i.value("provider").toString() + ":" + i.value("kind").toString() +
         ":" + i.value("id").toString();
}
void CatalogService::message(const QString &p, const QString &s) {
  QString value = p + ": " + s;
  if (!m_messages.contains(value))
    m_messages << value;
  emit changed();
}
void CatalogService::reset(const QString &h) {
  ++m_generation;
  m_mbQueue.clear();
  m_mbTimer.stop();
  m_failed.clear();
  m_mbHasMore = false;
  m_lmaHasMore = false;
  m_pending = 0;
  auto old = m_replies;
  m_replies.clear();
  for (auto *r : old)
    r->abort();
  m_items.clear();
  m_seen.clear();
  m_messages.clear();
  m_liveStatus.clear();
  m_heading = h;
  m_page = 0;
  emit changed();
}
void CatalogService::fetch(const QString &p, const QUrl &url, Handler done) {
  if (!publicUrl(url)) {
    message(p, "Endereço HTTPS inválido");
    return;
  }
  if (cacheable(p, url)) {
    if (auto *cached = m_mbCache.object(url.toString());
        cached && QDateTime::currentMSecsSinceEpoch() - cached->savedAt <
                      15 * 60 * 1000) {
      done(QJsonDocument::fromJson(cached->bytes).object());
      emit changed();
      return;
    }
  }
  ++m_pending;
  emit changed();
  auto request = std::make_shared<Request>();
  request->provider = p;
  request->url = url;
  request->done = std::move(done);
  request->generation = m_generation;
  enqueue(request);
}
bool CatalogService::cacheable(const QString &p, const QUrl &url) const {
  return p == "MusicBrainz" && publicUrl(url) &&
         url.host() == "musicbrainz.org" && url.path().startsWith("/ws/2/");
}
qint64 CatalogService::retryDelay(const QByteArray &header, int attempt,
                                  qint64 now) {
  // Retry-After can be seconds or an HTTP date. Never retry before it.
  qint64 delay = attempt == 0 ? 2000 : 5000;
  bool ok = false;
  auto seconds = header.trimmed().toLongLong(&ok);
  if (ok && seconds >= 0)
    return std::max(delay, std::min(seconds, qint64(86400)) * 1000);
  auto date =
      QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date);
  if (date.isValid())
    delay = std::max(delay, date.toMSecsSinceEpoch() - now);
  return delay;
}
void CatalogService::enqueue(const std::shared_ptr<Request> &state) {
  auto run = [this, state] {
    if (state->generation != m_generation)
      return;
    QNetworkRequest request(state->url);
    request.setTransferTimeout(18000);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "IndieTonal/0.1.1 (local music discovery; "
                      "https://github.com/natheus/tonal-player)");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    auto *reply = m_get ? m_get(request) : m_net.get(request);
    m_replies << reply;
    connect(reply, &QNetworkReply::readyRead, this, [reply] {
      if (reply->bytesAvailable() > 6 * 1024 * 1024)
        reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, state] { dispatch(reply, state); });
  };
  if (state->provider == "MusicBrainz") {
    m_mbQueue.enqueue(run);
    if (!m_mbTimer.isActive())
      m_mbTimer.start();
  } else
    run();
}
void CatalogService::dispatch(QNetworkReply *r,
                              const std::shared_ptr<Request> &state) {
  r->deleteLater();
  m_replies.removeAll(r);
  if (state->generation != m_generation)
    return;
  const auto p = state->provider;
  const int status =
      r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const bool transient =
      status == 429 || status == 500 || status == 502 || status == 503 ||
      status == 504 ||
      (status == 0 &&
       (r->error() == QNetworkReply::TimeoutError ||
        r->error() == QNetworkReply::RemoteHostClosedError ||
        r->error() == QNetworkReply::TemporaryNetworkFailureError));
  m_messages.removeAll(state->notice);
  if (p == "MusicBrainz" && transient) {
    const auto delay = retryDelay(r->rawHeader("Retry-After"), state->attempt,
                                  QDateTime::currentMSecsSinceEpoch());
    m_mbNotBefore =
        std::max(m_mbNotBefore, QDateTime::currentMSecsSinceEpoch() + delay);
    if (state->attempt < 2 && delay <= 30000) {
      ++state->attempt;
      state->notice = p + QString(": Serviço temporariamente indisponível. "
                                  "Nova tentativa %1/2 em cerca de %2 s…")
                              .arg(state->attempt)
                              .arg((delay + 999) / 1000);
      m_messages << state->notice;
      enqueue(state);
      emit changed();
      return; // Keep this logical request busy during backoff.
    }
  }
  --m_pending;
  if (status == 429) {
    message(
        p,
        "Limite temporário de consultas. Aguarde antes de tentar novamente.");
  } else if (r->error() != QNetworkReply::NoError || status < 200 ||
             status >= 300) {
    message(p, QString("Consulta indisponível (HTTP %1). Outras fontes "
                       "continuam disponíveis.")
                   .arg(status));
  } else {
    auto bytes = r->readAll();
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(bytes, &error);
    if (bytes.size() > 6 * 1024 * 1024 ||
        error.error != QJsonParseError::NoError || !doc.isObject())
      message(p, "Resposta inválida ou muito grande");
    else {
      if (cacheable(p, state->url))
        m_mbCache.insert(
            state->url.toString(),
            new CachedResponse{bytes, QDateTime::currentMSecsSinceEpoch()},
            std::max(1, int((bytes.size() + 1023) / 1024)));
      state->done(doc.object());
      emit changed();
      return;
    }
  }
  m_failed << state;
  if (archiveProvider(p) && m_route.isEmpty())
    m_liveStatus = "Não foi possível consultar os shows. Use Tentar novamente.";
  if (transient && cacheable(p, state->url)) {
    if (auto *cached = m_mbCache.object(state->url.toString());
        cached && QDateTime::currentMSecsSinceEpoch() - cached->savedAt <
                      24 * 60 * 60 * 1000) {
      state->done(QJsonDocument::fromJson(cached->bytes).object());
      message(p, "Exibindo dados consultados anteriormente (até 24 h). "
                 "Tente novamente para atualizar.");
    }
  }
  emit changed();
}
void CatalogService::retryFailed() {
  if (busy() || m_failed.isEmpty())
    return;
  auto failed = m_failed;
  m_failed.clear();
  m_messages.clear();
  for (const auto &state : failed) {
    state->attempt = 0;
    state->notice.clear();
    ++m_pending;
    enqueue(state);
  }
  emit changed();
}
bool CatalogService::hasMore() const {
  if (archiveProvider(m_provider) && m_route.isEmpty())
    return m_lmaHasMore;
  if (m_provider == "MusicBrainz" || m_route.value("provider") == "MusicBrainz")
    return (m_route.isEmpty() || m_route.value("kind") == "artist") &&
           m_mbHasMore;
  if (!m_route.isEmpty() && archiveProvider(m_route.value("provider").toString()))
    return false;
  return !m_items.isEmpty();
}
void CatalogService::append(const QVariantMap &i) {
  if (i.value("id").toString().isEmpty() || m_seen.contains(identity(i)))
    return;
  m_seen.insert(identity(i));
  auto safe = i;
  if (!publicUrl(QUrl(safe.value("image").toString())))
    safe["image"] = QString();
  m_items << safe;
  // Detail pages retain the source's album/track order. Only discovery is
  // ranked; arrival order of independent HTTP requests must not set relevance.
  if (m_route.isEmpty() && !m_query.isEmpty())
    std::sort(
        m_items.begin(), m_items.end(), [this](const auto &a, const auto &b) {
          auto left = a.toMap(), right = b.toMap();
          const int x = relevance(left, m_query), y = relevance(right, m_query);
          if (x != y)
            return x > y;
          return identity(left) < identity(right);
        });
}
int CatalogService::relevance(const QVariantMap &i, const QString &query) {
  const int title = match(i.value("title").toString(), query);
  const int artist = match(i.value("artist").toString(), query);
  const auto kind = i.value("kind").toString();
  if (kind == "artist") {
    const int confidence = std::clamp(i.value("searchScore").toInt(), 0, 100);
    return title * 16 +
           (title ? (i.value("provider") == "MusicBrainz" ? 40 + confidence / 5
                     : i.value("verified").toBool()       ? 20
                                                          : 0)
                  : 0);
  }
  const int score = std::max(title * 10, artist == 100 ? 1400 : artist * 9);
  return score + (score ? (kind == "show" || kind == "demo" ? 35 + (i.value("curated").toBool() ? 25 : 0)
                           : kind == "album"              ? 20
                           : i.value("playable").toBool() ? 5
                                                          : 0)
                        : 0);
}
void CatalogService::search(const QString &query, const QString &provider) {
  reloadConnections();
  m_query = query.simplified().left(160);
  m_provider = provider;
  emit searchStarted(m_query, m_provider);
  m_route.clear();
  reset(m_query.isEmpty() ? "Descubra outras gravações"
                          : "Resultados para “" + m_query + "”");
  if (m_query.size() < 2) {
    message("Busca", "Digite pelo menos dois caracteres");
    return;
  }
  list(0);
}
void CatalogService::more() {
  if (busy() || canRetry() || !hasMore())
    return;
  list(++m_page);
}
void CatalogService::list(int page) {
  const int offset = page * PageSize;
  if (!m_route.isEmpty()) {
    QString p = m_route.value("provider").toString(),
            id = m_route.value("id").toString(),
            kind = m_route.value("kind").toString();
    if (p == "Audius") {
      QString path = kind == "artist" ? "users/" + id + "/tracks"
                                      : "playlists/" + id + "/tracks";
      fetch(p,
            api("https://api.audius.co/v1/" + path,
                {{"limit", PageSize}, {"offset", offset}}),
            [this](auto o) { audius(o, "track"); });
    } else if (p == "MusicBrainz") {
      if (kind == "artist")
        fetch(p,
              api("https://musicbrainz.org/ws/2/release-group",
                  {{"artist", id},
                   {"limit", PageSize},
                   {"offset", offset},
                   {"fmt", "json"}}),
              [this](auto o) { musicbrainz(o, "album"); });
      else if (kind == "album" && page == 0)
        fetch(p,
              api("https://musicbrainz.org/ws/2/release-group/" + id,
                  {{"inc", "releases"}, {"fmt", "json"}}),
              [this](auto o) { musicbrainz(o, "release"); });
      else if (kind == "release" && page == 0)
        fetch(
            p,
            api("https://musicbrainz.org/ws/2/release/" + id,
                {{"inc", "recordings+artist-credits+isrcs"}, {"fmt", "json"}}),
            [this](auto o) { musicbrainz(o, "track"); });
    } else if (archiveProvider(p) && page == 0) {
      fetch(p,
            QUrl("https://archive.org/metadata/" +
                 QString::fromLatin1(QUrl::toPercentEncoding(id))),
            [this, id, p](auto o) { archiveFiles(o, id, p); });
    } else if (p == "Jamendo") {
      QString path = kind == "artist" ? "artists/tracks/" : "albums/tracks/";
      fetch(p,
            api("https://api.jamendo.com/v3.0/" + path,
                {{"client_id", m_jamendo},
                 {"format", "json"},
                 {"id", id},
                 {"limit", PageSize},
                 {"offset", offset},
                 {"audioformat", "mp32"}}),
            [this](auto o) { jamendo(o, "nested"); });
    } else if (p == "OpenSubsonic" && page == 0) {
      fetch(p,
            subUrl(kind == "artist" ? "getArtist" : "getAlbum", {{"id", id}}),
            [this, kind](auto o) {
              auto response = o["subsonic-response"].toObject();
              if (response["status"] != "ok") {
                message("OpenSubsonic", "Acesso recusado pela biblioteca");
                return;
              }
              auto entity =
                  response[kind == "artist" ? "artist" : "album"].toObject();
              QJsonObject result;
              result[kind == "artist" ? "album" : "song"] =
                  entity[kind == "artist" ? "album" : "song"];
              subsonic(
                  {{"subsonic-response",
                    QJsonObject{{"status", "ok"}, {"searchResult3", result}}}});
            });
    }
    return;
  }
  auto enabled = [this](const QString &p) {
    return m_provider == "all" || m_provider == p ||
           (m_provider == "extras" && p != "MusicBrainz");
  };
  if (enabled("Audius"))
    for (const auto &kind : {"track", "artist", "album"}) {
      QString endpoint = QString(kind) == "track"    ? "tracks"
                         : QString(kind) == "artist" ? "users"
                                                     : "playlists";
      fetch("Audius",
            api("https://api.audius.co/v1/" + endpoint + "/search",
                {{"query", m_query}, {"limit", PageSize}, {"offset", offset}}),
            [this, kind](auto o) { audius(o, kind); });
    }
  if (enabled("MusicBrainz"))
    fetch("MusicBrainz",
          api("https://musicbrainz.org/ws/2/artist",
              {{"query", "artist:" + quote(m_query)},
               {"fmt", "json"},
               {"limit", PageSize},
               {"offset", offset}}),
          [this](auto o) { musicbrainz(o, "artist"); });
  // All-sources uses the wider audio archive once; separate filters retain
  // the curated collection and etree views without duplicate HTTP requests.
  if (m_provider == "all" || m_provider == "extras" || archiveProvider(m_provider)) {
    const QString p = archiveProvider(m_provider) ? m_provider : "Internet Archive";
    const QString scope = p == "Live Music Archive" ? "collection:etree AND mediatype:etree" :
                          p == "Aadam Jacobs" ? "collection:aadamjacobs AND mediatype:audio" :
                          "(mediatype:audio OR mediatype:etree)";
    m_liveStatus = "Consultando shows, demos e gravações no Internet Archive…";
    fetch(p,
          api("https://archive.org/advancedsearch.php",
              {{"q", scope + " AND (creator:" +
                         quote(m_query) + " OR title:" + quote(m_query) + ")"},
               {"output", "json"},
               {"rows", PageSize},
               {"page", page + 1},
               {"sort[]", "downloads desc"},
               {"fl[]", "identifier,title,creator,date,mediatype"}}),
          [this, p](auto o) { archive(o, p); });
  }
  if (enabled("Jamendo")) {
    if (m_jamendo.isEmpty())
      message("Jamendo", "Configure um client ID próprio ativo em Conexões.");
    else
      for (const auto &kind : {"track", "artist", "album"}) {
        QString plural = QString(kind) + "s/";
        fetch("Jamendo",
              api("https://api.jamendo.com/v3.0/" + plural,
                  {{"client_id", m_jamendo},
                   {"format", "json"},
                   {"namesearch", m_query},
                   {"limit", PageSize},
                   {"offset", offset},
                   {"audioformat", "mp32"},
                   {"type", "single albumtrack"}}),
              [this, kind](auto o) { jamendo(o, kind); });
      }
  }
  if (enabled("OpenSubsonic")) {
    if (m_server.isEmpty())
      message("OpenSubsonic",
              "Configure sua biblioteca ou coleção Bandcamp em Conexões.");
    else
      fetch("OpenSubsonic",
            subUrl("search3", {{"query", m_query},
                               {"artistCount", PageSize},
                               {"albumCount", PageSize},
                               {"songCount", PageSize},
                               {"artistOffset", offset},
                               {"albumOffset", offset},
                               {"songOffset", offset}}),
            [this](auto o) { subsonic(o); });
  }
}
void CatalogService::archive(const QJsonObject &o, const QString &provider) {
  if (!o["response"].isObject()) {
    m_liveStatus = "A fonte de shows retornou uma resposta inválida.";
    message(provider, m_liveStatus);
    return;
  }
  const auto response = o["response"].toObject();
  const int total = response["numFound"].toInt();
  m_lmaHasMore = total > (m_page + 1) * PageSize;
  int exact = 0;
  for (auto v : response["docs"].toArray()) {
    auto d = v.toObject();
    if ((d["mediatype"] != "etree" && d["mediatype"] != "audio") || yes(d["is_dark"]) || yes(d["access-restricted-item"]))
      continue; // Collections are not concerts.
    auto i = item(provider, d["identifier"].toString(), recordingKind(d),
                  d["title"].toString(), text(d["creator"]));
    const bool sameArtist =
        normalized(i["artist"].toString()) == normalized(m_query);
    if (sameArtist)
      ++exact;
    i["curated"] = d["identifier"].toString().startsWith("ajc");
    i["related"] = !sameArtist;
    i["detail"] = (sameArtist ? recordingLabel(i["kind"].toString()) + " • "
                              : QString("Relacionado / tributo / menção • ")) +
                  d["date"].toString().left(10);
    if (i["curated"].toBool()) i["detail"] = "Coleção Aadam Jacobs • " + i["detail"].toString();
    i["image"] = "https://archive.org/services/img/" + d["identifier"].toString();
    i["url"] = "https://archive.org/details/" + d["identifier"].toString();
    append(i);
  }
  m_liveStatus =
      total > 0 ? QString("Shows / demos / gravações: %1 registros; %2 nesta página com "
                          "nome exato do artista. Confira tributos e menções.")
                      .arg(total)
                      .arg(exact)
                : "Nenhum show / demo / gravação encontrado nesta fonte. Experimente Internet Archive ou Aadam Jacobs.";
}
void CatalogService::archiveFiles(const QJsonObject &o, const QString &id, const QString &provider) {
  const auto meta = o["metadata"].toObject();
  if (yes(o["is_dark"]) || yes(meta["access-restricted-item"]) || yes(meta["is_dark"])) { message(provider, "Item restrito"); return; }
  // One representation per original track. Prefer small MP3 derivatives for
  // mobile buffering, then Ogg, then FLAC when the item has no lossy copy.
  QMap<QString, QJsonObject> chosen;
  auto priority = [](QString f) { f = f.toLower(); return f.contains("mp3") ? 3 : f.contains("ogg") ? 2 : f.contains("flac") ? 1 : 0; };
  for (auto v : o["files"].toArray()) {
    auto f = v.toObject(); const auto name = f["name"].toString();
    if (!QRegularExpression("\\.(mp3|ogg|oga|flac)$",QRegularExpression::CaseInsensitiveOption).match(name).hasMatch() || yes(f["private"]) || name.isEmpty() || name.contains("..") || name.startsWith('/') || name.contains('\\') || priority(f["format"].toString()) == 0) continue;
    QString original = f["original"].toString(name);
    original = original.left(original.lastIndexOf('.') > 0 ? original.lastIndexOf('.') : original.size());
    if (!chosen.contains(original) || priority(f["format"].toString()) > priority(chosen[original]["format"].toString())) chosen[original] = f;
  }
  auto files = chosen.values(); QCollator collator; collator.setNumericMode(true);
  std::stable_sort(files.begin(), files.end(), [&collator](auto a, auto b) { return collator.compare(a["name"].toString(), b["name"].toString()) < 0; });
  for (auto f : files) {
    const auto name = f["name"].toString();
    auto i = item(provider, id + "/" + name, "track", f["title"].toString(name), text(meta["creator"]));
    i["album"] = meta["title"].toString(); i["image"] = "https://archive.org/services/img/" + id;
    i["stream"] = QString("https://archive.org/download/%1/%2").arg(QString::fromLatin1(QUrl::toPercentEncoding(id)), QString::fromLatin1(QUrl::toPercentEncoding(name)));
    i["url"] = "https://archive.org/details/" + id; i["playable"] = true;
    i["detail"] = recordingLabel(recordingKind(meta)) + " • " + meta["date"].toString().left(10) + " • " + f["format"].toString();
    bool ok = false; double seconds = f["length"].toVariant().toDouble(&ok);
    if (ok && seconds > 0 && seconds < 86400) i["durationMs"] = qRound(seconds * 1000);
    append(i);
  }
  if (files.isEmpty()) message(provider, "Sem MP3, Ogg ou FLAC público neste item. Consulte a página da gravação.");
}
void CatalogService::audius(const QJsonObject &o, const QString &kind) {
  QJsonArray entries = o["data"].toArray();
  if (kind == "track" && o["data"].isArray() && entries.size() == 1 &&
      entries.first().toObject().contains("tracks"))
    entries = entries.first().toObject()["tracks"].toArray();
  for (auto v : entries) {
    auto t = v.toObject();
    auto user = t["user"].toObject();
    auto i = item("Audius", text(t["id"]), kind,
                  kind == "artist"  ? t["name"].toString()
                  : kind == "album" ? t["playlist_name"].toString()
                                    : t["title"].toString(),
                  user["name"].toString());
    if (kind == "album") {
      i["kind"] = t["is_album"].toBool() ? "album" : "playlist";
    }
    i["image"] =
        cover(t[kind == "artist" ? "profile_picture" : "artwork"].toObject());
    i["verified"] = t["is_verified"].toBool();
    i["durationMs"] = t["duration"].toInt() * 1000;
    i["genre"] = t["genre"].toString();
    i["playable"] = kind == "track" && t["is_streamable"].toBool() &&
                    !t["is_stream_gated"].toBool() && !t["is_delete"].toBool();
    i["detail"] =
        kind == "track"
            ? (i["playable"].toBool() ? "Reprodução completa • acesso público"
                                      : "Acesso restrito / indisponível")
        : kind == "artist" ? "Artista / perfil • verifique a identidade"
                           : "Álbum / playlist";
    QString permalink = t["permalink"].toString();
    if (permalink.startsWith("/"))
      permalink = "https://audius.co" + permalink;
    i["url"] = permalink;
    append(i);
  }
}
void CatalogService::musicbrainz(const QJsonObject &o, const QString &kind) {
  QJsonArray entries = o[kind == "artist"    ? "artists"
                         : kind == "album"   ? "release-groups"
                         : kind == "release" ? "releases"
                                             : "unused"]
                           .toArray();
  const QString countKey = kind == "artist" ? "count" : "release-group-count";
  m_mbHasMore = (kind == "artist" || kind == "album") &&
                o.value(countKey).toInt(m_page * PageSize + entries.size()) >
                    (m_page + 1) * PageSize;
  if (kind == "track")
    for (auto media : o["media"].toArray())
      for (auto track : media.toObject()["tracks"].toArray())
        entries.append(track.toObject()["recording"]);
  for (auto v : entries) {
    auto t = v.toObject();
    QString id = t["id"].toString();
    auto i = item("MusicBrainz", id, kind,
                  t[kind == "artist" ? "name" : "title"].toString(),
                  routeArtist(m_route));
    i["searchScore"] = t["score"].toInt();
    i["detail"] =
        QString("Somente discografia • %1 • %2")
            .arg(t["disambiguation"].toString(),
                 t["first-release-date"].toString(t["date"].toString()));
    if (kind == "artist")
      i["detail"] =
          QString("Catálogo MusicBrainz • ") +
          (t["type"].toString() == "Group" ? "Banda / grupo" : "Artista") +
          " • " + t["country"].toString() + " • " +
          t["disambiguation"].toString();
    i["url"] = "https://musicbrainz.org/" +
               QString(kind == "album"   ? "release-group"
                       : kind == "track" ? "recording"
                                         : kind) +
               "/" + id;
    append(i);
  }
}
void CatalogService::jamendo(const QJsonObject &o, const QString &kind) {
  if (o["headers"].toObject()["status"] != "success") {
    message("Jamendo",
            "Credencial recusada ou API indisponível. Verifique o client ID.");
    return;
  }
  for (auto v : o["results"].toArray()) {
    auto t = v.toObject();
    if (kind == "nested") {
      QJsonObject nested{{"headers", QJsonObject{{"status", "success"}}},
                         {"results", t["tracks"]}};
      jamendo(nested, "track");
      continue;
    }
    auto i = item("Jamendo", text(t["id"]), kind, t["name"].toString(),
                  t["artist_name"].toString(routeArtist(m_route)));
    i["image"] = t["image"].toString(t["album_image"].toString());
    i["stream"] = t["audio"].toString();
    i["playable"] = kind == "track" && publicUrl(QUrl(i["stream"].toString()));
    i["url"] = t["shareurl"].toString();
    i["detail"] = "Jamendo • " + t["album_name"].toString();
    i["durationMs"] = t["duration"].toInt() * 1000;
    i["genre"] = t["genre"].toString();
    append(i);
  }
}
QUrl CatalogService::subUrl(const QString &method,
                            const QVariantMap &args) const {
  QString salt = QUuid::createUuid().toString(QUuid::Id128);
  QVariantMap values = args;
  values["u"] = m_user;
  values["s"] = salt;
  values["t"] =
      QString::fromLatin1(QCryptographicHash::hash((m_password + salt).toUtf8(),
                                                   QCryptographicHash::Md5)
                              .toHex());
  values["v"] = "1.16.1";
  values["c"] = "IndieTonal";
  values["f"] = "json";
  return api(m_server + "/rest/" + method + ".view", values);
}
void CatalogService::subsonic(const QJsonObject &o) {
  auto response = o["subsonic-response"].toObject();
  if (response["status"] != "ok") {
    message("OpenSubsonic", "Biblioteca recusou a consulta; confira "
                            "credenciais e compatibilidade.");
    return;
  }
  auto result = response["searchResult3"].toObject();
  for (const auto &key : {"artist", "album", "song"})
    for (auto v : result[key].toArray()) {
      auto t = v.toObject();
      QString kind = QString(key) == "song" ? "track" : QString(key);
      auto i = item("OpenSubsonic", text(t["id"]), kind,
                    t[kind == "track" ? "title" : "name"].toString(),
                    t["artist"].toString());
      i["playable"] = kind == "track";
      i["server"] = m_server;
      i["account"] = m_user;
      i["durationMs"] = t["duration"].toInt() * 1000;
    i["genre"] = t["genre"].toString();
      i["detail"] = "Sua biblioteca • " + t["album"].toString();
      append(i);
    }
}
void CatalogService::open(const QVariantMap &i) {
  reloadConnections();
  if (i.value("provider") == "OpenSubsonic" &&
      (i.value("server").toString() != m_server ||
       i.value("account").toString() != m_user)) {
    message("OpenSubsonic", "A conexão mudou. Faça a busca novamente.");
    return;
  }
  if (i.value("kind") == "track")
    return;
  m_route = i;
  reset(i.value("title").toString());
  list(0);
}
QVariantMap CatalogService::configuration() const {
  return {{"jamendoClientId", m_jamendo},
          {"server", m_server},
          {"user", m_user},
          {"hasPassword", !m_password.isEmpty()}};
}
bool CatalogService::configure(const QVariantMap &v) {
  QString server = v.value("server").toString().trimmed();
  while (server.endsWith("/"))
    server.chop(1);
  if (!server.isEmpty() &&
      (!publicUrl(QUrl(server)) || QUrl(server).hasQuery())) {
    message("Conexões",
            "Use um endereço HTTPS sem usuário, senha ou parâmetros na URL.");
    return false;
  }
  QString user = v.value("user").toString().trimmed(),
          password = v.value("password").toString();
  if (password.isEmpty() && server == m_server && user == m_user)
    password = m_password;
  if (server.isEmpty()) {
    user.clear();
    password.clear();
  }
  if (!server.isEmpty() && (user.isEmpty() || password.isEmpty())) {
    message("Conexões", "Informe usuário e senha da biblioteca");
    return false;
  }
  if (!TokenStore::writeSecret(
          "IndieTonal/OpenSubsonic",
          QJsonDocument(QJsonObject{{"server", server},
                                    {"user", user},
                                    {"password", password}})
              .toJson(QJsonDocument::Compact),
          "IndieTonal", "Biblioteca independente do Spotify")) {
    message("Conexões",
            "Não foi possível guardar a conexão no cofre do sistema");
    return false;
  }
  m_server = server;
  m_user = user;
  m_password = password;
  m_jamendo = v.value("jamendoClientId").toString().trimmed();
  QSettings("Tonal", "IndieTonal").setValue("jamendoClientId", m_jamendo);
  emit configurationChanged();
  return true;
}
void CatalogService::clearLibraryCredentials() {
  if (TokenStore::deleteSecret("IndieTonal/OpenSubsonic")) {
    m_server.clear();
    m_user.clear();
    m_password.clear();
    emit configurationChanged();
  } else {
    message("Conexões", "Não foi possível remover a credencial do cofre.");
  }
}
void CatalogService::resolve(const QVariantMap &i) {
  reloadConnections();
  if (!i.value("playable").toBool()) {
    message(i.value("provider").toString(),
            "Este item não oferece reprodução direta");
    return;
  }
  QString provider = i.value("provider").toString();
  if (provider == "OpenSubsonic" &&
      (m_server.isEmpty() || i.value("server").toString() != m_server ||
       i.value("account").toString() != m_user)) {
    message(provider,
            "A conexão mudou ou foi removida. Faça a busca novamente.");
    return;
  }
  QUrl url;
  if (provider == "Audius")
    url = api("https://api.audius.co/v1/tracks/" + i.value("id").toString() +
              "/stream");
  else if (provider == "OpenSubsonic")
    url = subUrl("stream", {{"id", i.value("id")}, {"format", "raw"}});
  else if(provider=="Jamendo" && i.value("stream").toString().isEmpty()) {
    if(m_jamendo.isEmpty()){message(provider,"Configure um client ID ativo em Conexões.");return;}
    fetch(provider,api("https://api.jamendo.com/v3.0/tracks/",{{"client_id",m_jamendo},{"id",i["id"]},{"format","json"},{"audioformat","mp32"}}),[this,i](auto o){auto all=o["results"].toArray();if(all.isEmpty()){message("Jamendo","Faixa indisponível nesta conta.");return;}auto t=i;t["stream"]=all.first().toObject()["audio"].toString();if(!t["stream"].toString().isEmpty())resolve(t);});return;
  } else
    url = QUrl(i.value("stream").toString());
  if (publicUrl(url))
    emit playable(url, i.value("title").toString() + " • " +
                           i.value("artist").toString());
}
