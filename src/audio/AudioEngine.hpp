#pragma once
#include "AudioSettings.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
namespace tonal::audio {
class NativePcmPipe;
class AudioEngine : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
  Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY diagnosticsChanged)
  Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY diagnosticsChanged)
  Q_PROPERTY(bool localActive READ localActive NOTIFY localChanged)
  Q_PROPERTY(bool localPlaying READ localPlaying NOTIFY localChanged)
  Q_PROPERTY(bool sourceFailed READ sourceFailed NOTIFY localChanged)
  Q_PROPERTY(QString localName READ localName NOTIFY localChanged)
  Q_PROPERTY(int durationMs READ durationMs NOTIFY localChanged)
  Q_PROPERTY(int positionMs READ positionMs NOTIFY positionChanged)
  Q_PROPERTY(bool hrtfAvailable READ hrtfAvailable NOTIFY diagnosticsChanged)
  Q_PROPERTY(bool hrtfActive READ hrtfActive NOTIFY diagnosticsChanged)
  Q_PROPERTY(bool iamfAvailable READ iamfAvailable CONSTANT)
  Q_PROPERTY(bool sourceIamf READ sourceIamf NOTIFY localChanged)
  Q_PROPERTY(bool visualizerActive READ visualizerActive WRITE setVisualizerActive)
public:
  explicit AudioEngine(QObject *parent = nullptr,
                       const QString &storageRoot = {});
  ~AudioEngine() override;
  QVariantMap settings() const;
  QString diagnostics() const { return m_diagnostics; }
  QVariantMap metrics() const { return m_metrics; }
  bool advancedEnabled() const;
  bool normalizationEnabled() const;
  bool localActive() const { return m_local; }
  bool localPlaying() const { return m_localPlaying; }
  bool sourceFailed() const { return m_sourceFailed; }
  QString localName() const { return m_localName; }
  QUrl localFileUrl() const { std::lock_guard l(m_mutex); return m_network ? QUrl() : QUrl::fromLocalFile(m_file); }
  int durationMs() const { return m_durationMs; }
  int positionMs() const { return m_positionMs; }
  bool hrtfAvailable() const { return m_hrtfAvailable; }
  bool hrtfActive() const { return m_hrtfActive; }
  bool iamfAvailable() const;
  bool sourceIamf() const { return m_iamf; }
  bool visualizerActive() const { return m_visualizer.load(); }
  void setVisualizerActive(bool active) { m_visualizer.store(active); }
  uintptr_t prepareSpotifyPipe();
  void spotifyStarted();
  void stopSpotify();
  Q_INVOKABLE void setSetting(const QString &, const QVariant &);
  Q_INVOKABLE void setEqBand(int, double);
  Q_INVOKABLE void applyPreset(const QString &);
  Q_INVOKABLE void reset();
  Q_INVOKABLE void resetEqualizer();
  Q_INVOKABLE void openFile(const QUrl &);
  Q_INVOKABLE void openStream(const QUrl &, const QString &title);
  Q_INVOKABLE void returnToSpotify();
  Q_INVOKABLE void toggleLocalPlay();
  Q_INVOKABLE void seekLocal(int);
  Q_INVOKABLE void setLocalVolume(int);
  void flushSpotify();
  void setSpotifyPaused(bool);
  bool spotifyPaused() const { std::lock_guard lock(m_mutex); return m_pause; }
  uint64_t spotifyFramesRead() const { return m_spotifyFrames.load(); }
signals:
  void settingsChanged();
  void diagnosticsChanged();
  void localChanged();
  void positionChanged();
  void localModeRequested();
  void spotifyModeRequested();
  void streamConfigurationChanged();
  void backendFailed(const QString &);
  void spotifyInputEnded();

private:
  void changed();
  void run();
  void postStatus(const QString &, bool hrtf = false, bool active = false);
  mutable std::mutex m_mutex;
  AudioSettings m_settings;
  QString m_settingsPath, m_cacheDir;
  std::shared_ptr<NativePcmPipe> m_pipe;
  QString m_file;
  bool m_network = false;
  int m_revision = 0;
  bool m_pause = false, m_flush = false;
  double m_seek = -1;
  float m_volume = 0.6f;
  std::atomic<uint64_t> m_generation{0};
  std::atomic<bool> m_quit{false};
  std::atomic<bool> m_visualizer{false};
  std::atomic<uint64_t> m_spotifyFrames{0};
  bool m_failed = false;
  std::thread m_thread;
  QString m_diagnostics = QStringLiteral("Aguardando áudio local");
  QString m_localName;
  QVariantMap m_metrics;
  bool m_local = false, m_localPlaying = false, m_hrtfAvailable = false,
       m_iamf = false;
  bool m_sourceFailed = false;
  bool m_hrtfActive = false;
  int m_durationMs = 0, m_positionMs = 0;
};
} // namespace tonal::audio
