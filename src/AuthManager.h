#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

class QNetworkAccessManager;
class QTcpServer;
class QTcpSocket;

// Login OAuth 2.0 "Authorization Code + PKCE" na Web API do Spotify.
// Não usa client secret: só o Client ID do seu app no Developer Dashboard.
// O redirect é capturado por um mini servidor HTTP em 127.0.0.1.
class AuthManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(
      bool authenticated READ isAuthenticated NOTIFY authenticatedChanged)
  Q_PROPERTY(bool busy READ isBusy NOTIFY busyChanged)
  Q_PROPERTY(
      QString clientId READ clientId WRITE setClientId NOTIFY clientIdChanged)
  Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
  // "" (não conferido) | "checking" | "valid" | "invalid" | "unknown" (sem
  // resposta clara do Spotify)
  Q_PROPERTY(
      QString clientIdStatus READ clientIdStatus NOTIFY clientIdStatusChanged)
  // O login atual permite criar e editar playlists. Quem entrou antes de o
  // Tonal pedir playlist-modify-private precisa autorizar de novo (login() com
  // a sessão aberta).
  Q_PROPERTY(
      bool canEditPlaylists READ canEditPlaylists NOTIFY grantedScopesChanged)

public:
  explicit AuthManager(QNetworkAccessManager *nam, QObject *parent = nullptr);

  static constexpr quint16 kCallbackPort = 8898;

  bool isAuthenticated() const { return !m_refreshToken.isEmpty(); }
  bool isBusy() const { return m_busy; }
  QString clientId() const { return m_clientId; }
  void setClientId(const QString &id);
  QString redirectUri() const;
  QString lastError() const { return m_lastError; }
  QString clientIdStatus() const { return m_clientIdStatus; }
  bool canEditPlaylists() const;

  QString accessToken() const { return m_accessToken; }
  bool hasValidAccessToken() const;

  // Client ID do Spotify: 32 caracteres hexadecimais (o Client Secret tem o
  // mesmo formato).
  Q_INVOKABLE bool isClientIdFormat(const QString &id) const;
  // Pergunta ao Spotify se o Client ID existe, sem abrir o navegador. Resultado
  // em clientIdStatus.
  Q_INVOKABLE void checkClientId(const QString &id);

  Q_INVOKABLE void login();
  Q_INVOKABLE void cancelLogin();
  Q_INVOKABLE void logout();

  // Renova o access token. Os chamadores ficam aguardando o sinal
  // tokenRefreshed().
  void refresh();
  bool isRefreshing() const { return m_refreshing; }

signals:
  void authenticatedChanged();
  void busyChanged();
  void clientIdChanged();
  void lastErrorChanged();
  void clientIdStatusChanged();
  void grantedScopesChanged();
  void tokenRefreshed(bool ok);

private:
  friend class SpotifyRecoveryTests;
  int m_tokenDeadlineMs = 20000;
  void setBusy(bool b);
  void setError(const QString &e);
  void setClientIdStatus(const QString &s);
  void startCallbackServer();
  void stopCallbackServer();
  void handleCallback(const QString &requestPath, QTcpSocket *socket);
  void exchangeCode(const QString &code);
  void requestToken(const QByteArray &formBody, bool isRefresh);
  void loadTokens();
  void saveTokens();
  void scheduleRefresh();
  void setGrantedScopes(const QStringList &scopes);

  QNetworkAccessManager *m_nam;
  QTcpServer *m_server = nullptr;
  QTimer m_refreshTimer;

  QString m_clientId;
  QString m_accessToken;
  QString m_refreshToken;
  QDateTime m_expiresAt;
  QStringList m_grantedScopes; // o que o Spotify concedeu de fato (campo
                               // "scope" do token)

  QString m_codeVerifier;
  QString m_state;
  QString m_lastError;
  QString m_clientIdStatus;
  QString m_checkedClientId;
  int m_checkSerial = 0;
  bool m_busy = false;
  bool m_refreshing = false;
};
