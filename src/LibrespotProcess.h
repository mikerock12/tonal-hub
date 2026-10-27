#pragma once

#include <QElapsedTimer>
#include <utility>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <cstdint>
namespace tonal::audio {
class AudioEngine;
}

// Roda o librespot (https://github.com/librespot-org/librespot) como processo
// auxiliar. Ele aparece como um dispositivo Spotify Connect ("Tonal @ PC") e é
// quem de fato baixa, decodifica e toca o áudio. O Tonal só manda comandos
// pela Web API apontando para esse dispositivo.
class LibrespotProcess : public QObject {
  Q_OBJECT
  Q_PROPERTY(State state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
  Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
  Q_PROPERTY(
      QString executablePath READ executablePath NOTIFY executablePathChanged)
  Q_PROPERTY(int bitrate READ bitrate WRITE setBitrate NOTIFY bitrateChanged)

public:
  enum State { Stopped, NotFound, Starting, NeedsLogin, Ready, Crashed };
  Q_ENUM(State)

  explicit LibrespotProcess(QObject *parent = nullptr,
                            const QString &storageRoot = {});
  ~LibrespotProcess() override;

  State state() const { return m_state; }
  QString statusText() const { return m_statusText; }
  QString deviceName() const { return m_deviceName; }
  QString executablePath() const { return m_exe; }
  QString sessionCachePath() const { return cacheDir(); }
  bool nativeLoginAvailable() const { return hasCachedCredentials(); }
  int bitrate() const { return m_bitrate; }
  void setBitrate(int kbps);
  void setAudioEngine(tonal::audio::AudioEngine *engine);
  void setPlaybackExpected(bool playing);

  Q_INVOKABLE void start();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void restart();
  // Apaga as credenciais salvas do librespot e refaz o login dele.
  Q_INVOKABLE void resetLogin();
  Q_INVOKABLE void setExecutablePath(const QString &path);
  Q_INVOKABLE QString recentLog() const {
    return m_log.join(QLatin1Char('\n'));
  }

signals:
  void stateChanged();
  void executablePathChanged();
  void bitrateChanged();
  // Disparado quando o librespot carrega/troca de faixa, pausa, etc.
  // O Player usa isso para atualizar o estado sem ficar fazendo polling
  // agressivo.
  void playbackEvent();
  // A sessão com o Spotify caiu (o motor vai voltar sozinho). O que estava
  // tocando aqui se perde.
  void sessionLost();

public:
  // Primeira faixa de uma sequência que o librespot pulou por falta de chave de
  // áudio. O Player retoma por ela depois do reinício; consumir uma vez.
  QString takeFailedTrack() { return std::exchange(m_failedTrack, QString()); }

private:
  friend class SpotifyRecoveryTests;
  void beginRestart(bool automatic, const QString &reason);
  void checkPlaybackHealth();
  void logDiagnostic(const QString &event);
  static QString safeLog(QString line);
  void setState(State s, const QString &text = {});
  void onReadyRead();
  void handleLine(const QString &line);
  void onFinished(int exitCode, QProcess::ExitStatus status);
  QString findExecutable() const;
  QString cacheDir() const;
  bool hasCachedCredentials() const;
  bool requestGracefulExit();
  void attachToJob();

  QProcess m_process;
  QTimer m_restartTimer;
  QTimer m_terminateTimer, m_healthTimer;
  QElapsedTimer m_recoveryWindow, m_pcmProgressClock, m_loadFailureClock, m_keyTimeoutClock;
  int m_loadFailures = 0;
  QString m_firstFailedTrack, m_failedTrack;
  QByteArray m_buffer;
  QStringList m_log;
  QString m_exe;
  QString m_deviceName;
  QString m_statusText;
  State m_state = Stopped;
  int m_bitrate = 320;
  int m_crashCount = 0;
  bool m_stopping = false;
  tonal::audio::AudioEngine *m_audio = nullptr;
  uintptr_t m_pcmWriter = 0;
  bool m_deferPcm = false;
  QString m_storageRoot;
  bool m_restartRequested = false, m_autoRestartBlocked = false;
  bool m_expectedPlayback = false;
  int m_restartDelay = 0, m_restartBaseMs = 1500, m_stallTimeoutMs = 30000;
  uint64_t m_lastPcmFrames = 0;
};
