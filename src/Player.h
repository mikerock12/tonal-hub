#pragma once

#include "audio/AudioEngine.hpp"
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
class Player : public QObject {
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
  Player(SpotifyApi *api, LibrespotProcess *librespot, AuthManager *auth,
         QObject *parent = nullptr);
  void setAudioEngine(tonal::audio::AudioEngine *engine);
  bool localPlayback() const { return m_audio && m_audio->localActive(); }

  bool hasTrack() const { return localPlayback() || !m_trackUri.isEmpty(); }
  QString trackName() const {
    return localPlayback() ? m_audio->localName() : m_trackName;
  }
  QString trackUri() const {
    return localPlayback() ? QStringLiteral("local:audio") : m_trackUri;
  }
  QVariantList artists() const {
    return localPlayback() ? QVariantList{} : m_artists;
  }
  QString artistsText() const;
  QString albumName() const {
    return localPlayback() ? QStringLiteral("Arquivo local") : m_albumName;
  }
  QString albumUri() const { return localPlayback() ? QString() : m_albumUri; }
  QString coverUrl() const { return localPlayback() ? QString() : m_coverUrl; }
  QString coverUrlLarge() const {
    return localPlayback() ? QString() : m_coverUrlLarge;
  }
  int durationMs() const {
    return localPlayback() ? m_audio->durationMs() : m_durationMs;
  }
  QString contextUri() const { return m_contextUri; }
  bool isPlaying() const {
    return localPlayback() ? m_audio->localPlaying() : m_isPlaying;
  }
  bool shuffle() const { return !localPlayback() && m_shuffle; }
  QString repeatMode() const {
    return localPlayback() ? QStringLiteral("off") : m_repeatMode;
  }
  int volume() const { return m_volume; }
  int positionMs() const;
  bool liked() const { return !localPlayback() && m_liked; }
  QString activeDeviceId() const { return m_activeDeviceId; }
  QString activeDeviceName() const {
    return localPlayback() ? QStringLiteral("Saída local") : m_activeDeviceName;
  }
  bool playingHere() const {
    return localPlayback() ||
           (!m_localDeviceId.isEmpty() && m_activeDeviceId == m_localDeviceId);
  }
  QString localDeviceId() const { return m_localDeviceId; }
  QVariantList queue() const { return m_queue; }
  QVariantList preparedQueue() const {return m_orderedRows;}
  bool queueEditPending() const{return m_queueEditPending;}
  void reconcileJamQueue(const QVariantList &rows){if(rows.isEmpty()||rows.first().toMap().value("uri").toString()!=m_trackUri)return;m_orderedRows=rows;emit orderedQueueEdited(rows);queueFromOrderedRows();}
  QVariantList devices() const { return m_devices; }

  Q_INVOKABLE void togglePlay();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void next();
  Q_INVOKABLE void previous();
  Q_INVOKABLE void seek(int ms);
  Q_INVOKABLE void setVolume(int percent);
  // Mudo: guarda o volume anterior (persistido) e o restaura ao reativar.
  Q_INVOKABLE void toggleMute();
  Q_INVOKABLE void toggleShuffle();
  Q_INVOKABLE void cycleRepeat();
  Q_INVOKABLE void toggleLike();
  // Toca um álbum/playlist/artista. offsetUri ou offsetIndex escolhem a faixa
  // inicial.
  Q_INVOKABLE void playContext(const QString &contextUri,
                               const QString &offsetUri = {},
                               int offsetIndex = -1);
  // Toca uma lista solta de faixas (ex.: resultados de busca).
  Q_INVOKABLE void playUris(const QStringList &uris, int offsetIndex = 0);
  Q_INVOKABLE void playOrderedUris(const QStringList &uris);
  void playOrderedUrisAt(const QStringList &uris,int position);
  // Linhas do mix aplicado (título, artista, capa, Descoberta) para a Fila.
  Q_INVOKABLE void setOrderedQueue(const QVariantList &rows);
  Q_INVOKABLE void addToQueue(const QString &uri);
  Q_INVOKABLE void insertInQueue(const QVariantMap &track,bool next);
  Q_INVOKABLE void transferTo(const QString &deviceId);
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void refreshQueue();
  Q_INVOKABLE void refreshDevices();

signals:
  void playbackRequested();
  void trackChanged();
  void stateChanged();
  void volumeChanged();
  void positionChanged();
  void likedChanged();
  void localDeviceChanged();
  void queueChanged();
  void queueRefreshFinished(int status);
  void orderedQueueEdited(const QVariantList &rows);
  // Uma seleção comum (não um mix) substituiu a reprodução.
  void selectionReplaced();
  void devicesChanged();
  void message(const QString &text);

private:
  friend class SpotifyRecoveryTests;
  void cancelResume();
  void applyState(const QJsonObject &obj, bool acceptVolume = true);
  void dispatchVolume();
  void cancelVolume();
  void clearState();
  void refreshSoon(int ms = 400);
  void scheduleNextPoll();
  void checkLiked();
  void loadContextQueue(const QString &path, const QString &context,
                        const QString &track, uint64_t generation,
                        QVariantList rows = {}, QJsonObject album = {}, int page = 0);
  void findLocalDevice(int attemptsLeft);
  QString targetDeviceForPlay() const;
  void sendPlay(const QJsonObject &body, const QString &deviceId,
                bool retried = false, bool ordered = false);
  void setPositionBase(int ms);
  void rememberForResume();
  void resumeAfterEngineRestart();

  SpotifyApi *m_api;
  LibrespotProcess *m_librespot;
  AuthManager *m_auth;
  tonal::audio::AudioEngine *m_audio = nullptr;

  QTimer m_pollTimer;
  QTimer m_tickTimer;
  QTimer m_volumeTimer;
  QElapsedTimer m_volumeIntentClock;
  QElapsedTimer m_volumeSentClock;
  // Desde quando o motor voltou a ficar pronto: um volume remoto muito
  // diferente logo depois é o valor inicial do motor, não escolha do usuário.
  QElapsedTimer m_engineReadyClock;
  void reassertVolume();
  uint64_t m_volumeGeneration = 0;
  uint64_t m_volumeRouteGeneration = 0;
  bool m_volumeInFlight = false;
  QString m_volumeDevice;
  // Seek: a posição aparece na hora; o pedido à Web API sai uma vez, com a
  // última posição pedida, e um GET atrasado não puxa a barra de volta.
  QTimer m_seekTimer;
  QElapsedTimer m_seekClock;
  int m_seekTarget = -1;
  uint64_t m_seekGeneration = 0;
  // Ida e volta de um comando à Web API (média móvel), para reaplicar a
  // fila na posição em que a faixa atual vai estar quando o comando chegar.
  int m_commandLatencyMs = 250;
  bool seekSettling(int remoteMs) const;
  void cancelSeek();
  // Repetição escolhida pelo usuário; o librespot a zera a cada nova seleção.
  QString m_repeatWanted = QStringLiteral("off");
  QElapsedTimer m_repeatClock;
  QElapsedTimer m_selectionClock;
  QVariantList m_orderedRows;
  bool m_queueInsertionBusy=false,m_queueEditPending=false;
  QString m_queueEditAnchor,m_queueEditDevice;
  void reapplyRepeat(const QString &deviceId);
  void skipLeavesTrackRepeat();
  bool queueFromOrderedRows();
  void playOrdered(const QStringList &uris, int position, bool keepCurrent);
  QElapsedTimer m_positionClock;
  QElapsedTimer m_transportIntentClock;
  uint64_t m_transportGeneration = 0;
  int m_positionBase = 0;
  int m_pendingVolume = -1;
  bool m_requestInFlight = false;

  QString m_trackName, m_trackUri, m_albumName, m_albumUri, m_coverUrl,
      m_coverUrlLarge;
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

  // Lista do último playUris (busca, populares): a retomada precisa dela para
  // não perder a fila.
  QStringList m_lastUris;
  // O que retomar quando o motor voltar depois de perder a sessão (vazio =
  // nada).
  QJsonObject m_resumeBody;
  bool m_resumeShuffle = false;
  QString m_resumeRepeat;
  bool m_resumeInFlight = false;
  int m_resumeAttempts = 0;
  uint64_t m_resumeGeneration = 0;
};
