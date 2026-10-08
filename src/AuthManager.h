#pragma once

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QTimer>

class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

// Login OAuth 2.0 "Authorization Code + PKCE" na Web API do Spotify.
// Não usa client secret: só o Client ID do seu app no Developer Dashboard.
// O redirect é capturado por um mini servidor HTTP em 127.0.0.1.
class AuthManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool authenticated READ isAuthenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
    Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
    Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit AuthManager(QNetworkAccessManager *nam, QObject *parent = nullptr);

    static constexpr quint16 kCallbackPort = 8898;

    bool isAuthenticated() const { return !m_refreshToken.isEmpty(); }
    bool isBusy() const { return m_busy; }
    QString clientId() const { return m_clientId; }
    void setClientId(const QString &id);
    QString redirectUri() const;
    QString lastError() const { return m_lastError; }

    QString accessToken() const { return m_accessToken; }
    bool hasValidAccessToken() const;

    Q_INVOKABLE void login();
    Q_INVOKABLE void logout();

    // Renova o access token. Os chamadores ficam aguardando o sinal tokenRefreshed().
    void refresh();
    bool isRefreshing() const { return m_refreshing; }

signals:
    void authenticatedChanged();
    void busyChanged();
    void clientIdChanged();
    void lastErrorChanged();
    void tokenRefreshed(bool ok);

private:
    void setBusy(bool b);
    void setError(const QString &e);
    void startCallbackServer();
    void stopCallbackServer();
    void handleCallback(const QString &requestPath, QTcpSocket *socket);
    void exchangeCode(const QString &code);
    void requestToken(const QByteArray &formBody, bool isRefresh);
    void loadTokens();
    void saveTokens();
    void scheduleRefresh();

    QNetworkAccessManager *m_nam;
    QTcpServer *m_server = nullptr;
    QTimer m_refreshTimer;

    QString m_clientId;
    QString m_accessToken;
    QString m_refreshToken;
    QDateTime m_expiresAt;

    QString m_codeVerifier;
    QString m_state;
    QString m_lastError;
    bool m_busy = false;
    bool m_refreshing = false;
};
