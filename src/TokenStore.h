#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

// Cofre dos tokens do Spotify. No Windows é o Gerenciador de Credenciais (credencial genérica "Tonal/Spotify",
// cifrada pelo Windows para o usuário). Fora do Windows não há cofre: load() não acha nada e save() devolve
// false, e o AuthManager continua no QSettings.
namespace TokenStore {

struct Tokens
{
    QString refreshToken;
    QString accessToken;
    QDateTime expiresAt;
};

std::optional<Tokens> load();
// Tokens sem refresh token apagam a credencial. false = não gravou (o chamador guarda de outro jeito).
bool save(const Tokens &tokens);

} // namespace TokenStore
