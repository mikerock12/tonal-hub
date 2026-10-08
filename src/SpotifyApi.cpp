#include "SpotifyApi.h"
#include "AuthManager.h"

#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonObject>
#include <utility>

namespace {
const char *kApiBase = "https://api.spotify.com/v1/";

QVariant unwrapJs(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<QJSValue>())
        return v.value<QJSValue>().toVariant();
    return v;
}

QString queryValue(const QVariant &raw)
{
    const QVariant v = unwrapJs(raw);
    if (v.metaType().id() == QMetaType::QVariantList || v.metaType().id() == QMetaType::QStringList) {
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

SpotifyApi::SpotifyApi(QNetworkAccessManager *nam, AuthManager *auth, QObject *parent)
    : QObject(parent), m_nam(nam), m_auth(auth)
{
    connect(m_auth, &AuthManager::tokenRefreshed, this, &SpotifyApi::flushWaiting);
}

QUrl SpotifyApi::buildUrl(const QString &path, const QVariantMap &query) const
{
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
            q.addQueryItem(it.key(), QString::fromLatin1(QUrl::toPercentEncoding(queryValue(v))));
        }
        url.setQuery(q.query(QUrl::FullyEncoded), QUrl::StrictMode);
    }
    return url;
}

void SpotifyApi::get(const QString &path, const QVariantMap &query, Callback cb)
{
    enqueue({"GET", buildUrl(path, query), {}, std::move(cb)});
}

void SpotifyApi::put(const QString &path, const QVariantMap &query, const QJsonDocument &body, Callback cb)
{
    enqueue({"PUT", buildUrl(path, query), body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact), std::move(cb)});
}

void SpotifyApi::post(const QString &path, const QVariantMap &query, const QJsonDocument &body, Callback cb)
{
    enqueue({"POST", buildUrl(path, query), body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact), std::move(cb)});
}

void SpotifyApi::del(const QString &path, const QVariantMap &query, const QJsonDocument &body, Callback cb)
{
    enqueue({"DELETE", buildUrl(path, query), body.isNull() ? QByteArray() : body.toJson(QJsonDocument::Compact), std::move(cb)});
}

void SpotifyApi::call(const QString &method, const QString &path, const QVariantMap &query,
                      const QVariant &body, const QJSValue &callback)
{
    QJsonDocument doc;
    const QVariant b = unwrapJs(body);
    if (b.isValid() && !b.isNull())
        doc = QJsonDocument::fromVariant(b);

    QJSValue cbCopy = callback;
    QPointer<QJSEngine> engine = m_engine;
    Callback cb = [cbCopy, engine](int status, const QJsonDocument &resp) mutable {
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
    const QByteArray payload = doc.isNull() ? QByteArray() : doc.toJson(QJsonDocument::Compact);
    enqueue({verb, buildUrl(path, query), payload, std::move(cb)});
}

void SpotifyApi::enqueue(Pending p)
{
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

void SpotifyApi::flushWaiting(bool tokenOk)
{
    const QList<Pending> waiting = std::exchange(m_waiting, {});
    for (const Pending &p : waiting) {
        if (tokenOk)
            send(p);
        else if (p.cb)
            p.cb(401, {});
    }
}

void SpotifyApi::send(Pending p)
{
    QNetworkRequest req(p.url);
    req.setRawHeader("Authorization", "Bearer " + m_auth->accessToken().toLatin1());
    req.setRawHeader("Accept", "application/json");
    if (!p.body.isEmpty())
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setTransferTimeout(20000);

    QNetworkReply *reply = nullptr;
    if (p.verb == "GET")
        reply = m_nam->get(req);
    else
        reply = m_nam->sendCustomRequest(req, p.verb, p.body);

    p.attempts++;
    connect(reply, &QNetworkReply::finished, this, [this, reply, p]() mutable {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = reply->readAll();

        if (status == 401 && p.attempts < 2) {
            // token expirou antes do previsto: renova e repete
            m_waiting.append(p);
            m_auth->refresh();
            return;
        }
        if (status == 429 && p.attempts < 4) {
            const int retryAfter = qBound(1, reply->rawHeader("Retry-After").toInt(), 60);
            if (!m_rateLimited) {
                m_rateLimited = true;
                emit rateLimitedChanged();
            }
            QTimer::singleShot(retryAfter * 1000, this, [this, p]() mutable { send(std::move(p)); });
            return;
        }
        if ((status == 502 || status == 503) && p.attempts < 3) {
            QTimer::singleShot(800 * p.attempts, this, [this, p]() mutable { send(std::move(p)); });
            return;
        }

        if (m_rateLimited && status != 429) {
            m_rateLimited = false;
            emit rateLimitedChanged();
        }
        if (status == 401)
            emit unauthorized();
        if (status >= 400 || status == 0) {
            qWarning("Spotify API %s %s -> %d %s", p.verb.constData(),
                     qPrintable(p.url.path()), status, data.left(300).constData());
        }

        if (p.cb)
            p.cb(status, data.isEmpty() ? QJsonDocument() : QJsonDocument::fromJson(data));
    });
}
