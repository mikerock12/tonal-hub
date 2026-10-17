#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>

class Player;
class QNetworkAccessManager;
class QNetworkReply;

// Letra da faixa atual, do LRCLIB (lrclib.net, acervo aberto de letras sincronizadas: a Web API do Spotify não
// tem letras). Só busca enquanto a tela de letra está aberta (active), para não mandar ao LRCLIB o nome de
// cada música tocada. Guarda as últimas letras em memória.
class Lyrics : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(Status status READ status NOTIFY changed)
    // Linhas como { time (ms, -1 na letra sem sincronia), text }. Texto vazio = trecho instrumental.
    Q_PROPERTY(QVariantList lines READ lines NOTIFY changed)

public:
    enum Status { Idle, Loading, Synced, Plain, Instrumental, NotFound, Error };
    Q_ENUM(Status)

    Lyrics(QNetworkAccessManager *nam, Player *player, QObject *parent = nullptr);

    bool active() const { return m_active; }
    void setActive(bool active);
    Status status() const { return m_status; }
    QVariantList lines() const { return m_lines; }

    // Índice da linha que está tocando em positionMs (-1 antes da primeira).
    Q_INVOKABLE int lineAt(int positionMs) const;
    // Início da linha em ms (-1 se não houver).
    Q_INVOKABLE int lineTime(int index) const;
    Q_INVOKABLE void retry();

signals:
    void activeChanged();
    void changed();

private:
    struct Entry
    {
        Status status = NotFound;
        QList<int> times;
        QStringList texts;
    };

    void load();
    void request(const QString &path, const QList<std::pair<QString, QString>> &query, bool isSearch);
    void handleReply(QNetworkReply *reply, bool isSearch, int serial);
    Entry entryFrom(const QJsonObject &obj) const;
    QJsonObject bestMatch(const QJsonArray &results) const;
    void show(const Entry &entry);

    QNetworkAccessManager *m_nam;
    Player *m_player;
    bool m_active = false;
    int m_serial = 0;
    int m_retries = 0;

    // O que foi pedido ao LRCLIB: a busca de reserva usa os mesmos dados.
    QString m_queryArtist;
    QString m_queryTrack;
    int m_queryDurationMs = 0;

    QString m_trackUri;   // faixa das linhas mostradas (ou da busca em andamento)
    Status m_status = Idle;
    QVariantList m_lines;
    QList<int> m_times;

    QHash<QString, Entry> m_cache;
    QStringList m_cacheOrder;
};
