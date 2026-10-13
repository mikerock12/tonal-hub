#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <optional>

// Cofre dos segredos do Tonal. No Windows é o Gerenciador de Credenciais: cada segredo vira uma credencial
// genérica "Tonal/<nome>", cifrada pelo Windows para o usuário ("Tonal/Spotify" para os tokens,
// "Tonal/setlist.fm" para a API key). Fora do Windows não há cofre: as leituras não acham nada e as gravações
// devolvem false, e quem chama guarda no QSettings.
namespace TokenStore {

std::optional<QByteArray> readSecret(const QString &name);
// false = não gravou (o chamador guarda de outro jeito).
bool writeSecret(const QString &name, const QByteArray &data, const QString &userName, const QString &comment);
// true também quando a credencial não existia.
bool deleteSecret(const QString &name);

struct Tokens
{
    QString refreshToken;
    QString accessToken;
    QDateTime expiresAt;
};

// Tokens do Spotify, como JSON em "Tonal/Spotify".
std::optional<Tokens> load();
// Tokens sem refresh token apagam a credencial. false = não gravou.
bool save(const Tokens &tokens);

} // namespace TokenStore
