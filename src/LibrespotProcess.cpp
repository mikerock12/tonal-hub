#include "LibrespotProcess.h"
#include "audio/AudioEngine.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
// Erros sempre aparecem; o log completo só com
// QT_LOGGING_RULES="tonal.librespot.debug=true".
Q_LOGGING_CATEGORY(lcLibrespot, "tonal.librespot", QtInfoMsg)
} // namespace

LibrespotProcess::LibrespotProcess(QObject *parent, const QString &storageRoot)
    : QObject(parent), m_storageRoot(storageRoot) {
  QString host = QSysInfo::machineHostName();
  if (host.isEmpty())
    host = QStringLiteral("PC");
  m_deviceName = QStringLiteral("Tonal @ %1").arg(host);

  QSettings s;
  m_bitrate = s.value(QStringLiteral("librespot/bitrate"), 320).toInt();
  m_exe = s.value(QStringLiteral("librespot/path")).toString();
  if (m_exe.isEmpty() || !QFileInfo::exists(m_exe))
    m_exe = findExecutable();

  m_process.setProcessChannelMode(QProcess::SeparateChannels);
#ifdef Q_OS_WIN
  // Garante que nenhuma janela de console apareça.
  m_process.setCreateProcessArgumentsModifier(
      [this](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
        if (m_pcmWriter) {
          args->inheritHandles = true;
          args->startupInfo->dwFlags |= STARTF_USESTDHANDLES;
          args->startupInfo->hStdOutput = reinterpret_cast<HANDLE>(m_pcmWriter);
        }
      });
#endif
  connect(&m_process, &QProcess::readyReadStandardError, this,
          &LibrespotProcess::onReadyRead);
  connect(&m_process, &QProcess::readyReadStandardOutput, this,
          &LibrespotProcess::onReadyRead);
  connect(&m_process, &QProcess::started, this, [this] {
    if (m_audio && m_pcmWriter)
      m_audio->spotifyStarted();
    m_lastPcmFrames = 0;
    m_pcmProgressClock.start();
    if (m_pcmWriter)
      m_healthTimer.start();
    logDiagnostic("process started");
  });
  connect(&m_process, &QProcess::finished, this, &LibrespotProcess::onFinished);
  connect(&m_process, &QProcess::started, this, &LibrespotProcess::attachToJob);
  connect(&m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart) {
              if (m_audio) {
                m_audio->spotifyStarted();
                m_audio->stopSpotify();
              }
              m_pcmWriter = 0;
              m_healthTimer.stop();
              logDiagnostic("process failed to start");
              setState(NotFound, tr("Não consegui iniciar o librespot: %1")
                                     .arg(m_process.errorString()));
            }
          });

  m_restartTimer.setSingleShot(true);
  connect(&m_restartTimer, &QTimer::timeout, this, &LibrespotProcess::start);
  m_terminateTimer.setSingleShot(true);
  connect(&m_terminateTimer, &QTimer::timeout, this, [this] {
    if (m_restartRequested && m_process.state() != QProcess::NotRunning)
      m_process.kill();
  });
  m_healthTimer.setInterval(1000);
  connect(&m_healthTimer, &QTimer::timeout, this,
          &LibrespotProcess::checkPlaybackHealth);
}

LibrespotProcess::~LibrespotProcess() { stop(); }

void LibrespotProcess::setAudioEngine(tonal::audio::AudioEngine *engine) {
  m_audio = engine;
  connect(engine, &tonal::audio::AudioEngine::streamConfigurationChanged, this,
          [this] {
            if (m_process.state() != QProcess::NotRunning)
              restart();
          });
  connect(engine, &tonal::audio::AudioEngine::backendFailed, this,
          [this](const QString &) {
            if (m_process.state() != QProcess::NotRunning)
              beginRestart(true, "audio output failure");
          });
  connect(engine, &tonal::audio::AudioEngine::spotifyInputEnded, this, [this] {
    if (m_process.state() != QProcess::NotRunning)
      beginRestart(true, "PCM input closed unexpectedly");
  });
}

QString LibrespotProcess::findExecutable() const {
#ifdef Q_OS_WIN
  const QString name = QStringLiteral("librespot.exe");
#else
  const QString name = QStringLiteral("librespot");
#endif
  // 1) ao lado do Tonal.exe
  const QString local =
      QDir(QCoreApplication::applicationDirPath()).filePath(name);
  if (QFileInfo::exists(local))
    return local;
  // 2) no PATH
  const QString inPath =
      QStandardPaths::findExecutable(QStringLiteral("librespot"));
  if (!inPath.isEmpty())
    return inPath;
  // 3) onde o "cargo install librespot" coloca
  const QString cargo =
      QDir::home().filePath(QStringLiteral(".cargo/bin/") + name);
  if (QFileInfo::exists(cargo))
    return cargo;
  return {};
}

QString LibrespotProcess::cacheDir() const {
  const QString dir =
      (m_storageRoot.isEmpty() ? QStandardPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)
                               : m_storageRoot) +
      QStringLiteral("/librespot");
  QDir().mkpath(dir);
  return dir;
}

bool LibrespotProcess::hasCachedCredentials() const {
  return QFileInfo::exists(
      QDir(cacheDir()).filePath(QStringLiteral("credentials.json")));
}

void LibrespotProcess::setExecutablePath(const QString &path) {
  QString p = path;
  if (p.startsWith(QLatin1String("file:")))
    p = QUrl(p).toLocalFile();
  m_exe = p;
  QSettings().setValue(QStringLiteral("librespot/path"), m_exe);
  emit executablePathChanged();
  restart();
}

void LibrespotProcess::setBitrate(int kbps) {
  if (kbps != 96 && kbps != 160 && kbps != 320)
    kbps = 320;
  if (kbps == m_bitrate)
    return;
  m_bitrate = kbps;
  QSettings().setValue(QStringLiteral("librespot/bitrate"), m_bitrate);
  emit bitrateChanged();
  if (m_process.state() != QProcess::NotRunning)
    restart();
}

void LibrespotProcess::start() {
  if (m_audio && m_audio->localActive())
    return;
  if (m_process.state() != QProcess::NotRunning)
    return;
  m_restartTimer.stop();
  m_restartRequested = false;

  if (m_exe.isEmpty() || !QFileInfo::exists(m_exe))
    m_exe = findExecutable();
  if (m_exe.isEmpty()) {
    setState(NotFound, tr("librespot não encontrado. Rode \"cargo install "
                          "librespot --locked\" "
                          "ou coloque o librespot.exe ao lado do Tonal.exe."));
    return;
  }

  const QString cache = cacheDir();
  QStringList args{
      QStringLiteral("--name"),
      m_deviceName,
      QStringLiteral("--device-type"),
      QStringLiteral("computer"),
      QStringLiteral("--bitrate"),
      QString::number(m_bitrate),
      QStringLiteral("--cache"),
      cache,
      QStringLiteral("--system-cache"),
      cache,
      QStringLiteral("--cache-size-limit"),
      QStringLiteral("1G"),
      QStringLiteral("--disable-discovery"),
      QStringLiteral("--initial-volume"),
      QString::number(
          QSettings().value(QStringLiteral("player/volume"), 60).toInt()),
  };
  if (!m_audio || m_audio->normalizationEnabled())
    args << QStringLiteral("--enable-volume-normalisation");
  m_pcmWriter = 0;
  // OAuth prints text to stdout. Complete first-time login in the original
  // backend, then restart with cached credentials before accepting raw PCM on
  // stdout.
  m_deferPcm = m_audio && m_audio->advancedEnabled() && !hasCachedCredentials();
  if (m_audio && m_audio->advancedEnabled() && !m_deferPcm)
    m_pcmWriter = m_audio->prepareSpotifyPipe();
  if (m_pcmWriter)
    args << QStringLiteral("--backend") << QStringLiteral("pipe")
         << QStringLiteral("--format") << QStringLiteral("F32");

  if (!hasCachedCredentials()) {
    // Primeiro uso: o librespot faz o próprio login OAuth no navegador
    // (é o mesmo fluxo do app oficial) e salva credentials.json no cache.
    args << QStringLiteral("--enable-oauth");
  }

  m_stopping = false;
  m_expectedPlayback = false;
  m_buffer.clear();
  auto environment = QProcessEnvironment::systemEnvironment();
  // State detection must keep working with a user's RUST_LOG=warn/error/off.
  // librespot-core 0.8 emits its authentication/ready event at INFO.
  QString logging =
      environment.value(QStringLiteral("RUST_LOG"), QStringLiteral("info")) +
      QStringLiteral(",librespot_core::session=info,librespot_connect=info");
  if (m_pcmWriter) {
    // Transport commands are emitted at debug level by the pinned librespot.
    // They go to stderr, separate from raw PCM, and cover Spotify Connect
    // controls too.
    logging += QStringLiteral(",librespot_playback::player=debug");
  }
  environment.insert(QStringLiteral("RUST_LOG"), logging);
  m_process.setProcessEnvironment(environment);
  setState(Starting, tr("Iniciando o motor de áudio..."));
  m_process.start(m_exe, args);
}

void LibrespotProcess::stop() {
  m_restartTimer.stop();
  m_terminateTimer.stop();
  m_healthTimer.stop();
  m_restartRequested = false;
  m_expectedPlayback = false;
  m_autoRestartBlocked = false;
  m_crashCount = 0;
  m_recoveryWindow.invalidate();
  m_stopping = true;
  // Primeiro pede para sair com Ctrl+C: o librespot avisa o Spotify que o
  // dispositivo saiu. Morto à força, a sessão antiga continua no servidor e,
  // quando ela expira (~2 min), o servidor derruba a sessão nova do mesmo
  // dispositivo.
  if (m_process.state() != QProcess::NotRunning &&
      (!requestGracefulExit() || !m_process.waitForFinished(3000))) {
    // librespot é um app de console: no Windows terminate() (WM_CLOSE) é
    // ignorado.
    m_process.kill();
    m_process.waitForFinished(2000);
  }
#ifdef Q_OS_WIN
  // Só agora, com o librespot já fora, o Tonal volta a aceitar Ctrl+C (ver
  // requestGracefulExit).
  SetConsoleCtrlHandler(nullptr, FALSE);
#endif
  setState(Stopped);
  m_pcmWriter = 0;
  if (m_audio)
    m_audio->stopSpotify();
  m_stopping = false;
}

bool LibrespotProcess::requestGracefulExit() {
#ifdef Q_OS_WIN
  // O Tonal é app GUI, sem console próprio: ele se anexa ao console oculto do
  // librespot (CREATE_NO_WINDOW), manda Ctrl+C para os processos desse console
  // e se desanexa. Enquanto isso o Tonal ignora o Ctrl+C, que também chega para
  // ele. O "ignorar" é herdado por processos filhos, por isso stop() só o
  // desfaz depois que o librespot terminou.
  if (!AttachConsole(static_cast<DWORD>(m_process.processId())))
    return false;
  SetConsoleCtrlHandler(nullptr, TRUE);
  const bool sent = GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
  FreeConsole();
  return sent;
#else
  return false; // fora do Windows fica o kill(), como antes
#endif
}

void LibrespotProcess::restart() {
  m_crashCount = 0;
  m_recoveryWindow.invalidate();
  m_autoRestartBlocked = false;
  beginRestart(false, "requested restart");
}

void LibrespotProcess::beginRestart(bool automatic, const QString &reason) {
  if (m_stopping || m_restartRequested || m_restartTimer.isActive())
    return;
  if (automatic) {
    if (m_autoRestartBlocked)
      return;
    if (!m_recoveryWindow.isValid() || m_recoveryWindow.elapsed() > 300000) {
      m_recoveryWindow.start();
      m_crashCount = 0;
    }
    ++m_crashCount;
    m_autoRestartBlocked = m_crashCount > 3;
  }
  // Capture intent before changing Ready/local-device state or disposing PCM.
  emit sessionLost();
  m_expectedPlayback = false;
  m_healthTimer.stop();
  m_restartDelay = automatic ? m_restartBaseMs * m_crashCount : 0;
  logDiagnostic(QString("recovery reason=%1 attempt=%2 blocked=%3")
                    .arg(reason)
                    .arg(m_crashCount)
                    .arg(m_autoRestartBlocked));
  setState(
      m_autoRestartBlocked ? Crashed : Starting,
      m_autoRestartBlocked
          ? tr("O motor falhou repetidamente. Use Reiniciar motor de áudio.")
          : tr("Reconectando o motor de áudio…"));
  if (m_autoRestartBlocked) {
    if (m_process.state() != QProcess::NotRunning)
      m_process.kill();
    return;
  }
  m_restartRequested = true;
  if (m_process.state() == QProcess::NotRunning) {
    m_restartRequested = false;
    m_pcmWriter = 0;
    if (m_audio)
      m_audio->stopSpotify();
    m_restartTimer.start(m_restartDelay);
  } else {
    // Recovery never waits for a child on the GUI thread.
    if (!requestGracefulExit())
      m_process.kill();
    else
      m_terminateTimer.start(1500);
  }
}

void LibrespotProcess::setPlaybackExpected(bool playing) {
  if (playing != m_expectedPlayback) {
    m_expectedPlayback = playing;
    m_pcmProgressClock.start();
    m_lastPcmFrames = m_audio ? m_audio->spotifyFramesRead() : 0;
  }
}

void LibrespotProcess::checkPlaybackHealth() {
  if (!m_audio || !m_pcmWriter || !m_expectedPlayback || m_state != Ready ||
      m_process.state() != QProcess::Running || m_audio->localActive())
    return;
  const auto frames = m_audio->spotifyFramesRead();
  if (frames != m_lastPcmFrames) {
    m_lastPcmFrames = frames;
    m_pcmProgressClock.start();
  } else if (m_pcmProgressClock.isValid() &&
             m_pcmProgressClock.elapsed() >= m_stallTimeoutMs)
    beginRestart(true, "playback expected but no PCM progress");
}

QString LibrespotProcess::safeLog(QString line) {
  line.replace(QRegularExpression("https?://[^\\s<>\\\"]+"), "[URL]");
  line.replace(QRegularExpression("(?i)(bearer\\s+)[^\\s,;]+"),
               "\\1[redacted]");
  line.replace(QRegularExpression(
                   "(?i)((?:access_token|refresh_token|password|authorization|"
                   "code_verifier)[\\\"']?\\s*[:=]\\s*[\\\"']?)[^\\s,;\\\"']+"),
               "\\1[redacted]");
  return line.left(4096);
}

void LibrespotProcess::logDiagnostic(const QString &event) {
  const QString root =
      (m_storageRoot.isEmpty() ? QStandardPaths::writableLocation(
                                     QStandardPaths::AppLocalDataLocation)
                               : m_storageRoot) +
      "/diagnostics";
  if (!QDir().mkpath(root))
    return;
  const QString path = root + "/playback.log";
  if (QFileInfo(path).size() > 256 * 1024) {
    QFile::remove(path + ".1");
    QFile::rename(path, path + ".1");
  }
  QFile file(path);
  if (file.open(QIODevice::Append))
    file.write((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) +
                " " + safeLog(event) + "\n")
                   .toUtf8());
}

void LibrespotProcess::resetLogin() {
  stop();
  QFile::remove(QDir(cacheDir()).filePath(QStringLiteral("credentials.json")));
  m_crashCount = 0;
  start();
}

void LibrespotProcess::onReadyRead() {
  m_buffer += m_process.readAllStandardError();
  if (!m_pcmWriter)
    m_buffer += m_process.readAllStandardOutput();
  if (m_buffer.size() > 65536) {
    m_buffer = m_buffer.right(65536);
    logDiagnostic("oversized diagnostic line truncated");
  }
  int idx;
  while ((idx = m_buffer.indexOf('\n')) >= 0) {
    const QString line = QString::fromUtf8(m_buffer.left(idx)).trimmed();
    m_buffer.remove(0, idx + 1);
    if (!line.isEmpty())
      handleLine(line);
  }
}

void LibrespotProcess::handleLine(const QString &line) {
  m_log << safeLog(line);
  if (m_log.size() > 300)
    m_log.removeFirst();

  const bool isError = line.contains(QLatin1String(" ERROR "));
  if (isError)
    qCWarning(lcLibrespot).noquote() << safeLog(line);
  else
    qCDebug(lcLibrespot).noquote() << safeLog(line);
  if (isError || line.contains("panicked at"))
    logDiagnostic(line);
  if (m_stopping || m_restartRequested || m_autoRestartBlocked)
    return;

  // Login interativo do librespot: "Browse to:
  // https://accounts.spotify.com/authorize?..." O próprio librespot (0.8) já
  // abre essa URL no navegador antes de imprimir a linha; abrir de novo aqui
  // criaria uma segunda aba. Só atualizamos o estado.
  if (line.contains(QLatin1String("Browse to:"))) {
    setState(NeedsLogin,
             tr("Autorize o motor de áudio na aba que abriu no navegador."));
    return;
  }

  if (line.contains(QLatin1String("Authenticated as"))) {
    setState(Ready, tr("Pronto para tocar"));
    logDiagnostic("authenticated; engine ready");
    if (m_deferPcm && hasCachedCredentials()) {
      m_deferPcm = false;
      QTimer::singleShot(100, this, &LibrespotProcess::restart);
    }
    return;
  }

  // O librespot só se reconecta sozinho quando o Spirc cai. Se cair só a
  // conexão com o AP (rede, hibernação, servidor), ele fica "meio vivo":
  // continua listado como dispositivo, mas o próximo play falha sem a chave de
  // áudio. Reiniciamos na hora.
  if (line.contains(QLatin1String("Connection to server closed"))) {
    beginRestart(true, "Spotify server connection closed");
    return;
  }
  // Aqui o próprio librespot abre uma sessão nova; só avisamos que a reprodução
  // se perdeu. O "Authenticated as" seguinte volta o estado para Ready.
  if (line.contains(QLatin1String("Spirc shut down unexpectedly"))) {
    beginRestart(true, "Spotify Connect session stopped unexpectedly");
    return;
  }
  if (line.contains("panicked at") ||
      (isError && (line.contains("Sink error", Qt::CaseInsensitive) ||
                   line.contains("audio sink", Qt::CaseInsensitive)))) {
    beginRestart(true, "fatal audio sink/player error");
    return;
  }

  if (line.contains(QLatin1String("librespot_playback::player")) ||
      line.contains(QLatin1String("librespot_connect"))) {
    if (line.contains("command=Pause") || line.contains("command=Stop"))
      setPlaybackExpected(false);
    else if (line.contains("command=Play"))
      setPlaybackExpected(true);
    else if (line.contains("command=Load {")) {
      setPlaybackExpected(!line.contains("play: false"));
      m_pcmProgressClock.start();
      logDiagnostic("transport load");
    }
    if (m_audio) {
      // Exact player commands: "Preload" and connection metadata aren't
      // transport events.
      if (line.contains(QLatin1String("command=Pause")) ||
          line.contains(QLatin1String("command=Stop")))
        m_audio->setSpotifyPaused(true);
      else if (line.contains(QLatin1String("command=Play")))
        m_audio->setSpotifyPaused(false);
      else if (line.contains(QLatin1String("command=Load {")))
        m_audio->setSpotifyPaused(line.contains(QLatin1String("play: false")));
      if (line.contains(QLatin1String("command=Seek(")) ||
          line.contains(QLatin1String("Loading <")))
        m_audio->flushSpotify();
    }
    if (line.contains(QLatin1String("Loading <")) ||
        line.contains(QLatin1String(") loaded")) ||
        line.contains(QLatin1String("Pause")) ||
        line.contains(QLatin1String("Play"))) {
      emit playbackEvent();
    }
  }

  if (isError && m_state == Starting)
    setState(Starting, line.section(QLatin1Char(']'), 1).trimmed());
}

void LibrespotProcess::onFinished(int exitCode, QProcess::ExitStatus status) {
  onReadyRead();
  m_terminateTimer.stop();
  m_healthTimer.stop();
  logDiagnostic(QString("process exit=%1 crash=%2 requested=%3")
                    .arg(exitCode)
                    .arg(status == QProcess::CrashExit)
                    .arg(m_stopping || m_restartRequested));
#ifdef Q_OS_WIN
  SetConsoleCtrlHandler(nullptr, FALSE);
#endif
  if (m_stopping) {
    return;
  }
  if (m_restartRequested) {
    m_restartRequested = false;
    if (m_audio)
      m_audio->stopSpotify();
    m_pcmWriter = 0;
    m_restartTimer.start(m_restartDelay);
    return;
  }
  if (!m_autoRestartBlocked)
    beginRestart(true, "unexpected process exit");
  else {
    if (m_audio)
      m_audio->stopSpotify();
    m_pcmWriter = 0;
  }
}

void LibrespotProcess::setState(State s, const QString &text) {
  if (m_state == s && m_statusText == text)
    return;
  m_state = s;
  m_statusText = text;
  emit stateChanged();
}

void LibrespotProcess::attachToJob() {
#ifdef Q_OS_WIN
  // Job Object com KILL_ON_JOB_CLOSE: se o Tonal fechar ou travar,
  // o Windows encerra o librespot junto. Nada de processo órfão.
  static HANDLE job = [] {
    HANDLE j = CreateJobObjectW(nullptr, nullptr);
    if (j) {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
      info.BasicLimitInformation.LimitFlags =
          JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(j, JobObjectExtendedLimitInformation, &info,
                              sizeof(info));
    }
    return j;
  }();
  if (!job)
    return;
  HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
                            static_cast<DWORD>(m_process.processId()));
  if (proc) {
    AssignProcessToJobObject(job, proc);
    CloseHandle(proc);
  }
#endif
}
