#include "SpotifyApi.h"
#include "AuthManager.h"
#include "NetworkDeadline.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <utility>
#include <limits>

namespace {
const char *kApiBase = "https://api.spotify.com/v1/";

// Erros sempre aparecem; cada chamada com status só com
// QT_LOGGING_RULES="tonal.api.debug=true".
Q_LOGGING_CATEGORY(lcApi, "tonal.api", QtInfoMsg)

QVariant unwrapJs(const QVariant &v) {
  if (v.metaType() == QMetaType::fromType<QJSValue>())
    return v.value<QJSValue>().toVariant();
  return v;
}

QString queryValue(const QVariant &raw) {
  const QVariant v = unwrapJs(raw);
  if (v.metaType().id() == QMetaType::QVariantList ||
      v.metaType().id() == QMetaType::QStringList) {
    QStringList parts;
    for (const QVariant &item : v.toList())
      parts << item.toString();
    return parts.join(QLatin1Char(','));
  }
  if (v.metaType().id() == QMetaType::Bool)
    return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
  return v.toString();
}
} // namespace

SpotifyApi::SpotifyApi(QNetworkAccessManager *nam, AuthManager *auth,
                       QObject *parent)
    : QObject(parent), m_nam(nam), m_auth(auth) {
  connect(m_auth, &AuthManager::tokenRefreshed, this,
          &SpotifyApi::flushWaiting);
  connect(m_auth, &AuthManager::authenticatedChanged, this,
          [this] { ++m_catalogEpoch; m_catalogPages.clear(); });
}

QUrl SpotifyApi::buildUrl(const QString &path, const QVariantMap &query) const {
  QUrl url;
  if (path.startsWith(QLatin1String("https://"))) {
    url = QUrl(path);
  } else {
    QString p = path;
    while (p.startsWith(QLatin1Char('/')))
      p.remove(0, 1);
    url = QUrl(QString::fromLatin1(kApiBase) + p);
  }
  if (!query.isEmpty()) {
    QUrlQuery q(url);
    for (auto it = query.constBegin(); it != query.constEnd(); ++it) {
      const QVariant v = unwrapJs(it.value());
      if (!v.isValid() || v.isNull())
        continue;
      q.removeAllQueryItems(it.key());
      q.addQueryItem(it.key(), QString::fromLatin1(
                                   QUrl::toPercentEncoding(queryValue(v))));
    }
    url.setQuery(q.query(QUrl::FullyEncoded), QUrl::StrictMode);
  }
  return url;
}

void SpotifyApi::get(const QString &path, const QVariantMap &query,
                     Callback cb) {
  enqueue({"GET", buildUrl(path, query), {}, std::move(cb)});
}

void SpotifyApi::put(const QString &path, const QVariantMap &query,
                     const QJsonDocument &body, Callback cb,
                     std::function<bool()> isCurrent, bool notifyCancellation) {
  Pending pending{"PUT", buildUrl(path, query),
                  body.isNull() ? QByteArray()
                                : body.toJson(QJsonDocument::Compact),
                  std::move(cb)};
  pending.isCurrent = std::move(isCurrent);
  pending.notifyCancellation = notifyCancellation;
  enqueue(std::move(pending));
}

void SpotifyApi::post(const QString &path, const QVariantMap &query,
                      const QJsonDocument &body, Callback cb) {
  enqueue({"POST", buildUrl(path, query),
           body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact),
           std::move(cb)});
}

void SpotifyApi::del(const QString &path, const QVariantMap &query,
                     const QJsonDocument &body, Callback cb) {
  enqueue({"DELETE", buildUrl(path, query),
           body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact),
           std::move(cb)});
}

void SpotifyApi::call(const QString &method, const QString &path,
                      const QVariantMap &query, const QVariant &body,
                      const QJSValue &callback) {
  QJsonDocument doc;
  const QVariant b = unwrapJs(body);
  if (b.isValid() && !b.isNull())
    doc = QJsonDocument::fromVariant(b);

  QJSValue cbCopy = callback;
  QPointer<QJSEngine> engine = m_engine;
  Callback cb = [cbCopy, engine](int status,
                                 const QJsonDocument &resp) mutable {
    if (!cbCopy.isCallable() || !engine)
      return;
    QJSValue data;
    if (resp.isObject())
      data = engine->toScriptValue(resp.object());
    else if (resp.isArray())
      data = engine->toScriptValue(resp.array());
    else
      data = QJSValue(QJSValue::NullValue);
    const QJSValue result = cbCopy.call({QJSValue(status), data});
    if (result.isError())
      qWarning("Erro no callback QML: %s", qPrintable(result.toString()));
  };

  const QByteArray verb = method.toUpper().toLatin1();
  const QByteArray payload =
      doc.isNull() ? QByteArray() : doc.toJson(QJsonDocument::Compact);
  enqueue({verb, buildUrl(path, query), payload, std::move(cb)});
}
void SpotifyApi::raw(const QByteArray &method,const QString &path,const QVariantMap &query,
                     const QByteArray &body,const QByteArray &contentType,Callback cb,
                     std::function<bool()> isCurrent) {
  Pending p{method,buildUrl(path,query),body,std::move(cb)};
  p.contentType=contentType;p.retryServer=false;p.isCurrent=std::move(isCurrent);
  enqueue(std::move(p));
}

void SpotifyApi::enqueue(Pending p) {
  if(p.url.scheme()!="https"||p.url.host()!="api.spotify.com"||
     !p.url.userInfo().isEmpty()||p.url.port()!=-1||!p.url.path().startsWith("/v1/")) {
    if(p.cb)p.cb(400,{});return;
  }
  if (!m_auth->isAuthenticated()) {
    if (p.cb)
      p.cb(401, {});
    return;
  }
  if (!m_auth->hasValidAccessToken()) {
    m_waiting.append(std::move(p));
    m_auth->refresh();
    return;
  }
  send(std::move(p));
}

void SpotifyApi::flushWaiting(bool tokenOk) {
  const QList<Pending> waiting = std::exchange(m_waiting, {});
  for (const Pending &p : waiting) {
    if (tokenOk)
      send(p);
    else if (p.cb)
      p.cb(401, {});
  }
}

QString SpotifyApi::cooldownKey(const Pending &p) const {
  auto parts = p.url.path().split('/');
  // Share a service wait across IDs and pagination of the same endpoint,
  // without inventing a global outage from one catalog endpoint's 429.
  if (parts.size() > 3 && QStringList{"artists", "albums", "playlists", "tracks",
      "shows", "episodes", "audiobooks", "chapters", "users"}.contains(parts[2]))
    parts[3] = ":id";
  const QString key = QString::fromLatin1(p.verb) + " " + parts.join('/');
  // Bound memory without evicting an active service wait. Normal app routes
  // fit well below this cap; excess unknown routes share an overflow bucket.
  if (!m_cooldowns.contains(key) && (m_cooldowns.contains("overflow") || m_cooldowns.size() >= 64))
    return "overflow";
  return key;
}

void SpotifyApi::expireCooldowns() {
  for (auto it = m_cooldowns.begin(); it != m_cooldowns.end();)
    if (it.value().hasExpired()) it = m_cooldowns.erase(it); else ++it;
  const bool active = !m_cooldowns.isEmpty();
  if (active != m_rateLimited) { m_rateLimited = active; emit rateLimitedChanged(); }
}

void SpotifyApi::send(Pending p) {
  if (p.isCurrent && !p.isCurrent()) {
    if (p.notifyCancellation && p.cb)
      p.cb(499, {}); // local cancellation, no HTTP request
    return;
  }
  if (!m_auth->isAuthenticated()) {
    if (p.cb) p.cb(401, {});
    return;
  }
  expireCooldowns();
  const bool discography = p.verb == "GET" &&
      p.url.path().startsWith("/v1/artists/") && p.url.path().endsWith("/albums");
  if (discography) {
    for (const auto &page : m_catalogPages) {
      if (page.url == p.url && !page.expires.hasExpired()) {
        if (p.cb) p.cb(200, page.doc);
        return;
      }
    }
  }
  const auto limit = m_cooldowns.constFind(cooldownKey(p));
  const qint64 remaining = limit == m_cooldowns.cend() ? 0 : limit.value().remainingTime();
  if (remaining > 0) {
    // A scheduled retry can wake a fraction before the deadline; wait out the
    // remainder instead of turning that retry into a terminal local 429.
    if ((p.verb != "GET" || p.attempts > 0) && remaining <= 30000 && p.attempts < 3) {
      QTimer::singleShot(int(remaining), Qt::PreciseTimer, this,
                        [this, p]() mutable { enqueue(std::move(p)); });
      return;
    }
    const qint64 seconds = (remaining + 999) / 1000;
    if (p.cb) p.cb(429, QJsonDocument(QJsonObject{
        {"retry_after", seconds}, {"rate_limit_source", "local"}, {"error", QJsonObject{{"message",
          QStringLiteral("O Spotify limitou as consultas. Aguarde %1 s para tentar novamente.").arg(seconds)}}}}));
    return;
  }
  QNetworkRequest req(p.url);
  req.setRawHeader("Authorization",
                   "Bearer " + m_auth->accessToken().toLatin1());
  req.setRawHeader("Accept", "application/json");
  if (!p.body.isEmpty())
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QString::fromLatin1(p.contentType));
  req.setTransferTimeout(20000);

  QNetworkReply *reply = nullptr;
  if (p.verb == "GET")
    reply = m_nam->get(req);
  else
    reply = m_nam->sendCustomRequest(req, p.verb, p.body);
  setReplyDeadline(reply, m_requestDeadlineMs);

  p.attempts++;
  const int catalogEpoch = m_catalogEpoch;
  connect(reply, &QNetworkReply::finished, this, [this, reply, p, catalogEpoch]() mutable {
    reply->deleteLater();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    const bool cancelled = p.notifyCancellation && p.isCurrent && !p.isCurrent();
    if (cancelled && status != 429) {
      if (p.cb)
        p.cb(499, {});
      return; // an obsolete volume must not enter token/backoff retry queues
    }
    if (status == 401 && p.attempts < 2) {
      // token expirou antes do previsto: renova e repete
      m_waiting.append(p);
      m_auth->refresh();
      return;
    }
    if (status == 429) {
      bool valid = false;
      const qint64 header = reply->rawHeader("Retry-After").toLongLong(&valid);
      // Spotify may ask for hours. Never truncate the wait to 60 seconds.
      const qint64 retryAfter = valid && header > 0 ? header : 30;
      const qint64 delay = retryAfter > std::numeric_limits<qint64>::max() / 1000
          ? std::numeric_limits<qint64>::max() : retryAfter * 1000;
      expireCooldowns();
      const QString key = cooldownKey(p);
      const auto previous = m_cooldowns.constFind(key);
      if (previous == m_cooldowns.cend() || delay > previous.value().remainingTime())
        m_cooldowns.insert(key, QDeadlineTimer(delay, Qt::PreciseTimer));
      if (!m_rateLimited) {
        m_rateLimited = true;
        emit rateLimitedChanged();
      }
      if (cancelled) { if (p.cb) p.cb(499, {}); return; }
      if (retryAfter <= 30 && p.attempts < 3) {
        QTimer::singleShot(int(retryAfter * 1000), Qt::PreciseTimer, this,
                         [this, p]() mutable { enqueue(std::move(p)); });
      } else if (p.cb) {
        p.cb(429, QJsonDocument(QJsonObject{{"retry_after", retryAfter}, {"rate_limit_source", "spotify"},
          {"error", QJsonObject{{"message", QStringLiteral("O Spotify limitou as consultas. Aguarde %1 s para tentar novamente.").arg(retryAfter)}}}}));
      }
      return;
    }
    if (p.retryServer && (status == 502 || status == 503) && p.attempts < 3) {
      QTimer::singleShot(800 * p.attempts, this,
                         [this, p]() mutable { send(std::move(p)); });
      return;
    }

    if (status == 401)
      emit unauthorized();
    const QString target =
        p.url.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority);
    if (status >= 400 || status == 0) {
      const QByteArray detail =
          data.isEmpty() ? reply->errorString().toUtf8() : data.left(300);
      qCWarning(lcApi, "%s %s -> %d %s", p.verb.constData(), qPrintable(target),
                status, detail.constData());
    } else {
      qCDebug(lcApi, "%s %s -> %d", p.verb.constData(), qPrintable(target),
              status);
    }

    const auto doc = data.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(data);
    if (status == 200 && catalogEpoch == m_catalogEpoch && p.verb == "GET" && p.url.path().startsWith("/v1/artists/") &&
        p.url.path().endsWith("/albums") && data.size() <= 131072 && doc.object()["items"].isArray()) {
      m_catalogPages.removeIf([&](const CatalogPage &page) { return page.url == p.url || page.expires.hasExpired(); });
      if (m_catalogPages.size() >= 32) m_catalogPages.removeFirst();
      m_catalogPages.append({p.url, doc, QDeadlineTimer(600000)});
    }
    if (p.cb) p.cb(status, doc);
  });
}
