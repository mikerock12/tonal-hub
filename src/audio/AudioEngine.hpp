#pragma once
#include "AudioSettings.hpp"
#include "playback/TransitionMixer.hpp"
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
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
  // Transições da fila: só com o PCM do Tonalify passando por este motor.
  bool spotifyPipeActive() const { std::lock_guard lock(m_mutex); return m_pipe != nullptr && !m_failed; }
  // `upcomingMs`: estimativa de quanto da faixa atual o librespot ainda não
  // entregou; `outroMs` = fim dela com o efeito; `introMs` = começo da
  // próxima injetado na mescla. O fim de A é sempre o marcador do librespot
  // (sem ele não há efeito). Retorna o id do pedido (0: recusado aqui); o
  // aceite, a mescla e o resultado chegam depois em transitionTrace().
  uint64_t armTransition(int effect, int upcomingMs, int outroMs, int introMs, bool trimSilence = false);
  // Último pedido e o que a transição realmente fez (ids, fase, motivo,
  // quadros de A e B na mescla). Seguro de qualquer thread.
  QVariantMap transitionTrace() const;
  // Começos de faixa marcados pelo librespot neste pipe: identifica cada
  // ocorrência no PCM, inclusive a mesma música duas vezes seguidas.
  uint64_t spotifyTrackSequence() const { return m_trackSequence.load(); }
  // Posição da faixa atual no PCM já lido, contada desde o marcador de começo
  // de faixa do librespot. -1 quando desconhecida (sem marcador ou após seek).
  int spotifyTrackPositionMs() const { return m_trackPositionMs.load(); }
  // O librespot deste pipe já marcou algum começo de faixa.
  bool spotifyTrackMarkers() const { return m_trackMarkers.load(); }
  // Quantas transições tiveram o fim de A fixado pelo marcador (diagnóstico).
  int transitionBoundaries() const { return m_transitionBoundaries.load(); }
  int spotifyTransportEvents() const { return m_transportEvents.load(); }
  // Áudio entregue pelo motor e ainda não tocado (pré-rolagem das transições).
  int transitionLatencyMs() const { return m_transitionLatencyMs.load(); }
  // O volume do Spotify é aplicado no librespot, antes do buffer: durante o
  // atraso da pré-rolagem o motor compensa para a mudança soar na hora.
  void noteSpotifyVolumeChange(int fromPercent, int toPercent);
  void cancelTransition();
  // 0 parado, 1 lendo o fim da faixa, 2 mesclando.
  int transitionState() const { return m_transitionState.load(); }

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
  void spotifyInputInvalid(const QString &reason);

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
  int m_transitionEffect = -1, m_transitionUpcomingMs = 0, m_transitionOutroMs = 0, m_transitionIntroMs = 0;
  bool m_transitionTrimSilence = false;
  std::atomic<int> m_trackPositionMs{-1};
  std::atomic<bool> m_trackMarkers{false};
  std::atomic<uint64_t> m_trackSequence{0};
  std::atomic<int> m_transitionBoundaries{0};
  std::atomic<int> m_transportEvents{0};
  uint64_t m_transitionRequest = 0, m_transitionNextId = 0;  // sob m_mutex
  mutable std::mutex m_traceMutex;
  tonal::playback::TransitionTrace m_trace;                 // sob m_traceMutex
  uint64_t m_traceRequest = 0, m_traceRejected = 0;         // sob m_traceMutex
  int m_traceRejection = 0;
  std::vector<std::pair<int, int>> m_volumeNotes;
  std::atomic<int> m_transitionLatencyMs{0};
  bool m_transitionCancel = false;
  std::atomic<int> m_transitionState{0};
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
