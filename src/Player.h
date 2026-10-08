#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class SpotifyApi;
class LibrespotProcess;
class AuthManager;

// Estado de reprodução + comandos (play, pause, próxima, seek, volume...).
// O estado vem de GET /me/player; a posição da barra é interpolada localmente
// para não precisar consultar a API a cada segundo.
class Player : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY trackChanged)
    Q_PROPERTY(QString trackName READ trackName NOTIFY trackChanged)
    Q_PROPERTY(QString trackUri READ trackUri NOTIFY trackChanged)
    Q_PROPERTY(QVariantList artists READ artists NOTIFY trackChanged)
    Q_PROPERTY(QString artistsText READ artistsText NOTIFY trackChanged)
    Q_PROPERTY(QString albumName READ albumName NOTIFY trackChanged)
    Q_PROPERTY(QString albumUri READ albumUri NOTIFY trackChanged)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY trackChanged)
    Q_PROPERTY(QString coverUrlLarge READ coverUrlLarge NOTIFY trackChanged)
    Q_PROPERTY(int durationMs READ durationMs NOTIFY trackChanged)
    Q_PROPERTY(QString contextUri READ contextUri NOTIFY stateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool shuffle READ shuffle NOTIFY stateChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY stateChanged)
    Q_PROPERTY(int volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(int positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(bool liked READ liked NOTIFY likedChanged)
    Q_PROPERTY(QString activeDeviceId READ activeDeviceId NOTIFY stateChanged)
    Q_PROPERTY(QString activeDeviceName READ activeDeviceName NOTIFY stateChanged)
    Q_PROPERTY(bool playingHere READ playingHere NOTIFY stateChanged)
    Q_PROPERTY(QString localDeviceId READ localDeviceId NOTIFY localDeviceChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    Player(SpotifyApi *api, LibrespotProcess *librespot, AuthManager *auth, QObject *parent = nullptr);

    bool hasTrack() const { return !m_trackUri.isEmpty(); }
    QString trackName() const { return m_trackName; }
    QString trackUri() const { return m_trackUri; }
    QVariantList artists() const { return m_artists; }
    QString artistsText() const;
    QString albumName() const { return m_albumName; }
    QString albumUri() const { return m_albumUri; }
    QString coverUrl() const { return m_coverUrl; }
    QString coverUrlLarge() const { return m_coverUrlLarge; }
    int durationMs() const { return m_durationMs; }
    QString contextUri() const { return m_contextUri; }
    bool isPlaying() const { return m_isPlaying; }
    bool shuffle() const { return m_shuffle; }
    QString repeatMode() const { return m_repeatMode; }
    int volume() const { return m_volume; }
    int positionMs() const;
    bool liked() const { return m_liked; }
    QString activeDeviceId() const { return m_activeDeviceId; }
    QString activeDeviceName() const { return m_activeDeviceName; }
    bool playingHere() const { return !m_localDeviceId.isEmpty() && m_activeDeviceId == m_localDeviceId; }
    QString localDeviceId() const { return m_localDeviceId; }
    QVariantList queue() const { return m_queue; }
    QVariantList devices() const { return m_devices; }

    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(int ms);
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void toggleLike();
    // Toca um álbum/playlist/artista. offsetUri ou offsetIndex escolhem a faixa inicial.
    Q_INVOKABLE void playContext(const QString &contextUri, const QString &offsetUri = {}, int offsetIndex = -1);
    // Toca uma lista solta de faixas (ex.: resultados de busca).
    Q_INVOKABLE void playUris(const QStringList &uris, int offsetIndex = 0);
    Q_INVOKABLE void addToQueue(const QString &uri);
    Q_INVOKABLE void transferTo(const QString &deviceId);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshQueue();
    Q_INVOKABLE void refreshDevices();

signals:
    void trackChanged();
    void stateChanged();
    void volumeChanged();
    void positionChanged();
    void likedChanged();
    void localDeviceChanged();
    void queueChanged();
    void devicesChanged();
    void message(const QString &text);

private:
    void applyState(const QJsonObject &obj);
    void clearState();
    void refreshSoon(int ms = 400);
    void scheduleNextPoll();
    void checkLiked();
    void findLocalDevice(int attemptsLeft);
    QString targetDeviceForPlay() const;
    void sendPlay(const QJsonObject &body, const QString &deviceId, bool retried = false);
    void setPositionBase(int ms);

    SpotifyApi *m_api;
    LibrespotProcess *m_librespot;
    AuthManager *m_auth;

    QTimer m_pollTimer;
    QTimer m_tickTimer;
    QTimer m_volumeTimer;
    QElapsedTimer m_positionClock;
    int m_positionBase = 0;
    int m_pendingVolume = -1;
    bool m_requestInFlight = false;

    QString m_trackName, m_trackUri, m_albumName, m_albumUri, m_coverUrl, m_coverUrlLarge;
    QVariantList m_artists;
    int m_durationMs = 0;
    QString m_contextUri;
    bool m_isPlaying = false;
    bool m_shuffle = false;
    QString m_repeatMode = QStringLiteral("off");
    int m_volume = 60;
    bool m_liked = false;
    QString m_activeDeviceId, m_activeDeviceName, m_localDeviceId;
    QVariantList m_queue, m_devices;
};
