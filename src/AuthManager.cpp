#include "AuthManager.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include "TokenStore.h"
#include "NetworkDeadline.h"

namespace {

const char *kAuthorizeUrl = "https://accounts.spotify.com/authorize";
const char *kTokenUrl = "https://accounts.spotify.com/api/token";

// Escopos necessários para biblioteca, busca, controle de reprodução e edição de playlists.
const char *kScopes =
    "user-read-private "
    "user-read-playback-state "
    "user-modify-playback-state "
    "user-read-currently-playing "
    "user-read-recently-played "
    "user-top-read "
    "user-library-read "
    "user-library-modify "
    "user-follow-read "
    "user-follow-modify "
    "playlist-modify-public "
    "playlist-modify-private "
    "playlist-read-private "
    "playlist-read-collaborative";

QString randomString(int length)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    const int n = int(sizeof(alphabet) - 1);
    QString out;
    out.reserve(length);
    auto *rng = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(QLatin1Char(alphabet[rng->bounded(n)]));
    return out;
}

QString pkceChallenge(const QString &verifier)
{
    const QByteArray hash = QCryptographicHash::hash(verifier.toLatin1(), QCryptographicHash::Sha256);
    return QString::fromLatin1(
        hash.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QByteArray htmlPage(const QString &title, const QString &message)
{
    const QString body = QStringLiteral(
        "<!doctype html><html><head><meta charset='utf-8'><title>Tonal</title>"
        "<style>body{background:#121212;color:#fff;font-family:Segoe UI,sans-serif;"
        "display:flex;align-items:center;justify-content:center;height:100vh;margin:0}"
        "div{text-align:center}h1{font-size:28px}p{color:#b3b3b3}</style></head>"
        "<body><div><h1>%1</h1><p>%2</p></div></body></html>").arg(title, message);
    const QByteArray bytes = body.toUtf8();
    return "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
           + QByteArray::number(bytes.size()) + "\r\nConnection: close\r\n\r\n" + bytes;
}

} // namespace

AuthManager::AuthManager(QNetworkAccessManager *nam, QObject *parent)
    : QObject(parent), m_nam(nam)
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &AuthManager::refresh);
    if(!QCoreApplication::arguments().contains(QStringLiteral("--local-only"))
       && !QCoreApplication::arguments().contains(QStringLiteral("--audio-smoke-test")))
        loadTokens();
}

QString AuthManager::redirectUri() const
{
    return QStringLiteral("http://127.0.0.1:%1/callback").arg(kCallbackPort);
}

void AuthManager::setClientId(const QString &id)
{
    const QString trimmed = id.trimmed();
    if (trimmed == m_clientId)
        return;
    m_clientId = trimmed;
    QSettings().setValue(QStringLiteral("auth/clientId"), m_clientId);
    emit clientIdChanged();
}

bool AuthManager::hasValidAccessToken() const
{
    return !m_accessToken.isEmpty()
        && m_expiresAt.isValid()
        && QDateTime::currentDateTimeUtc().secsTo(m_expiresAt) > 30;
}

bool AuthManager::isClientIdFormat(const QString &id) const
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{32}$"));
    return re.match(id.trimmed()).hasMatch();
}

// Confere o Client ID sem abrir o navegador. O /authorize não serve para isso (sem sessão no
// navegador ele manda tudo para a tela de login); o endpoint de token, chamado com um código
// falso, responde "invalid_client" para um ID inexistente e "invalid_grant" para um ID que existe.
void AuthManager::checkClientId(const QString &id)
{
    const QString trimmed = id.trimmed();
    if (!isClientIdFormat(trimmed)) {
        ++m_checkSerial; // descarta uma conferência em andamento
        m_checkedClientId.clear();
        setClientIdStatus(QString());
        return;
    }
    if (trimmed == m_checkedClientId && m_clientIdStatus != QLatin1String("unknown"))
        return;
    m_checkedClientId = trimmed;
    const int serial = ++m_checkSerial;
    setClientIdStatus(QStringLiteral("checking"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), QStringLiteral("tonal-conferencia"));
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    form.addQueryItem(QStringLiteral("client_id"), trimmed);
    form.addQueryItem(QStringLiteral("code_verifier"), QString(64, QLatin1Char('a'))); // PKCE: 43 a 128
    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_nam->post(req, form.query(QUrl::FullyEncoded).toLatin1());
    connect(reply, &QNetworkReply::finished, this, [this, reply, serial] {
        reply->deleteLater();
        if (serial != m_checkSerial)
            return;
        const QString error = QJsonDocument::fromJson(reply->readAll()).object()
                                  .value(QStringLiteral("error")).toString();
        if (error == QLatin1String("invalid_client"))
            setClientIdStatus(QStringLiteral("invalid"));
        else if (error == QLatin1String("invalid_grant"))
            setClientIdStatus(QStringLiteral("valid"));
        else
            setClientIdStatus(QStringLiteral("unknown")); // sem rede ou resposta inesperada
    });
}

void AuthManager::login()
{
    if (m_clientId.isEmpty()) {
        setError(tr("Informe o Client ID do seu app do Spotify."));
        return;
    }
    if (!isClientIdFormat(m_clientId)) {
        setError(tr("O Client ID tem 32 caracteres, só números e letras de a a f. Copie de novo do painel."));
        return;
    }
    if (m_clientId == m_checkedClientId && m_clientIdStatus == QLatin1String("invalid")) {
        setError(tr("O Spotify não conhece esse Client ID. Confira se copiou o Client ID, e não o Client Secret."));
        return;
    }
    setError(QString());
    m_codeVerifier = randomString(96);
    m_state = randomString(24);
    startCallbackServer();
    if (!m_server)
        return;

    QUrl url(QString::fromLatin1(kAuthorizeUrl));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), m_clientId);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("code_challenge"), pkceChallenge(m_codeVerifier));
    q.addQueryItem(QStringLiteral("state"), m_state);
    q.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScopes));
    url.setQuery(q);

    setBusy(true);
    QDesktopServices::openUrl(url);
}

// O usuário fechou o navegador ou desistiu: para de esperar o redirect.
void AuthManager::cancelLogin()
{
    stopCallbackServer();
    setBusy(false);
}

void AuthManager::logout()
{
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresAt = {};
    m_refreshTimer.stop();
    setGrantedScopes({});
    saveTokens();
    emit authenticatedChanged();
}

bool AuthManager::canEditPlaylists() const
{
    return m_grantedScopes.contains(QLatin1String("playlist-modify-public"))
        && m_grantedScopes.contains(QLatin1String("playlist-modify-private"));
}

void AuthManager::setGrantedScopes(const QStringList &scopes)
{
    if (scopes == m_grantedScopes)
        return;
    m_grantedScopes = scopes;
    emit grantedScopesChanged();
}

void AuthManager::startCallbackServer()
{
    stopCallbackServer();
    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::LocalHost, kCallbackPort)) {
        setError(tr("Não consegui abrir a porta %1 para o login: %2")
                     .arg(kCallbackPort).arg(m_server->errorString()));
        m_server->deleteLater();
        m_server = nullptr;
        return;
    }

    connect(m_server, &QTcpServer::newConnection, this, [this] {
        while (m_server && m_server->hasPendingConnections()) {
            QTcpSocket *socket = m_server->nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                // Só precisamos da primeira linha: "GET /callback?code=...&state=... HTTP/1.1"
                if (!socket->canReadLine())
                    return;
                const QByteArray line = socket->readLine().trimmed();
                const QList<QByteArray> parts = line.split(' ');
                if (parts.size() < 2) {
                    socket->disconnectFromHost();
                    return;
                }
                handleCallback(QString::fromLatin1(parts.at(1)), socket);
            });
        }
    });
}

void AuthManager::stopCallbackServer()
{
    if (!m_server)
        return;
    m_server->close();
    m_server->deleteLater();
    m_server = nullptr;
}

void AuthManager::handleCallback(const QString &requestPath, QTcpSocket *socket)
{
    const QUrl url(QStringLiteral("http://127.0.0.1") + requestPath);
    if (url.path() != QLatin1String("/callback")) {
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        socket->disconnectFromHost();
        return;
    }

    const QUrlQuery q(url);
    const QString error = q.queryItemValue(QStringLiteral("error"));
    const QString code = q.queryItemValue(QStringLiteral("code"));
    const QString state = q.queryItemValue(QStringLiteral("state"));

    if (!error.isEmpty() || code.isEmpty() || state != m_state) {
        socket->write(htmlPage(tr("Login não concluído"),
                               tr("Volte para o Tonal e tente de novo.")));
        socket->disconnectFromHost();
        setError(error.isEmpty() ? tr("Resposta de login inválida.") : tr("Login cancelado: %1").arg(error));
        setBusy(false);
        stopCallbackServer();
        return;
    }

    socket->write(htmlPage(tr("Pronto!"), tr("Login feito. Pode fechar esta aba e voltar para o Tonal.")));
    socket->disconnectFromHost();
    stopCallbackServer();
    exchangeCode(code);
}

void AuthManager::exchangeCode(const QString &code)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), redirectUri());
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("code_verifier"), m_codeVerifier);
    requestToken(form.query(QUrl::FullyEncoded).toLatin1(), false);
}

void AuthManager::refresh()
{
    if (m_refreshing)
        return;
    if (m_refreshToken.isEmpty() || m_clientId.isEmpty()) {
        emit tokenRefreshed(false);
        return;
    }
    m_refreshing = true;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    requestToken(form.query(QUrl::FullyEncoded).toLatin1(), true);
}

void AuthManager::requestToken(const QByteArray &formBody, bool isRefresh)
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kTokenUrl)));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_nam->post(req, formBody);
    setReplyDeadline(reply, m_tokenDeadlineMs);

    connect(reply, &QNetworkReply::finished, this, [this, reply, isRefresh] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();

        if (status != 200 || !obj.contains(QStringLiteral("access_token"))) {
            const QString err = obj.value(QStringLiteral("error_description")).toString(
                obj.value(QStringLiteral("error")).toString(reply->errorString()));
            if (isRefresh) {
                m_refreshing = false;
                // Refresh token revogado/expirado: precisa logar de novo.
                if (status == 400 || status == 401) {
                    setError(tr("Sessão expirada, faça login de novo."));
                    logout();
                } else {
                    // Erro de rede: tenta de novo em 30 s.
                    m_refreshTimer.start(30 * 1000);
                }
                emit tokenRefreshed(false);
            } else {
                setError(tr("Falha no login: %1").arg(err));
                setBusy(false);
            }
            return;
        }

        const bool wasAuthenticated = isAuthenticated();
        m_accessToken = obj.value(QStringLiteral("access_token")).toString();
        const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);
        m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(expiresIn);
        const QString newRefresh = obj.value(QStringLiteral("refresh_token")).toString();
        if (!newRefresh.isEmpty())
            m_refreshToken = newRefresh;
        // O refresh mantém as permissões do login original; só um login novo pede as que faltam.
        if (obj.contains(QStringLiteral("scope")))
            setGrantedScopes(obj.value(QStringLiteral("scope")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts));
        saveTokens();
        scheduleRefresh();
        setError(QString());

        if (isRefresh) {
            m_refreshing = false;
            emit tokenRefreshed(true);
        } else {
            setBusy(false);
        }
        if (!wasAuthenticated)
            emit authenticatedChanged();
    });
}

void AuthManager::scheduleRefresh()
{
    const qint64 secs = QDateTime::currentDateTimeUtc().secsTo(m_expiresAt) - 120;
    m_refreshTimer.start(int(qMax<qint64>(secs, 10)) * 1000);
}

void AuthManager::loadTokens()
{
    QSettings s;
    m_clientId = s.value(QStringLiteral("auth/clientId")).toString();
    // Texto, não lista: o registro do Windows devolve listas de 1 item como string.
    m_grantedScopes = s.value(QStringLiteral("auth/scope")).toString().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (const auto stored = TokenStore::load()) {
        m_refreshToken = stored->refreshToken;
        m_accessToken = stored->accessToken;
        m_expiresAt = stored->expiresAt;
    } else {
        // Até a versão 0.1.0 os tokens ficavam no registro, em texto puro (e ficam lá se o cofre falhar ou não
        // existir). saveTokens() passa para o cofre e só então apaga do registro.
        m_refreshToken = s.value(QStringLiteral("auth/refreshToken")).toString();
        m_accessToken = s.value(QStringLiteral("auth/accessToken")).toString();
        m_expiresAt = s.value(QStringLiteral("auth/expiresAt")).toDateTime();
        if (!m_refreshToken.isEmpty())
            saveTokens();
    }
    if (isAuthenticated()) {
        if (hasValidAccessToken())
            scheduleRefresh();
        else
            QTimer::singleShot(0, this, &AuthManager::refresh);
    }
}

void AuthManager::saveTokens()
{
    QSettings s;
    s.setValue(QStringLiteral("auth/scope"), m_grantedScopes.join(QLatin1Char(' ')));
    const QString keys[] = { QStringLiteral("auth/refreshToken"), QStringLiteral("auth/accessToken"),
                             QStringLiteral("auth/expiresAt") };
    if (TokenStore::save({ m_refreshToken, m_accessToken, m_expiresAt })) {
        for (const QString &key : keys)
            s.remove(key);
        return;
    }
    // Sem o cofre (outro sistema, ou o Windows recusou): fica no QSettings, como antes.
    s.setValue(keys[0], m_refreshToken);
    s.setValue(keys[1], m_accessToken);
    s.setValue(keys[2], m_expiresAt);
}

void AuthManager::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

void AuthManager::setClientIdStatus(const QString &s)
{
    if (m_clientIdStatus == s)
        return;
    m_clientIdStatus = s;
    emit clientIdStatusChanged();
}

void AuthManager::setError(const QString &e)
{
    if (m_lastError == e)
        return;
    m_lastError = e;
    emit lastErrorChanged();
}
