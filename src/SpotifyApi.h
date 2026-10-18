#pragma once

#include <QJSEngine>
#include <QJSValue>
#include <QJsonDocument>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariant>
#include <functional>

class QNetworkAccessManager;
class AuthManager;

// Cliente fino da Spotify Web API (https://api.spotify.com/v1).
// - injeta o Bearer token
// - renova o token automaticamente em 401
// - respeita Retry-After em 429
// - expõe call() para o QML com callback JavaScript
class SpotifyApi : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool rateLimited READ isRateLimited NOTIFY rateLimitedChanged)

public:
  using Callback = std::function<void(int status, const QJsonDocument &doc)>;

  SpotifyApi(QNetworkAccessManager *nam, AuthManager *auth,
             QObject *parent = nullptr);

  void setJsEngine(QJSEngine *engine) { m_engine = engine; }
  bool isRateLimited() const { return m_rateLimited; }

  // path: "/me/player", "me/playlists" ou URL completa (links "next" de
  // paginação)
  void get(const QString &path, const QVariantMap &query, Callback cb);
  void put(const QString &path, const QVariantMap &query,
           const QJsonDocument &body, Callback cb = {},
           std::function<bool()> isCurrent = {});
  void post(const QString &path, const QVariantMap &query,
            const QJsonDocument &body, Callback cb = {});
  void del(const QString &path, const QVariantMap &query,
           const QJsonDocument &body, Callback cb = {});
  // Import writes never retry an uncertain 5xx POST (could duplicate a list).
  void raw(const QByteArray &method,const QString &path,const QVariantMap &query,
           const QByteArray &body,const QByteArray &contentType,Callback cb,
           std::function<bool()> isCurrent = {});

  // Para o QML: App.api.call("GET", "/me/playlists", {limit: 50}, null,
  // function(status, data) {...})
  Q_INVOKABLE void call(const QString &method, const QString &path,
                        const QVariantMap &query = {},
                        const QVariant &body = {},
                        const QJSValue &callback = {});

signals:
  void rateLimitedChanged();
  void unauthorized();

private:
  friend class SpotifyRecoveryTests;
  int m_requestDeadlineMs = 30000;
  struct Pending {
    QByteArray verb;
    QUrl url;
    QByteArray body;
    Callback cb;
    int attempts = 0;
    std::function<bool()> isCurrent;
    QByteArray contentType = "application/json";
    bool retryServer = true;
  };

  QUrl buildUrl(const QString &path, const QVariantMap &query) const;
  void enqueue(Pending p);
  void send(Pending p);
  void flushWaiting(bool tokenOk);

  QNetworkAccessManager *m_nam;
  AuthManager *m_auth;
  QPointer<QJSEngine> m_engine;
  QList<Pending> m_waiting; // aguardando refresh do token
  bool m_rateLimited = false;
};
