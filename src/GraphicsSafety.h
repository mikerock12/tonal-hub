#pragma once
// Escolha segura e visível do renderizador do Qt Quick no Windows (0.17.2).
//
// Em 20/11/2026 o Direct3D deste PC passou a derrubar todos os apps ao criar
// o swapchain: o código do dxgi.dll estava alterado na memória (222 bytes) e
// até uma sonda mínima sem Qt caía. Depois de reiniciar o Windows, 0 bytes
// alterados e tudo voltou. O módulo do Warsaw continuou injetado nos dois casos,
// então não é a causa: o app não decide nada por módulos de terceiros. Não há
// exceção para capturar: o processo some. Por isso cada programa grava um
// marcador antes de abrir a janela e o limpa 3 s depois do primeiro quadro.
// Se a abertura anterior morreu antes disso, a próxima usa OpenGL; se OpenGL
// também falhar, usa renderização por software.
//
// A troca nunca é silenciosa: o app mostra um aviso explicando que o DirectX
// falhou, o renderizador em uso aparece em Sobre, cada decisão é registrada em
// %LOCALAPPDATA%/Tonal/graphics/<programa>.log e "Tentar DirectX de novo" volta
// ao padrão. Depois de reiniciar o Windows o DirectX é tentado de novo sozinho. QSG_RHI_BACKEND ou QT_QUICK_BACKEND definidos externamente prevalecem.
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QString>
#include <QTimer>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#endif

namespace tonal {

class GraphicsSafety : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString backend READ backend CONSTANT)
  Q_PROPERTY(QString reason READ reason CONSTANT)
  Q_PROPERTY(bool fallback READ fallback CONSTANT)
  Q_PROPERTY(QString title READ title CONSTANT)
  Q_PROPERTY(QString message READ message CONSTANT)
  Q_PROPERTY(QString adapter READ adapter CONSTANT)
  Q_PROPERTY(QString activeApi READ activeApi NOTIFY activeApiChanged)
  Q_PROPERTY(bool noticePending READ noticePending NOTIFY noticeChanged)
  Q_PROPERTY(QString logPath READ logPath CONSTANT)

public:
  // Chamar antes de criar o QGuiApplication.
  explicit GraphicsSafety(const char *program) : m_program(QString::fromLatin1(program)) {
    m_adapter = detectAdapter();
    if (qEnvironmentVariableIsSet("QSG_RHI_BACKEND") || qEnvironmentVariableIsSet("QT_QUICK_BACKEND")) {
      m_reason = QStringLiteral("external");
      return;
    }
    const QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty())
      return;
    const QString dir = base + QStringLiteral("/Tonal/graphics");
    QDir().mkpath(dir);
    m_path = dir + QLatin1Char('/') + m_program + QStringLiteral(".txt");
    m_logPath = dir + QLatin1Char('/') + m_program + QStringLiteral(".log");
    QString backend = QStringLiteral("auto"), reason;
    bool pending = false, dismissed = false;
    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) {
      const auto parts = QString::fromLatin1(file.readAll()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
      if (!parts.isEmpty() && (parts[0] == QLatin1String("auto") || parts[0] == QLatin1String("opengl") ||
                               parts[0] == QLatin1String("software")))
        backend = parts[0];
      pending = parts.value(1) == QLatin1String("1");
      dismissed = parts.value(2) == QLatin1String("1");
      reason = parts.value(3) == QLatin1String("-") ? QString() : parts.value(3);
      // Reiniciar o Windows costuma resolver: depois de um boot novo, tenta o
      // Direct3D outra vez (se cair de novo, o OpenGL volta sozinho).
      if (!pending && backend != QLatin1String("auto") && qAbs(parts.value(4).toLongLong() - bootId()) > 2) {
        log(QStringLiteral("Windows reiniciado desde a troca para %1; tentando Direct3D de novo.").arg(apiName(backend)));
        backend = QStringLiteral("auto");
        reason.clear();
      }
    }
    if (pending) {  // a abertura anterior caiu antes de desenhar a janela
      const QString failed = backend;
      backend = backend == QLatin1String("auto") ? QStringLiteral("opengl") : QStringLiteral("software");
      reason = failed == QLatin1String("auto") ? QStringLiteral("directx-crash") : QStringLiteral("opengl-crash");
      dismissed = false;
      log(QStringLiteral("A abertura anterior com %1 terminou antes do primeiro quadro; usando %2.")
              .arg(apiName(failed), apiName(backend)));
    }
    if (backend == QLatin1String("auto"))
      reason.clear();
    m_backend = backend;
    m_reason = reason;
    m_dismissed = dismissed;
    if (backend == QLatin1String("opengl"))
      qputenv("QSG_RHI_BACKEND", "opengl");
    else if (backend == QLatin1String("software"))
      qputenv("QT_QUICK_BACKEND", "software");
    write(true);
  }

  QString backend() const { return m_backend; }
  QString reason() const { return m_reason; }
  bool fallback() const { return m_backend != QLatin1String("auto") && !m_reason.isEmpty() && m_reason != QLatin1String("external"); }
  QString adapter() const { return m_adapter; }
  QString activeApi() const { return m_activeApi; }
  bool noticePending() const { return fallback() && !m_dismissed; }
  QString logPath() const { return m_logPath; }
  QString title() const {
    if (!fallback())
      return {};
    return m_backend == QLatin1String("software") ? tr("DirectX e OpenGL falharam neste PC")
                                                  : tr("O DirectX falhou neste PC");
  }
  QString message() const {
    const QString gpu = m_adapter.isEmpty() ? tr("a placa de vídeo") : m_adapter;
    if (m_reason == QLatin1String("directx-crash"))
      return tr("Na última abertura o Direct3D (dxgi.dll) do Windows fechou o app antes de mostrar a janela, "
                "então o Tonal passou a desenhar a tela com OpenGL. O problema está no DirectX do Windows ou no "
                "driver de vídeo de %1, não na música nem nos seus dados. Reiniciar o PC costuma resolver: depois "
                "disso o Tonal tenta o DirectX sozinho. Se continuar, atualize o driver no site do fabricante.").arg(gpu);
    if (m_reason == QLatin1String("opengl-crash"))
      return tr("O Direct3D e depois o OpenGL fecharam o app antes de mostrar a janela. O Tonal está usando "
                "renderização por software, que funciona mas usa mais processador. Reinicie o PC; se continuar, "
                "atualize o driver de vídeo de %1.").arg(gpu);
    return {};
  }

  // Depois de carregar o QML: confirma o renderizador quando a janela já desenhou.
  void watch(QQmlApplicationEngine &engine) {
    for (QObject *root : engine.rootObjects()) {
      if (auto *window = qobject_cast<QQuickWindow *>(root)) {
        auto connection = std::make_shared<QMetaObject::Connection>();
        *connection = QObject::connect(window, &QQuickWindow::frameSwapped, this, [this, window, connection] {
          QObject::disconnect(*connection);
          if (auto *renderer = window->rendererInterface())
            m_activeApi = apiLabel(renderer->graphicsApi());
          // Fechar normalmente depois do primeiro quadro não é queda (uma queda
          // real não passa por aboutToQuit): evita falso positivo na próxima abertura.
          QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
            if (!m_path.isEmpty())
              write(false);
          });
          emit activeApiChanged();
          QTimer::singleShot(3000, this, [this] {
            if (!m_path.isEmpty())
              write(false);
          });
        });
        return;
      }
    }
  }

  Q_INVOKABLE void dismissNotice() {
    m_dismissed = true;
    if (!m_path.isEmpty())
      write(false);
    emit noticeChanged();
  }
  // Volta ao Direct3D na próxima abertura (se cair de novo, o OpenGL volta sozinho).
  Q_INVOKABLE void retryDirectX() {
    if (m_path.isEmpty())
      return;
    log(QStringLiteral("Usuário pediu para tentar o DirectX de novo na próxima abertura."));
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
      file.write("auto 0 0");
  }

signals:
  void activeApiChanged();
  void noticeChanged();

private:
  static QString apiName(const QString &backend) {
    return backend == QLatin1String("opengl") ? QStringLiteral("OpenGL")
           : backend == QLatin1String("software") ? QStringLiteral("software")
                                                  : QStringLiteral("Direct3D");
  }
  static QString apiLabel(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
    case QSGRendererInterface::Direct3D11: return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::Direct3D12: return QStringLiteral("Direct3D 12");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL");
    case QSGRendererInterface::Software: return QStringLiteral("Software (processador)");
    case QSGRendererInterface::Vulkan: return QStringLiteral("Vulkan");
    default: return QStringLiteral("Outro");
    }
  }
  static QString detectAdapter() {
#ifdef _WIN32
    DISPLAY_DEVICEW device{};
    device.cb = sizeof(device);
    for (DWORD i = 0; EnumDisplayDevicesW(nullptr, i, &device, 0); ++i)
      if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        return QString::fromWCharArray(device.DeviceString);
#endif
    return {};
  }
  // Identifica o boot atual (minuto em que o Windows iniciou).
  static qint64 bootId() {
#ifdef _WIN32
    return (QDateTime::currentMSecsSinceEpoch() - qint64(GetTickCount64())) / 60000;
#else
    return 0;
#endif
  }
  void write(bool pending) const {
    QFile file(m_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      file.write(QStringLiteral("%1 %2 %3 %4 %5")
                     .arg(m_backend, pending ? QStringLiteral("1") : QStringLiteral("0"),
                          m_dismissed ? QStringLiteral("1") : QStringLiteral("0"),
                          m_reason.isEmpty() ? QStringLiteral("-") : m_reason, QString::number(bootId()))
                     .toLatin1());
      file.flush();
    }
  }
  void log(const QString &line) const {
    if (m_logPath.isEmpty())
      return;
    QFile file(m_logPath);
    if (file.size() > 64 * 1024)
      file.remove();
    if (file.open(QIODevice::Append | QIODevice::Text))
      file.write((QDateTime::currentDateTime().toString(Qt::ISODate) + QLatin1Char(' ') + m_adapter +
                  QStringLiteral(" | ") + line + QLatin1Char('\n'))
                     .toUtf8());
  }

  QString m_program, m_path, m_logPath, m_adapter, m_activeApi;
  QString m_backend = QStringLiteral("auto"), m_reason;
  bool m_dismissed = false;
};

} // namespace tonal
