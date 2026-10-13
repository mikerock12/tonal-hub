#pragma once

#include <QJSValue>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QVariantList>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;

// API do setlist.fm (api.setlist.fm/rest/1.0): busca os shows de uma banda para virar playlist. Cada pessoa usa a
// própria API key (grátis, para uso não comercial), guardada no Gerenciador de Credenciais como "Tonal/setlist.fm":
// o código é público, então não dá para embutir uma key no app.
class SetlistFm : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY apiKeyChanged)

public:
    SetlistFm(QNetworkAccessManager *nam, QObject *parent = nullptr);

    void setJsEngine(QJSEngine *engine) { m_engine = engine; }
    bool hasApiKey() const { return !m_apiKey.isEmpty(); }

    // Confere a key com uma busca de teste e só guarda se o setlist.fm aceitar. callback(erro): vazio = deu certo.
    Q_INVOKABLE void setApiKey(const QString &key, const QJSValue &callback);
    Q_INVOKABLE void forgetApiKey();

    // Shows de uma banda, do mais recente para o mais antigo (a 1ª página do setlist.fm, até 20), já sem as faixas
    // de fita (tape) e sem os shows sem músicas. callback(erro, setlists), cada setlist como
    // { id, artist, eventDate (dd-MM-yyyy), venue, city, country, tour, songs: [nomes], url }.
    Q_INVOKABLE void searchSetlists(const QString &artist, const QJSValue &callback);

signals:
    void apiKeyChanged();

private:
    QNetworkReply *get(const QString &path, const QList<std::pair<QString, QString>> &query, const QString &key);
    static QString errorText(int status, QNetworkReply *reply);
    static QVariantList flatten(const QJsonArray &setlists);
    void storeApiKey(const QString &key);

    QNetworkAccessManager *m_nam;
    QPointer<QJSEngine> m_engine;
    QString m_apiKey;
};
