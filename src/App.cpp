#include "App.h"

#include <QClipboard>
#include <QJsonArray>
#include <QGuiApplication>
#include <QJSEngine>
#include <QNetworkAccessManager>
#include <QQmlEngine>
#include <QColor>
#include <QImage>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QLocale>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>

App::App(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_imageNam(new QNetworkAccessManager(this))
    , m_auth(new AuthManager(m_nam, this))
    , m_api(new SpotifyApi(m_nam, m_auth, this))
    , m_audio(new tonal::audio::AudioEngine(this))
    , m_librespot(new LibrespotProcess(this))
    , m_player(new Player(m_api, m_librespot, m_auth, this))
    , m_lyrics(new Lyrics(m_nam, m_player, this))
    , m_setlistFm(new SetlistFm(m_nam, this))
    , m_discovery(new CatalogService(this))
    , m_hub(new HubController(this))
    , m_music(new MusicCompanion(this))
    , m_textPlaylist(new TextPlaylistImport(m_music,[this](const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &body,const QByteArray &mime,TextPlaylistImport::Reply cb,std::function<bool()> current){m_api->raw(method,path,query,body,mime,std::move(cb),std::move(current));},this))
    , m_listeningMix(new ListeningMix(m_api,m_music,this))
    , m_theme(new ThemeCatalog(this))
{
    connect(m_player,&Player::playbackRequested,m_listeningMix,&ListeningMix::cancel);
    connect(m_listeningMix,&ListeningMix::ready,this,[this](const QVariantList& tracks){m_music->rememberTracks(tracks);QStringList uris;for(auto v:tracks){auto t=v.toMap();uris<<t["uri"].toString();}m_player->playOrderedUris(uris);});
    connect(m_listeningMix,&ListeningMix::resumeReady,this,[this](const QVariantList& tracks,int position){m_music->rememberTracks(tracks);QStringList uris;for(auto v:tracks)uris<<v.toMap()["uri"].toString();m_player->playOrderedUrisAt(uris,position);});
    m_librespot->setAudioEngine(m_audio);
    m_player->setAudioEngine(m_audio);
    connect(m_music,&MusicCompanion::metadataRequested,this,[this](const QVariantMap &t){
        if(t.value("provider")!="Spotify"||!m_auth->isAuthenticated())return;
        QString id=t.value("uri").toString().section(':',-1);if(id.size()!=22)return;const auto key=MusicCompanion::key(t);
        m_api->get("/tracks/"+id,{},[this,key](int status,const QJsonDocument &doc){if(status!=200)return;auto track=doc.object(),album=track["album"].toObject();auto artists=track["artists"].toArray();QVariantMap patch{{"album",album["name"].toString()},{"albumId",album["id"].toString()}};auto images=album["images"].toArray();if(!images.isEmpty())patch["image"]=images[0].toObject()["url"].toString();if(!artists.isEmpty()){auto id=artists[0].toObject()["id"].toString();patch["artistId"]=id;m_api->get("/artists/"+id,{},[this,key](int status,const QJsonDocument &doc){if(status!=200)return;QStringList genres;for(auto v:doc.object()["genres"].toArray())if(genres.size()<4)genres<<v.toString();if(!genres.isEmpty())m_music->mergeMetadata(key,{{"genre",genres.join(", ")},{"genreSource","Spotify • artista"}});});}m_music->mergeMetadata(key,patch);});
    });
    auto selected = [this] {
        if (!m_player->hasTrack()) return;
        QVariantMap t{{"provider",m_player->localPlayback()?"Local":"Spotify"},
          {"id",m_player->localPlayback()?m_audio->localFileUrl().toString():m_player->trackUri()},
          {"uri",m_player->trackUri()},{"title",m_player->trackName()},{"artist",m_player->artistsText()},
          {"album",m_player->albumName()},{"image",m_player->coverUrlLarge()},
          {"durationMs",m_player->durationMs()},{"playable",true}};
        if (m_player->localPlayback()) t["localFile"]=m_audio->localFileUrl().toString();
        if(!m_player->artists().isEmpty())t["artistId"]=m_player->artists().first().toMap()["id"];
        t["albumId"]=m_player->albumUri().section(':',-1);
        m_music->selectTrack(t);
    };
    connect(m_player,&Player::trackChanged,this,selected);
    connect(m_player,&Player::likedChanged,this,[this] {
        if(!m_player->localPlayback()&&m_music->current().value("uri").toString()==m_player->trackUri())m_music->setFavorite(m_music->current(),m_player->liked());
    });
    auto *historyTimer=new QTimer(this);historyTimer->setInterval(1000);
    connect(historyTimer,&QTimer::timeout,this,[this,lastFrames=uint64_t(0)]() mutable {
        const auto frames=m_audio->spotifyFramesRead();
        const bool actual=m_player->localPlayback()?m_audio->localPlaying():frames!=lastFrames;
        m_music->pulse(m_player->isPlaying()&&m_player->playingHere()&&actual,m_player->positionMs());lastFrames=frames;
    });historyTimer->start();
    connect(m_music,&MusicCompanion::seekRequested,m_player,&Player::seek);
    connect(m_music,&MusicCompanion::queueRequested,this,[this](const QVariantList &tracks,int index) {
        if (index<0||index>=tracks.size()) return;
        const auto t=tracks[index].toMap();
        if(t["provider"]=="Local") {m_audio->openFile(QUrl(t["localFile"].toString()));return;}
        QStringList uris;int offset=0;for(int i=0;i<tracks.size();i++){auto uri=tracks[i].toMap()["uri"].toString();if(uri.startsWith("spotify:track:")){if(i==index)offset=uris.size();uris<<uri;}}
        if(!uris.isEmpty())m_player->playUris(uris,offset);
    });
    connect(m_music,&MusicCompanion::spotifyFavoriteRequested,this,[this](const QVariantMap &t,bool on) {
        const auto done=[this,t,on](int status,const QJsonDocument &) {if(status>=200&&status<300){m_music->setFavorite(t,on);m_player->refresh();}else emit m_music->message("Não foi possível atualizar as Músicas Curtidas no Spotify. Confira as permissões da conta.");};
        const QVariantMap query{{"uris",t["uri"]}};
        if(on)m_api->put("/me/library",query,{},done);else m_api->del("/me/library",query,{},done);
    });
    connect(m_audio, &tonal::audio::AudioEngine::localModeRequested, m_librespot, &LibrespotProcess::stop);
    connect(m_audio, &tonal::audio::AudioEngine::spotifyModeRequested, this, [this] { if(m_auth->isAuthenticated())m_librespot->start(); });
    auto *cache = new QNetworkDiskCache(m_imageNam);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/colors"));
    cache->setMaximumCacheSize(30LL * 1024 * 1024);
    m_imageNam->setCache(cache);

    // O librespot só sobe depois do login da Web API, para não abrir
    // duas abas de login ao mesmo tempo no primeiro uso.
    connect(m_auth, &AuthManager::authenticatedChanged, this, [this] {
        if (m_auth->isAuthenticated())
            m_librespot->start();
        else
            m_librespot->stop();
    });
    if (m_auth->isAuthenticated())
        m_librespot->start();
}

App::~App()
{
    m_librespot->stop();
}

App *App::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    s_instance->m_api->setJsEngine(jsEngine);
    s_instance->m_setlistFm->setJsEngine(jsEngine);
    return s_instance;
}

QString App::version() const
{
    return QCoreApplication::applicationVersion();
}

QString App::language() const
{
    return QLocale().language() == QLocale::English ? QStringLiteral("en") : QStringLiteral("pt");
}

QString App::startupLanguage()
{
    const QString saved = QSettings().value(QStringLiteral("ui/language")).toString();
    if (saved == QLatin1String("pt") || saved == QLatin1String("en"))
        return saved;
    const QStringList system = QLocale::system().uiLanguages();
    return !system.isEmpty() && system.first().startsWith(QLatin1String("pt")) ? QStringLiteral("pt")
                                                                               : QStringLiteral("en");
}

void App::setLanguage(const QString &language)
{
    if (language == QLatin1String("pt") || language == QLatin1String("en"))
        QSettings().setValue(QStringLiteral("ui/language"), language);
}

void App::restart()
{
    s_restartRequested = true;
    QCoreApplication::quit();
}

void App::copyToClipboard(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}

QString App::clipboardText() const
{
    return QGuiApplication::clipboard()->text();
}

void App::dominantColor(const QString &imageUrl, const QJSValue &callback)
{
    if (imageUrl.isEmpty() || !callback.isCallable())
        return;
    const auto cached = m_colorCache.constFind(imageUrl);
    if (cached != m_colorCache.constEnd()) {
        QJSValue cb = callback;
        cb.call({ QJSValue(*cached) });
        return;
    }

    QNetworkRequest req { QUrl(imageUrl) };
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    QNetworkReply *reply = m_imageNam->get(req);
    QJSValue cb = callback;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cb, imageUrl]() mutable {
        reply->deleteLater();
        QImage img;
        if (reply->error() != QNetworkReply::NoError || !img.loadFromData(reply->readAll()))
            return;

        // Média de cores num thumbnail pequeno, ignorando pixels quase pretos/brancos.
        const QImage small = img.scaled(24, 24, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                 .convertToFormat(QImage::Format_RGB32);
        double r = 0, g = 0, b = 0, weight = 0;
        for (int y = 0; y < small.height(); ++y) {
            const QRgb *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
            for (int x = 0; x < small.width(); ++x) {
                const QColor c(line[x]);
                const double s = c.hsvSaturationF();
                const double v = c.valueF();
                if (v < 0.12 || (v > 0.95 && s < 0.08))
                    continue;
                const double w = 0.2 + s; // cores saturadas pesam mais
                r += c.redF() * w;
                g += c.greenF() * w;
                b += c.blueF() * w;
                weight += w;
            }
        }
        QColor color = weight > 0 ? QColor::fromRgbF(float(r / weight), float(g / weight), float(b / weight))
                                  : QColor(83, 83, 83);
        float h, s, l, a;
        color.getHslF(&h, &s, &l, &a);
        color = QColor::fromHslF(h < 0 ? 0 : h, qBound(0.0f, s, 0.75f), qBound(0.30f, l, 0.45f));

        const QString name = color.name();
        m_colorCache.insert(imageUrl, name);
        cb.call({ QJSValue(name) });
    });
}
