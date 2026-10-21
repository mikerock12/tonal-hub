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
#include "Lyrics.h"
#include "Player.h"
#include "SetlistFm.h"
#include "SpotifyApi.h"
#include "audio/AudioEngine.hpp"
#include "indie/CatalogService.h"
#include "indie/HubController.h"
#include "companion/MusicCompanion.h"
#include "playlist/TextPlaylistImport.hpp"
#include "playback/ListeningMix.hpp"
#include "ThemeCatalog.h"

// Registro dos tipos C++ que o QML enxerga (sem criação pelo QML).
struct AuthManagerForeign {
    Q_GADGET
    QML_FOREIGN(AuthManager)
    QML_ANONYMOUS
};
struct AudioEngineForeign {
    Q_GADGET
    QML_FOREIGN(tonal::audio::AudioEngine)
    QML_ANONYMOUS
};
struct IndieCatalogForeign { Q_GADGET QML_FOREIGN(CatalogService) QML_ANONYMOUS };
struct TonalHubForeign { Q_GADGET QML_FOREIGN(HubController) QML_ANONYMOUS };
struct MusicCompanionForeign { Q_GADGET QML_FOREIGN(MusicCompanion) QML_ANONYMOUS };
struct ListeningMixForeign { Q_GADGET QML_FOREIGN(ListeningMix) QML_ANONYMOUS };
struct TextPlaylistForeign { Q_GADGET QML_FOREIGN(TextPlaylistImport) QML_ANONYMOUS };
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
struct SetlistFmForeign {
    Q_GADGET
    QML_FOREIGN(SetlistFm)
    QML_ANONYMOUS
};
struct LyricsForeign {
    Q_GADGET
    QML_FOREIGN(Lyrics)
    QML_NAMED_ELEMENT(Lyrics)
    QML_UNCREATABLE("Use App.lyrics")
};

// Ponto de entrada do QML: App.auth, App.api, App.player, App.librespot, App.lyrics, App.setlistFm
class App : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(AuthManager *auth READ auth CONSTANT)
    Q_PROPERTY(SpotifyApi *api READ api CONSTANT)
    Q_PROPERTY(Player *player READ player CONSTANT)
    Q_PROPERTY(LibrespotProcess *librespot READ librespot CONSTANT)
    Q_PROPERTY(Lyrics *lyrics READ lyrics CONSTANT)
    Q_PROPERTY(SetlistFm *setlistFm READ setlistFm CONSTANT)
    Q_PROPERTY(tonal::audio::AudioEngine *audio READ audio CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(CatalogService *discovery READ discovery CONSTANT)
    Q_PROPERTY(HubController *hub READ hub CONSTANT)
    Q_PROPERTY(MusicCompanion *music READ music CONSTANT)
    Q_PROPERTY(ListeningMix *listeningMix READ listeningMix CONSTANT)
    Q_PROPERTY(TextPlaylistImport *textPlaylist READ textPlaylist CONSTANT)
    Q_PROPERTY(ThemeCatalog *theme READ theme CONSTANT)
    // Idioma em uso nesta execução: "pt" ou "en".
    Q_PROPERTY(QString language READ language CONSTANT)

public:
    // Sem construtor padrão de propósito: se o App fosse default-constructible, o QML criaria uma
    // segunda instância com ele (o Qt prefere o construtor ao create()), com outro librespot e sem o
    // engine JS no SpotifyApi. Assim o QML sempre usa create(), que devolve a instância do main.cpp.
    explicit App(QObject *parent);
    ~App() override;

    static void setInstance(App *app) { s_instance = app; }
    static App *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    AuthManager *auth() const { return m_auth; }
    SpotifyApi *api() const { return m_api; }
    Player *player() const { return m_player; }
    LibrespotProcess *librespot() const { return m_librespot; }
    Lyrics *lyrics() const { return m_lyrics; }
    SetlistFm *setlistFm() const { return m_setlistFm; }
    tonal::audio::AudioEngine *audio() const { return m_audio; }
    QString version() const;
    CatalogService *discovery() const { return m_discovery; }
    HubController *hub() const { return m_hub; }
    MusicCompanion *music() const { return m_music; }
    ListeningMix *listeningMix() const{return m_listeningMix;}
    TextPlaylistImport *textPlaylist() const { return m_textPlaylist; }
    ThemeCatalog *theme() const { return m_theme; }
    QString language() const;

    // Idioma do próximo início: o salvo no menu ou, sem escolha, o do Windows (português ou, senão, inglês).
    static QString startupLanguage();
    static bool restartRequested() { return s_restartRequested; }

    // Guarda o idioma escolhido ("pt" ou "en"); vale a partir do próximo início.
    Q_INVOKABLE void setLanguage(const QString &language);
    // Fecha o Tonal e abre de novo (o main.cpp abre a instância nova depois de parar o librespot).
    Q_INVOKABLE void restart();

    Q_INVOKABLE void copyToClipboard(const QString &text) const;
    Q_INVOKABLE QString clipboardText() const;
    // Cor predominante de uma capa (para o degradê do cabeçalho, igual ao app oficial).
    // callback(colorString)
    Q_INVOKABLE void dominantColor(const QString &imageUrl, const QJSValue &callback);

private:
    static inline App *s_instance = nullptr;
    static inline bool s_restartRequested = false;

    QNetworkAccessManager *m_nam;
    QNetworkAccessManager *m_imageNam;
    QHash<QString, QString> m_colorCache;
    AuthManager *m_auth;
    SpotifyApi *m_api;
    tonal::audio::AudioEngine *m_audio;
    LibrespotProcess *m_librespot;
    Player *m_player;
    Lyrics *m_lyrics;
    SetlistFm *m_setlistFm;
    CatalogService *m_discovery;
    HubController *m_hub;
    MusicCompanion *m_music;
    TextPlaylistImport *m_textPlaylist;
    ListeningMix *m_listeningMix;
    ThemeCatalog *m_theme;
};
