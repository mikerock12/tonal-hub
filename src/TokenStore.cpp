#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

#include "TokenStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

#include <string>

Q_LOGGING_CATEGORY(lcTokens, "tonal.auth", QtInfoMsg)

namespace TokenStore {

namespace {

const QString kSpotify = QStringLiteral("Spotify");

} // namespace

#ifdef _WIN32

namespace {

// Nome que aparece em Painel de Controle > Gerenciador de Credenciais > Credenciais do Windows.
std::wstring targetFor(const QString &name)
{
    return (QStringLiteral("Tonal/") + name).toStdWString();
}

} // namespace

std::optional<QByteArray> readSecret(const QString &name)
{
    const std::wstring target = targetFor(name);
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND)
            qCWarning(lcTokens) << "Não consegui ler a credencial" << name << error;
        return std::nullopt;
    }
    QByteArray blob(reinterpret_cast<const char *>(cred->CredentialBlob), int(cred->CredentialBlobSize));
    CredFree(cred);
    return blob;
}

bool writeSecret(const QString &name, const QByteArray &data, const QString &userName, const QString &comment)
{
    if (data.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        qCWarning(lcTokens) << "Segredo maior que o limite do Gerenciador de Credenciais:" << name << data.size();
        return false;
    }
    std::wstring target = targetFor(name);
    std::wstring user = userName.toStdWString();
    std::wstring note = comment.toStdWString();
    QByteArray blob = data;

    CREDENTIALW cred {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = target.data();
    cred.UserName = user.data();
    cred.Comment = note.data();
    cred.CredentialBlobSize = DWORD(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE; // só nesta máquina, não vai junto com o perfil móvel
    const bool written = CredWriteW(&cred, 0);
    SecureZeroMemory(blob.data(), size_t(blob.size()));
    if (!written)
        qCWarning(lcTokens) << "Não consegui gravar a credencial" << name << GetLastError();
    return written;
}

bool deleteSecret(const QString &name)
{
    const std::wstring target = targetFor(name);
    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND)
        return true;
    qCWarning(lcTokens) << "Não consegui apagar a credencial" << name << GetLastError();
    return false;
}

#else

std::optional<QByteArray> readSecret(const QString &name)
{
    Q_UNUSED(name)
    return std::nullopt;
}

bool writeSecret(const QString &name, const QByteArray &data, const QString &userName, const QString &comment)
{
    Q_UNUSED(name)
    Q_UNUSED(data)
    Q_UNUSED(userName)
    Q_UNUSED(comment)
    return false;
}

bool deleteSecret(const QString &name)
{
    Q_UNUSED(name)
    return false;
}

#endif

std::optional<Tokens> load()
{
    const auto blob = readSecret(kSpotify);
    if (!blob)
        return std::nullopt;
    const QJsonObject obj = QJsonDocument::fromJson(*blob).object();
    Tokens tokens;
    tokens.refreshToken = obj.value(QStringLiteral("refresh")).toString();
    tokens.accessToken = obj.value(QStringLiteral("access")).toString();
    tokens.expiresAt = QDateTime::fromString(obj.value(QStringLiteral("expiresAt")).toString(), Qt::ISODate);
    if (tokens.refreshToken.isEmpty())
        return std::nullopt;
    return tokens;
}

bool save(const Tokens &tokens)
{
    if (tokens.refreshToken.isEmpty())
        return deleteSecret(kSpotify);
    const QByteArray blob = QJsonDocument(QJsonObject {
                                              { QStringLiteral("refresh"), tokens.refreshToken },
                                              { QStringLiteral("access"), tokens.accessToken },
                                              { QStringLiteral("expiresAt"), tokens.expiresAt.toString(Qt::ISODate) },
                                          })
                                .toJson(QJsonDocument::Compact);
    return writeSecret(kSpotify, blob, QStringLiteral("Spotify Web API"), QStringLiteral("Login do Tonal no Spotify"));
}

} // namespace TokenStore
