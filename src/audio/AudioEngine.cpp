#include "AudioEngine.hpp"
#include "AudioDiagnostics.hpp"
#include "DspGraph.hpp"
#include "FfmpegDecoder.hpp"
#include "IamfDecoderAdapter.hpp"
#include "LoudnessAnalyzer.hpp"
#include "NativePcmPipe.hpp"
#include "OpenALOutput.hpp"
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/log.h>
}
namespace tonal::audio {
AudioEngine::AudioEngine(QObject *parent, const QString &storageRoot)
    : QObject(parent) {
  m_settingsPath =
      (storageRoot.isEmpty()
           ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           : storageRoot) +
      "/audio.json";
  m_cacheDir =
      (storageRoot.isEmpty()
           ? QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
           : storageRoot) +
      "/loudness-v1";
  m_settings = AudioSettings::load(m_settingsPath);
  m_volume = std::clamp(QSettings().value("player/volume", 60).toInt() / 100.f,
                        0.f, 1.f);
  av_log_set_level(AV_LOG_ERROR);
  m_thread = std::thread([this] { run(); });
}
AudioEngine::~AudioEngine() {
  m_quit = true;
  ++m_generation;
  if (m_thread.joinable())
    m_thread.join();
}
QVariantMap AudioEngine::settings() const {
  std::lock_guard l(m_mutex);
  return m_settings.json().toVariantMap();
}
bool AudioEngine::advancedEnabled() const {
  std::lock_guard l(m_mutex);
  return m_settings.enabled && !m_failed;
}
bool AudioEngine::normalizationEnabled() const {
  std::lock_guard l(m_mutex);
  return m_settings.normalization;
}
bool AudioEngine::iamfAvailable() const {
  return IamfDecoderAdapter::available();
}
void AudioEngine::changed() {
  AudioSettings copy;
  {
    std::lock_guard l(m_mutex);
    m_settings.validate();
    ++m_revision;
    copy = m_settings;
  }
  if (!copy.save(m_settingsPath))
    postStatus(tr("Não foi possível salvar as configurações de áudio"));
  emit settingsChanged();
}
void AudioEngine::setSetting(const QString &key, const QVariant &v) {
  const bool stream = key == "enabled" || key == "normalization";
  {
    std::lock_guard l(m_mutex);
    auto j = m_settings.json();
    if (!j.contains(key) || key == "schemaVersion" || key == "eq" ||
        key == "preset")
      return;
    if (v.metaType().id() == QMetaType::Double && !std::isfinite(v.toDouble()))
      return;
    j.insert(key, QJsonValue::fromVariant(v));
    m_settings = AudioSettings::fromJson(j);
    if (key == "enabled")
      m_failed = false;
  }
  changed();
  if (stream && !m_local)
    emit streamConfigurationChanged();
}
void AudioEngine::setEqBand(int i, double db) {
  if (i < 0 || i >= 10 || !std::isfinite(db))
    return;
  {
    std::lock_guard l(m_mutex);
    m_settings.eq[i] = std::clamp(db, -12., 12.);
    m_settings.eqEnabled = true;
    m_settings.preset = "Personalizada";
  }
  changed();
}
void AudioEngine::applyPreset(const QString &name) {
  {
    std::lock_guard l(m_mutex);
    m_settings.applyPreset(name);
  }
  changed();
}
void AudioEngine::reset() {
  bool stream;
  {
    std::lock_guard l(m_mutex);
    stream = !m_settings.enabled || !m_settings.normalization;
    m_settings = {};
    m_failed = false;
  }
  changed();
  if (stream && !m_local)
    emit streamConfigurationChanged();
}
void AudioEngine::resetEqualizer() {
  {
    std::lock_guard l(m_mutex);
    m_settings.eq = {};
    m_settings.eqEnabled = false;
    m_settings.preset = "Flat";
  }
  changed();
}
uintptr_t AudioEngine::prepareSpotifyPipe() {
  if (!advancedEnabled() || m_local)
    return 0;
  auto pipe = std::make_shared<NativePcmPipe>();
  if (!pipe->valid())
    return 0;
  {
    std::lock_guard l(m_mutex);
    m_pipe = pipe;
    m_file.clear();
    m_pause = false;
    m_flush = false;
    m_seek = -1;
    m_spotifyFrames = 0;
    ++m_generation;
  }
  return pipe->writer();
}
void AudioEngine::spotifyStarted() {
  std::lock_guard l(m_mutex);
  if (m_pipe)
    m_pipe->closeWriter();
}
void AudioEngine::stopSpotify() {
  std::lock_guard l(m_mutex);
  m_pipe.reset();
  if (!m_local)
    ++m_generation;
}
void AudioEngine::flushSpotify() {
  std::lock_guard l(m_mutex);
  if (!m_local)
    m_flush = true;
}
void AudioEngine::setSpotifyPaused(bool paused) {
  std::lock_guard l(m_mutex);
  if (!m_local)
    m_pause = paused;
}
void AudioEngine::openFile(const QUrl &url) {
  const auto path = url.isLocalFile() ? url.toLocalFile() : url.toString();
  if (!QFileInfo(path).isFile()) {
    postStatus(tr("Arquivo não encontrado"));
    return;
  }
  if (!advancedEnabled()) {
    setSetting("enabled", true);
  }
  m_local = true;
  m_sourceFailed = false;
  m_localPlaying = true;
  m_localName = QFileInfo(path).fileName();
  m_durationMs = 0;
  m_positionMs = 0;
  m_iamf = false;
  emit localChanged();
  emit localModeRequested();
  {
    std::lock_guard l(m_mutex);
    m_pipe.reset();
    m_file = path;
    m_network = false;
    m_pause = false;
    m_seek = -1;
    ++m_generation;
  }
}
void AudioEngine::openStream(const QUrl &url, const QString &title) {
  if (!url.isValid() || url.scheme() != "https" || url.host().isEmpty() ||
      !url.userInfo().isEmpty()) {
    postStatus(tr("Endereço de streaming inválido"));
    return;
  }
  m_local = true;
  m_sourceFailed = false;
  m_localPlaying = true;
  m_localName = title;
  m_durationMs = m_positionMs = 0;
  m_iamf = false;
  emit localModeRequested();
  {
    std::lock_guard l(m_mutex);
    m_pipe.reset();
    m_file = url.toString(QUrl::FullyEncoded);
    m_network = true;
    m_pause = false;
    m_seek = -1;
    ++m_generation;
  }
  emit localChanged();
}
void AudioEngine::returnToSpotify() {
  m_local = false;
  m_localPlaying = false;
  m_iamf = false;
  {
    std::lock_guard l(m_mutex);
    m_file.clear();
    m_pipe.reset();
    ++m_generation;
  }
  emit localChanged();
  emit spotifyModeRequested();
}
void AudioEngine::toggleLocalPlay() {
  if (!m_local)
    return;
  m_localPlaying = !m_localPlaying;
  {
    std::lock_guard l(m_mutex);
    m_pause = !m_localPlaying;
    if (m_localPlaying && m_durationMs > 0 && m_positionMs >= m_durationMs)
      m_seek = 0;
  }
  emit localChanged();
}
void AudioEngine::seekLocal(int ms) {
  if (!m_local)
    return;
  {
    std::lock_guard l(m_mutex);
    m_positionMs = std::clamp(ms, 0, m_durationMs);
    m_seek = m_positionMs / 1000.;
  }
  emit positionChanged();
}
void AudioEngine::setLocalVolume(int percent) {
  std::lock_guard l(m_mutex);
  m_volume = std::clamp(percent, 0, 100) / 100.f;
}
void AudioEngine::postStatus(const QString &s, bool hrtf, bool active) {
  QMetaObject::invokeMethod(
      this,
      [this, s, hrtf, active] {
        if (m_diagnostics != s || m_hrtfAvailable != hrtf ||
            m_hrtfActive != active) {
          m_diagnostics = s;
          m_hrtfAvailable = hrtf;
          m_hrtfActive = active;
          emit diagnosticsChanged();
        }
      },
      Qt::QueuedConnection);
}
void AudioEngine::run() {
  std::unique_ptr<IAudioDecoder> decoder;
  std::shared_ptr<NativePcmPipe> pipe;
  OpenALOutput output;
  std::unique_ptr<DspGraph> dsp;
  AudioFormat format;
  bool opened = false, ended = false, local = false, drained = false;
  uint64_t generation = ~uint64_t(0);
  int revision = -1;
  int64_t pts = 0;
  float volume = 1;
  Loudness loudness;
  double replayGain = 0, logicalPosition = 0;
  bool hasReplayGain = false;
  auto lastReport = std::chrono::steady_clock::now();
  AudioDiagnostics counters;
  QString file, error;
  AudioSettings settings;
  auto rebuild = [&](double gain) {
    if (dsp) {
      dsp->update(settings, gain);
      return true;
    }
    dsp = std::make_unique<DspGraph>();
    return dsp->configure(format, settings, gain, error);
  };
  auto fail = [&](const QString &why) {
    postStatus(why);
    opened = false;
    output.close();
    decoder.reset();
    dsp.reset();
    pipe.reset();
    ended = true;
    const auto failedGeneration = generation;
    QMetaObject::invokeMethod(
        this,
        [this, why, local, failedGeneration] {
          if (m_generation != failedGeneration)
            return;
          if (local) {
            m_localPlaying = false;
            m_sourceFailed = true;
            emit localChanged();
          } else {
            {
              std::lock_guard l(m_mutex);
              m_failed = true;
            }
            emit backendFailed(why);
          }
        },
        Qt::QueuedConnection);
  };
  while (!m_quit) {
    AudioSettings next;
    std::shared_ptr<NativePcmPipe> requested;
    QString path;
    int rev;
    bool paused, flush, network;
    double seek;
    float target;
    uint64_t serial;
    {
      std::lock_guard l(m_mutex);
      next = m_settings;
      requested = m_pipe;
      path = m_file;
      network = m_network;
      rev = m_revision;
      paused = m_pause;
      flush = std::exchange(m_flush, false);
      seek = std::exchange(m_seek, -1.);
      target = m_volume;
      serial = m_generation.load();
    }
    if (serial != generation) {
      generation = serial;
      counters = {};
      output.close();
      opened = false;
      decoder.reset();
      dsp.reset();
      pipe = requested;
      drained = false;
      local = !path.isEmpty();
      file = path;
      pts = 0;
      ended = false;
      revision = -1;
      loudness = {};
      hasReplayGain = false;
      settings = next;
      volume = local ? target : 1.f;
      logicalPosition = 0;
      if (local) {
        postStatus(network ? tr("Carregando stream com buffer…")
                           : tr("Abrindo arquivo local…"));
        if (!network && path.endsWith(".iamf", Qt::CaseInsensitive))
          decoder = std::make_unique<IamfDecoderAdapter>(
              [&] { return m_quit || m_generation != generation; });
        else
          decoder =
              std::make_unique<FfmpegDecoder>(network, [this, generation] {
                return m_quit || m_generation != generation;
              });
        if (!decoder->open(path, error)) {
          fail(error);
          continue;
        }
        format = decoder->format();
        hasReplayGain = decoder->hasReplayGain();
        replayGain = decoder->replayGainDb();
        if (!hasReplayGain && !decoder->analysisPath().isEmpty()) {
          postStatus(tr("Medindo loudness da faixa (cache local)…"));
          loudness = LoudnessAnalyzer::analyze(
              decoder->analysisPath(), m_cacheDir,
              [&] { return m_quit || m_generation != generation; });
        }
        if (m_generation != generation)
          continue;
        const int duration = int(decoder->duration() * 1000);
        const bool binaural = format.binaural;
        QMetaObject::invokeMethod(
            this,
            [this, duration, generation, binaural] {
              if (m_generation == generation) {
                m_durationMs = duration;
                m_iamf = binaural;
                {
                  std::lock_guard l(m_mutex);
                  if (binaural)
                    m_settings.spatial = "iamf";
                  else if (m_settings.spatial == "iamf")
                    m_settings.spatial = "original";
                }
                changed();
                emit localChanged();
              }
            },
            Qt::QueuedConnection);
      } else {
        format = {};
      }
    }
    if (ended && decoder && seek >= 0)
      ended = false;
    if ((!pipe && !decoder) || ended) {
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      continue;
    }
    const bool spatialChanged =
        revision >= 0 &&
        (settings.spatial != next.spatial || settings.enabled != next.enabled);
    settings = next;
    if (opened && spatialChanged &&
        !output.setHrtf(settings.enabled && (settings.spatial == "hrtf" || settings.spatial == "hrtf-speakers"),
                        error)) {
      fail(error);
      continue;
    }
    if (!opened || spatialChanged) {
      if (!opened &&
          !output.open(format, settings.enabled && (settings.spatial == "hrtf" || settings.spatial == "hrtf-speakers"),
                       error)) {
        fail(error);
        continue;
      }
      opened = true;
      output.pause(paused, local);
      postStatus(output.description() +
                     QString(" · FFmpeg %1").arg(av_version_info()),
                 output.hrtfSupported() && !format.binaural, output.hrtf());
    }
    if (!output.connected()) {
      output.close();
      opened = false;
      postStatus(tr("Dispositivo removido; tentando reconectar…"));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }
    if (flush) {
      output.clear();
      if (pipe)
        pipe->discard();
      dsp.reset();
      drained = false;
      revision = -1;
    }
    if (seek >= 0 && decoder) {
      output.clear();
      if (!decoder->seek(seek, error)) {
        postStatus(error);
      } else {
        dsp.reset();
        drained = false;
        revision = -1;
        ended = false;
        logicalPosition = seek;
      }
    }
    if (rev != revision) {
      const double gain =
          local && settings.normalization
              ? (hasReplayGain
                     ? std::clamp(replayGain, -24., 12.)
                     : LoudnessAnalyzer::gain(loudness, settings.targetLufs,
                                              settings.ceilingDb))
              : 0;
      if (!rebuild(gain)) {
        fail(error);
        continue;
      }
      revision = rev;
    }
    output.pause(paused, local);
    target = local ? target : 1.f;
    volume += (target - volume) * 0.2f;
    output.setVolume(volume);
    output.collect();
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport > std::chrono::milliseconds(100)) {
      lastReport = now;
      if (output.queued())
        logicalPosition = output.position();
      const int position = int(logicalPosition * 1000);
      const auto status =
          output.description() + QString(" · fila %1 · underruns %2")
                                     .arg(output.queued())
                                     .arg(output.underruns());
      const auto metrics =
          counters.values(format, output.hrtf(), output.underruns());
      QMetaObject::invokeMethod(
          this,
          [this, position, generation, status, metrics] {
            if (m_generation != generation)
              return;
            if (m_local) {
              m_positionMs = position;
              emit positionChanged();
            }
            if (m_diagnostics != status || m_metrics != metrics) {
              m_diagnostics = status;
              m_metrics = metrics;
              emit diagnosticsChanged();
            }
          },
          Qt::QueuedConnection);
    }
    if ((paused && local) || output.queued() >= 6) {
      std::this_thread::sleep_for(std::chrono::milliseconds(3));
      continue;
    }
    AudioFrame in;
    int got;
    if (decoder)
      got = decoder->read(in, error);
    else {
      in.format = format;
      in.pts = pts;
      got = pipe->read(in.samples, 512);
      if (got > 0)
        m_spotifyFrames += uint64_t(got);
      pts += in.frames();
    }
    if (got < 0) {
      if (pipe) {
        output.close();
        opened = false;
        pipe.reset();
        ended = true;
        QMetaObject::invokeMethod(
            this,
            [this, generation] {
              if (m_generation == generation)
                emit spotifyInputEnded();
            },
            Qt::QueuedConnection);
      } else
        fail(error);
      continue;
    }
    if (got == 0) {
      if (pipe) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        continue;
      }
      if (!drained) {
        AudioFrame tail;
        if (!dsp->drain(tail, error)) {
          fail(error);
          continue;
        }
        if (!tail.samples.empty())
          output.enqueue(tail, error);
        drained = true;
      }
      // Drain OpenAL while allowing seek/source changes and shutdown.
      while (output.queued() && !m_quit && m_generation == generation) {
        bool interrupted;
        {
          std::lock_guard l(m_mutex);
          interrupted = m_seek >= 0 || m_pause;
        }
        if (interrupted)
          break;
        output.collect();
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
      }
      bool transportPending;
      {
        std::lock_guard l(m_mutex);
        transportPending = m_seek >= 0 || m_pause;
      }
      if (transportPending)
        continue;
      ended = true;
      QMetaObject::invokeMethod(
          this,
          [this, generation] {
            if (m_generation == generation) {
              m_localPlaying = false;
              m_positionMs = m_durationMs;
              emit localChanged();
              emit positionChanged();
            }
          },
          Qt::QueuedConnection);
      continue;
    }
    AudioFrame out;
    const auto dspStart = std::chrono::steady_clock::now();
    if (!dsp->process(in, out, error)) {
      if (pipe) {
        // Never pass damaged PCM through or keep a poisoned filter graph.
        // Reopen the stream with the existing bounded recovery policy; keep
        // the user's effects enabled instead of switching to a raw backend.
        output.close();
        opened = false;
        pipe.reset();
        dsp.reset();
        ended = true;
        const auto reason = error;
        QMetaObject::invokeMethod(this, [this, generation, reason] {
          if (m_generation == generation)
            emit spotifyInputInvalid(reason);
        }, Qt::QueuedConnection);
      } else
        fail(error);
      continue;
    }
    if(m_visualizer.load() && counters.blocks % 5 == 0)counters.visualize(out);
    else if(!m_visualizer.load() && !counters.waveform.isEmpty()){counters.waveform.clear();counters.spectrum.clear();}
    counters.observe(out, std::chrono::duration<double, std::micro>(
                              std::chrono::steady_clock::now() - dspStart)
                              .count());
    if (!output.enqueue(out, error)) {
      fail(error);
      continue;
    }
  }
}
} // namespace tonal::audio
