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

Q_LOGGING_CATEGORY(lcTokens, "tonal.auth", QtInfoMsg)

namespace TokenStore {

#ifdef _WIN32

namespace {

// Nome que aparece em Painel de Controle > Gerenciador de Credenciais > Credenciais do Windows.
wchar_t kTarget[] = L"Tonal/Spotify";

} // namespace

std::optional<Tokens> load()
{
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(kTarget, CRED_TYPE_GENERIC, 0, &cred)) {
        const DWORD error = GetLastError();
        if (error != ERROR_NOT_FOUND)
            qCWarning(lcTokens) << "Não consegui ler a credencial do Tonal:" << error;
        return std::nullopt;
    }
    const QByteArray blob(reinterpret_cast<const char *>(cred->CredentialBlob), int(cred->CredentialBlobSize));
    CredFree(cred);

    const QJsonObject obj = QJsonDocument::fromJson(blob).object();
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
    if (tokens.refreshToken.isEmpty()) {
        if (!CredDeleteW(kTarget, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) {
            qCWarning(lcTokens) << "Não consegui apagar a credencial do Tonal:" << GetLastError();
            return false;
        }
        return true;
    }

    QByteArray blob = QJsonDocument(QJsonObject {
                                        { QStringLiteral("refresh"), tokens.refreshToken },
                                        { QStringLiteral("access"), tokens.accessToken },
                                        { QStringLiteral("expiresAt"), tokens.expiresAt.toString(Qt::ISODate) },
                                    })
                          .toJson(QJsonDocument::Compact);
    if (blob.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
        qCWarning(lcTokens) << "Tokens maiores que o limite do Gerenciador de Credenciais:" << blob.size();
        return false;
    }

    wchar_t userName[] = L"Spotify Web API";
    wchar_t comment[] = L"Login do Tonal no Spotify";
    CREDENTIALW cred {};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = kTarget;
    cred.UserName = userName;
    cred.Comment = comment;
    cred.CredentialBlobSize = DWORD(blob.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(blob.data());
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE; // só nesta máquina, não vai junto com o perfil móvel
    const bool written = CredWriteW(&cred, 0);
    SecureZeroMemory(blob.data(), size_t(blob.size()));
    if (!written)
        qCWarning(lcTokens) << "Não consegui gravar a credencial do Tonal:" << GetLastError();
    return written;
}

#else

std::optional<Tokens> load()
{
    return std::nullopt;
}

bool save(const Tokens &tokens)
{
    Q_UNUSED(tokens)
    return false;
}

#endif

} // namespace TokenStore
