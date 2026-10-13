#include "SetlistFm.h"

#include <QCoreApplication>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

#include "TokenStore.h"

Q_LOGGING_CATEGORY(lcSetlist, "tonal.setlistfm", QtInfoMsg)

namespace {

const QString kSecretName = QStringLiteral("setlist.fm");
const QString kSettingsKey = QStringLiteral("setlistfm/apiKey"); // só sem o cofre do Windows

} // namespace

SetlistFm::SetlistFm(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam)
{
    if (const auto stored = TokenStore::readSecret(kSecretName))
        m_apiKey = QString::fromUtf8(*stored);
    else
        m_apiKey = QSettings().value(kSettingsKey).toString();
}

QNetworkReply *SetlistFm::get(const QString &path, const QList<std::pair<QString, QString>> &query, const QString &key)
{
    QUrl url(QStringLiteral("https://api.setlist.fm/rest/1.0") + path);
    QUrlQuery q;
    for (const auto &[name, value] : query)
        q.addQueryItem(name, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    url.setQuery(q.query(QUrl::FullyEncoded), QUrl::StrictMode);

    QNetworkRequest req(url);
    req.setRawHeader("x-api-key", key.toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Accept-Language", "pt"); // cidades e países em português
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Tonal/%1 (https://github.com/natheus/tonal-player)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(15000);
    return m_nam->get(req);
}

QString SetlistFm::errorText(int status, QNetworkReply *reply)
{
    switch (status) {
    case 401:
    case 403:
        return tr("O setlist.fm recusou a API key. Confira se copiou a key inteira.");
    case 429:
        return tr("O setlist.fm limitou as buscas por um momento. Tente de novo em alguns segundos.");
    case 0:
        return tr("Não consegui falar com o setlist.fm (%1).").arg(reply->errorString());
    default:
        return tr("O setlist.fm respondeu com erro %1.").arg(status);
    }
}

void SetlistFm::setApiKey(const QString &key, const QJSValue &callback)
{
    const QString trimmed = key.trimmed();
    QJSValue cb = callback;
    if (trimmed.isEmpty()) {
        cb.call({ QJSValue(tr("Cole a API key do setlist.fm.")) });
        return;
    }
    // Busca pequena só para ver se a key vale: 200 ou 404 (nada achado) = aceita.
    QNetworkReply *reply = get(QStringLiteral("/search/artists"),
                               { { QStringLiteral("artistName"), QStringLiteral("Radiohead") },
                                 { QStringLiteral("p"), QStringLiteral("1") } },
                               trimmed);
    connect(reply, &QNetworkReply::finished, this, [this, reply, trimmed, cb]() mutable {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCDebug(lcSetlist) << "conferência da key ->" << status;
        if (status != 200 && status != 404) {
            cb.call({ QJSValue(errorText(status, reply)) });
            return;
        }
        storeApiKey(trimmed);
        cb.call({ QJSValue(QString()) });
    });
}

void SetlistFm::forgetApiKey()
{
    storeApiKey(QString());
}

void SetlistFm::storeApiKey(const QString &key)
{
    QSettings settings;
    const bool inVault = key.isEmpty()
        ? TokenStore::deleteSecret(kSecretName)
        : TokenStore::writeSecret(kSecretName, key.toUtf8(), QStringLiteral("setlist.fm API"),
                                  QStringLiteral("API key do setlist.fm usada pelo Tonal"));
    if (inVault || key.isEmpty())
        settings.remove(kSettingsKey);
    else
        settings.setValue(kSettingsKey, key); // sem o cofre: fica no QSettings, como os tokens
    if (key == m_apiKey)
        return;
    m_apiKey = key;
    emit apiKeyChanged();
}

void SetlistFm::searchSetlists(const QString &artist, const QJSValue &callback)
{
    QJSValue cb = callback;
    QPointer<QJSEngine> engine = m_engine;
    const QString name = artist.trimmed();
    if (name.isEmpty() || m_apiKey.isEmpty()) {
        cb.call({ QJSValue(tr("Informe o nome da banda.")), QJSValue(QJSValue::NullValue) });
        return;
    }
    QNetworkReply *reply = get(QStringLiteral("/search/setlists"),
                               { { QStringLiteral("artistName"), name }, { QStringLiteral("p"), QStringLiteral("1") } },
                               m_apiKey);
    connect(reply, &QNetworkReply::finished, this, [reply, cb, engine, name]() mutable {
        reply->deleteLater();
        if (!engine)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qCDebug(lcSetlist).noquote() << "GET /search/setlists" << name << "->" << status;
        QVariantList setlists;
        if (status == 200) {
            const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
            setlists = flatten(obj.value(QStringLiteral("setlist")).toArray());
        } else if (status != 404) { // 404 = nenhum show dessa banda
            qCWarning(lcSetlist) << "setlist.fm:" << status << reply->errorString();
            cb.call({ QJSValue(errorText(status, reply)), QJSValue(QJSValue::NullValue) });
            return;
        }
        const QJSValue result = cb.call({ QJSValue(QString()), engine->toScriptValue(setlists) });
        if (result.isError())
            qCWarning(lcSetlist) << "Erro no callback QML:" << result.toString();
    });
}

QVariantList SetlistFm::flatten(const QJsonArray &setlists)
{
    QVariantList out;
    for (const QJsonValue &value : setlists) {
        const QJsonObject setlist = value.toObject();
        QStringList songs;
        const QJsonArray sets = setlist.value(QStringLiteral("sets")).toObject().value(QStringLiteral("set")).toArray();
        for (const QJsonValue &set : sets) {
            const QJsonArray setSongs = set.toObject().value(QStringLiteral("song")).toArray();
            for (const QJsonValue &song : setSongs) {
                const QJsonObject s = song.toObject();
                // tape = música de fundo (intro, saída), não foi tocada pela banda.
                if (s.value(QStringLiteral("tape")).toBool())
                    continue;
                const QString songName = s.value(QStringLiteral("name")).toString().trimmed();
                if (!songName.isEmpty())
                    songs << songName;
            }
        }
        if (songs.isEmpty())
            continue;

        const QJsonObject venue = setlist.value(QStringLiteral("venue")).toObject();
        const QJsonObject city = venue.value(QStringLiteral("city")).toObject();
        out.append(QVariantMap {
            { QStringLiteral("id"), setlist.value(QStringLiteral("id")).toString() },
            { QStringLiteral("artist"), setlist.value(QStringLiteral("artist")).toObject().value(QStringLiteral("name")).toString() },
            { QStringLiteral("eventDate"), setlist.value(QStringLiteral("eventDate")).toString() },
            { QStringLiteral("venue"), venue.value(QStringLiteral("name")).toString() },
            { QStringLiteral("city"), city.value(QStringLiteral("name")).toString() },
            { QStringLiteral("country"), city.value(QStringLiteral("country")).toObject().value(QStringLiteral("name")).toString() },
            { QStringLiteral("tour"), setlist.value(QStringLiteral("tour")).toObject().value(QStringLiteral("name")).toString() },
            { QStringLiteral("songs"), songs },
            { QStringLiteral("url"), setlist.value(QStringLiteral("url")).toString() },
        });
    }
    return out;
}
