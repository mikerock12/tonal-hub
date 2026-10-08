#include "LibrespotProcess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

LibrespotProcess::LibrespotProcess(QObject *parent)
    : QObject(parent)
{
    QString host = QSysInfo::machineHostName();
    if (host.isEmpty())
        host = QStringLiteral("PC");
    m_deviceName = QStringLiteral("Tonal @ %1").arg(host);

    QSettings s;
    m_bitrate = s.value(QStringLiteral("librespot/bitrate"), 320).toInt();
    m_exe = s.value(QStringLiteral("librespot/path")).toString();
    if (m_exe.isEmpty() || !QFileInfo::exists(m_exe))
        m_exe = findExecutable();

    m_process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    // Garante que nenhuma janela de console apareça.
    m_process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    connect(&m_process, &QProcess::readyRead, this, &LibrespotProcess::onReadyRead);
    connect(&m_process, &QProcess::finished, this, &LibrespotProcess::onFinished);
    connect(&m_process, &QProcess::started, this, &LibrespotProcess::attachToJob);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            setState(NotFound, tr("Não consegui iniciar o librespot: %1").arg(m_process.errorString()));
    });

    m_restartTimer.setSingleShot(true);
    connect(&m_restartTimer, &QTimer::timeout, this, &LibrespotProcess::start);
}

LibrespotProcess::~LibrespotProcess()
{
    stop();
}

QString LibrespotProcess::findExecutable() const
{
#ifdef Q_OS_WIN
    const QString name = QStringLiteral("librespot.exe");
#else
    const QString name = QStringLiteral("librespot");
#endif
    // 1) ao lado do Tonal.exe
    const QString local = QDir(QCoreApplication::applicationDirPath()).filePath(name);
    if (QFileInfo::exists(local))
        return local;
    // 2) no PATH
    const QString inPath = QStandardPaths::findExecutable(QStringLiteral("librespot"));
    if (!inPath.isEmpty())
        return inPath;
    // 3) onde o "cargo install librespot" coloca
    const QString cargo = QDir::home().filePath(QStringLiteral(".cargo/bin/") + name);
    if (QFileInfo::exists(cargo))
        return cargo;
    return {};
}

QString LibrespotProcess::cacheDir() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                        + QStringLiteral("/librespot");
    QDir().mkpath(dir);
    return dir;
}

bool LibrespotProcess::hasCachedCredentials() const
{
    return QFileInfo::exists(QDir(cacheDir()).filePath(QStringLiteral("credentials.json")));
}

void LibrespotProcess::setExecutablePath(const QString &path)
{
    QString p = path;
    if (p.startsWith(QLatin1String("file:")))
        p = QUrl(p).toLocalFile();
    m_exe = p;
    QSettings().setValue(QStringLiteral("librespot/path"), m_exe);
    emit executablePathChanged();
    restart();
}

void LibrespotProcess::setBitrate(int kbps)
{
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

void LibrespotProcess::start()
{
    if (m_process.state() != QProcess::NotRunning)
        return;

    if (m_exe.isEmpty() || !QFileInfo::exists(m_exe))
        m_exe = findExecutable();
    if (m_exe.isEmpty()) {
        setState(NotFound, tr("librespot não encontrado. Rode \"cargo install librespot --locked\" "
                              "ou coloque o librespot.exe ao lado do Tonal.exe."));
        return;
    }

    const QString cache = cacheDir();
    QStringList args {
        QStringLiteral("--name"), m_deviceName,
        QStringLiteral("--device-type"), QStringLiteral("computer"),
        QStringLiteral("--bitrate"), QString::number(m_bitrate),
        QStringLiteral("--cache"), cache,
        QStringLiteral("--system-cache"), cache,
        QStringLiteral("--cache-size-limit"), QStringLiteral("1G"),
        QStringLiteral("--disable-discovery"),
        QStringLiteral("--enable-volume-normalisation"),
        QStringLiteral("--initial-volume"),
            QString::number(QSettings().value(QStringLiteral("player/volume"), 60).toInt()),
    };

    if (!hasCachedCredentials()) {
        // Primeiro uso: o librespot faz o próprio login OAuth no navegador
        // (é o mesmo fluxo do app oficial) e salva credentials.json no cache.
        args << QStringLiteral("--enable-oauth");
    }

    m_stopping = false;
    m_buffer.clear();
    setState(Starting, tr("Iniciando o motor de áudio..."));
    m_process.start(m_exe, args);
}

void LibrespotProcess::stop()
{
    m_restartTimer.stop();
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_stopping = true;
    // librespot é um app de console: no Windows terminate() (WM_CLOSE) é ignorado.
    m_process.kill();
    m_process.waitForFinished(2000);
    setState(Stopped);
}

void LibrespotProcess::restart()
{
    stop();
    m_crashCount = 0;
    start();
}

void LibrespotProcess::resetLogin()
{
    stop();
    QFile::remove(QDir(cacheDir()).filePath(QStringLiteral("credentials.json")));
    m_crashCount = 0;
    start();
}

void LibrespotProcess::onReadyRead()
{
    m_buffer += m_process.readAll();
    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_buffer.left(idx)).trimmed();
        m_buffer.remove(0, idx + 1);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void LibrespotProcess::handleLine(const QString &line)
{
    m_log << line;
    if (m_log.size() > 300)
        m_log.removeFirst();

    // Login interativo do librespot: "Browse to: https://accounts.spotify.com/authorize?..."
    // O próprio librespot (0.8) já abre essa URL no navegador antes de imprimir a linha;
    // abrir de novo aqui criaria uma segunda aba. Só atualizamos o estado.
    if (line.contains(QLatin1String("Browse to:"))) {
        setState(NeedsLogin, tr("Autorize o motor de áudio na aba que abriu no navegador."));
        return;
    }

    if (line.contains(QLatin1String("Authenticated as"))) {
        m_crashCount = 0;
        setState(Ready, tr("Pronto para tocar"));
        return;
    }

    if (line.contains(QLatin1String("librespot_playback::player"))
        || line.contains(QLatin1String("librespot_connect"))) {
        if (line.contains(QLatin1String("Loading <")) || line.contains(QLatin1String(") loaded"))
            || line.contains(QLatin1String("Pause")) || line.contains(QLatin1String("Play"))) {
            emit playbackEvent();
        }
    }

    if (line.contains(QLatin1String(" ERROR "))) {
        qWarning("librespot: %s", qPrintable(line));
        if (m_state == Starting)
            setState(Starting, line.section(QLatin1Char(']'), 1).trimmed());
    }
}

void LibrespotProcess::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    if (m_stopping) {
        m_stopping = false;
        return;
    }
    ++m_crashCount;
    const QString last = m_log.isEmpty() ? QString() : m_log.constLast();
    if (m_crashCount <= 3) {
        setState(Crashed, tr("O motor de áudio parou (código %1). Reiniciando...").arg(exitCode));
        m_restartTimer.start(1500 * m_crashCount);
    } else {
        setState(Crashed, tr("O motor de áudio parou várias vezes. Última mensagem: %1").arg(last));
    }
}

void LibrespotProcess::setState(State s, const QString &text)
{
    if (m_state == s && m_statusText == text)
        return;
    m_state = s;
    m_statusText = text;
    emit stateChanged();
}

void LibrespotProcess::attachToJob()
{
#ifdef Q_OS_WIN
    // Job Object com KILL_ON_JOB_CLOSE: se o Tonal fechar ou travar,
    // o Windows encerra o librespot junto. Nada de processo órfão.
    static HANDLE job = [] {
        HANDLE j = CreateJobObjectW(nullptr, nullptr);
        if (j) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info {};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(j, JobObjectExtendedLimitInformation, &info, sizeof(info));
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
