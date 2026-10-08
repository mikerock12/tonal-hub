#pragma once

#include <QHash>
#include <QJSValue>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QNetworkAccessManager;
class QQmlEngine;

#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "Player.h"
#include "SpotifyApi.h"

// Registro dos tipos C++ que o QML enxerga (sem criação pelo QML).
struct AuthManagerForeign {
    Q_GADGET
    QML_FOREIGN(AuthManager)
    QML_ANONYMOUS
};
struct SpotifyApiForeign {
    Q_GADGET
    QML_FOREIGN(SpotifyApi)
    QML_ANONYMOUS
};
struct PlayerForeign {
    Q_GADGET
    QML_FOREIGN(Player)
    QML_ANONYMOUS
};
struct LibrespotForeign {
    Q_GADGET
    QML_FOREIGN(LibrespotProcess)
    QML_NAMED_ELEMENT(Librespot)
    QML_UNCREATABLE("Use App.librespot")
};

// Ponto de entrada do QML: App.auth, App.api, App.player, App.librespot
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(AuthManager *auth READ auth CONSTANT)
    Q_PROPERTY(SpotifyApi *api READ api CONSTANT)
    Q_PROPERTY(Player *player READ player CONSTANT)
    Q_PROPERTY(LibrespotProcess *librespot READ librespot CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)

public:
    explicit App(QObject *parent = nullptr);
    ~App() override;

    static void setInstance(App *app) { s_instance = app; }
    static App *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    AuthManager *auth() const { return m_auth; }
    SpotifyApi *api() const { return m_api; }
    Player *player() const { return m_player; }
    LibrespotProcess *librespot() const { return m_librespot; }
    QString version() const;

    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    // Cor predominante de uma capa (para o degradê do cabeçalho, igual ao app oficial).
    // callback(colorString)
    Q_INVOKABLE void dominantColor(const QString &imageUrl, const QJSValue &callback);

private:
    static inline App *s_instance = nullptr;

    QNetworkAccessManager *m_nam;
    QNetworkAccessManager *m_imageNam;
    QHash<QString, QString> m_colorCache;
    AuthManager *m_auth;
    SpotifyApi *m_api;
    LibrespotProcess *m_librespot;
    Player *m_player;
};
