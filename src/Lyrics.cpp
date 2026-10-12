#include "Lyrics.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>

#include "Player.h"

Q_LOGGING_CATEGORY(lcLyrics, "tonal.lyrics", QtInfoMsg)

namespace {

constexpr int kCacheSize = 30;
// Diferença máxima de duração aceita na busca. O LRCLIB guarda várias versões da mesma música (ao vivo,
// remasterizada, edição de rádio) e a letra de outra versão sai fora de sincronia.
constexpr double kMaxDurationDiff = 5.0;

// Tira o que o Spotify acrescenta ao nome e o LRCLIB costuma não ter: " - Remastered 2011", " (feat. X)".
QString searchableName(QString name)
{
    const int dash = int(name.indexOf(QStringLiteral(" - ")));
    if (dash > 0)
        name.truncate(dash);
    static const QRegularExpression feat(QStringLiteral(R"(\s*[\(\[](feat\.?|ft\.?|with)\s[^\)\]]*[\)\]])"),
                                         QRegularExpression::CaseInsensitiveOption);
    return name.remove(feat).trimmed();
}

// LRC: "[mm:ss.xx] texto", às vezes com várias marcas de tempo na mesma linha. Linhas sem marca de tempo
// (as tags [ar:...], [ti:...] e linhas soltas) ficam de fora.
QList<std::pair<int, QString>> parseLrc(const QString &lrc)
{
    static const QRegularExpression stamp(QStringLiteral(R"(^\[(\d{1,3}):(\d{1,2})(?:[.:](\d{1,3}))?\])"));
    QList<std::pair<int, QString>> out;
    const QStringList rows = lrc.split(QLatin1Char('\n'));
    for (const QString &row : rows) {
        QString rest = row.trimmed();
        QList<int> times;
        for (QRegularExpressionMatch m = stamp.match(rest); m.hasMatch(); m = stamp.match(rest)) {
            const QString fraction = m.captured(3);
            int ms = m.captured(1).toInt() * 60000 + m.captured(2).toInt() * 1000;
            if (!fraction.isEmpty())
                ms += fraction.toInt() * (fraction.size() == 1 ? 100 : fraction.size() == 2 ? 10 : 1);
            times << ms;
            rest = rest.mid(m.capturedLength());
        }
        const QString text = rest.trimmed();
        for (int time : std::as_const(times))
            out.append({ time, text });
    }
    std::stable_sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    return out;
}

} // namespace

Lyrics::Lyrics(QNetworkAccessManager *nam, Player *player, QObject *parent)
    : QObject(parent), m_nam(nam), m_player(player)
{
    connect(m_player, &Player::trackChanged, this, [this] {
        if (m_active)
            load();
    });
}

void Lyrics::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    emit activeChanged();
    if (m_active)
        load();
}

int Lyrics::lineAt(int positionMs) const
{
    if (m_status != Synced)
        return -1;
    const auto it = std::upper_bound(m_times.cbegin(), m_times.cend(), positionMs);
    return int(it - m_times.cbegin()) - 1;
}

int Lyrics::lineTime(int index) const
{
    return m_times.value(index, -1);
}

void Lyrics::retry()
{
    if (m_active && m_status == Error)
        load();
}

void Lyrics::load()
{
    const QString uri = m_player->trackUri();
    if (uri == m_trackUri && m_status != Error)
        return; // já mostra (ou já busca) a letra desta faixa
    m_trackUri = uri;
    ++m_serial; // descarta a resposta de uma busca anterior

    Entry entry;
    if (uri.isEmpty()) {
        entry.status = Idle;
        show(entry);
        return;
    }
    const auto cached = m_cache.constFind(uri);
    if (cached != m_cache.constEnd()) {
        show(*cached);
        return;
    }
    if (!uri.startsWith(QLatin1String("spotify:track:"))) {
        show(entry); // episódio de podcast: não tem letra
        return;
    }

    entry.status = Loading;
    show(entry);
    const QVariantList artists = m_player->artists();
    m_queryArtist = artists.isEmpty() ? QString() : artists.first().toMap().value(QStringLiteral("name")).toString();
    m_queryTrack = m_player->trackName();
    m_queryDurationMs = m_player->durationMs();
    // Busca exata primeiro (nome, artista, álbum e duração com ±2 s). Se não achar, handleReply tenta a busca.
    request(QStringLiteral("/api/get"),
            { { QStringLiteral("artist_name"), m_queryArtist },
              { QStringLiteral("track_name"), m_queryTrack },
              { QStringLiteral("album_name"), m_player->albumName() },
              { QStringLiteral("duration"), QString::number(qRound(m_queryDurationMs / 1000.0)) } },
            false);
}

void Lyrics::request(const QString &path, const QList<std::pair<QString, QString>> &query, bool isSearch)
{
    QUrl url(QStringLiteral("https://lrclib.net") + path);
    QUrlQuery q;
    for (const auto &[key, value] : query)
        q.addQueryItem(key, QString::fromLatin1(QUrl::toPercentEncoding(value)));
    url.setQuery(q.query(QUrl::FullyEncoded), QUrl::StrictMode);

    QNetworkRequest req(url);
    // O LRCLIB pede um User-Agent que identifique o app.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Tonal/%1 (https://github.com/natheus/tonal-player)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_nam->get(req);
    const int serial = m_serial;
    connect(reply, &QNetworkReply::finished, this, [this, reply, isSearch, serial] {
        handleReply(reply, isSearch, serial);
    });
}

void Lyrics::handleReply(QNetworkReply *reply, bool isSearch, int serial)
{
    reply->deleteLater();
    if (serial != m_serial)
        return;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    qCDebug(lcLyrics).noquote() << "GET" << reply->url().path() << "->" << status;

    if (!isSearch && status == 404) {
        request(QStringLiteral("/api/search"),
                { { QStringLiteral("track_name"), searchableName(m_queryTrack) },
                  { QStringLiteral("artist_name"), m_queryArtist } },
                true);
        return;
    }
    Entry entry;
    if (status != 200) {
        qCWarning(lcLyrics).noquote() << "LRCLIB:" << status << reply->errorString();
        entry.status = Error; // fora do cache: "Tentar de novo" busca outra vez
        show(entry);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (isSearch) {
        const QJsonObject best = bestMatch(doc.array());
        if (!best.isEmpty())
            entry = entryFrom(best);
    } else {
        entry = entryFrom(doc.object());
    }
    if (!m_cache.contains(m_trackUri)) {
        m_cacheOrder << m_trackUri;
        if (m_cacheOrder.size() > kCacheSize)
            m_cache.remove(m_cacheOrder.takeFirst());
    }
    m_cache.insert(m_trackUri, entry);
    show(entry);
}

Lyrics::Entry Lyrics::entryFrom(const QJsonObject &obj) const
{
    Entry entry;
    if (obj.value(QStringLiteral("instrumental")).toBool()) {
        entry.status = Instrumental;
        return entry;
    }
    const auto synced = parseLrc(obj.value(QStringLiteral("syncedLyrics")).toString());
    if (!synced.isEmpty()) {
        entry.status = Synced;
        for (const auto &[time, text] : synced) {
            // Um ♪ só por trecho instrumental.
            if (text.isEmpty() && !entry.texts.isEmpty() && entry.texts.constLast().isEmpty())
                continue;
            entry.times << time;
            entry.texts << text;
        }
        return entry;
    }
    const QString plain = obj.value(QStringLiteral("plainLyrics")).toString().trimmed();
    if (!plain.isEmpty()) {
        entry.status = Plain;
        const QStringList rows = plain.split(QLatin1Char('\n'));
        for (const QString &row : rows) {
            entry.times << -1;
            entry.texts << row.trimmed();
        }
    }
    return entry;
}

QJsonObject Lyrics::bestMatch(const QJsonArray &results) const
{
    const double duration = m_queryDurationMs / 1000.0;
    QJsonObject best;
    double bestScore = 0;
    for (const QJsonValue &value : results) {
        const QJsonObject obj = value.toObject();
        const double diff = std::abs(obj.value(QStringLiteral("duration")).toDouble() - duration);
        if (diff > kMaxDurationDiff)
            continue;
        // A letra sincronizada vence; entre duas iguais, a duração mais próxima.
        const double score = diff + (obj.value(QStringLiteral("syncedLyrics")).toString().isEmpty() ? 100 : 0);
        if (best.isEmpty() || score < bestScore) {
            best = obj;
            bestScore = score;
        }
    }
    return best;
}

void Lyrics::show(const Entry &entry)
{
    m_status = entry.status;
    m_times = entry.times;
    m_lines.clear();
    m_lines.reserve(entry.texts.size());
    for (qsizetype i = 0; i < entry.texts.size(); ++i) {
        m_lines.append(QVariantMap { { QStringLiteral("time"), entry.times.value(i, -1) },
                                     { QStringLiteral("text"), entry.texts.at(i) } });
    }
    emit changed();
}
