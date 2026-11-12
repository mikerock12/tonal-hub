#include "DesktopUpdate.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QVersionNumber>

namespace {
constexpr auto portal = "https://softwares.smellsliketech.com.br/api/releases/tonal-windows";
constexpr auto assets = "https://github.com/mikerock12/tonal-hub/releases/download/";
constexpr qint64 maxInstaller = 256 * 1024 * 1024;
constexpr qint64 maxMetadata = 16 * 1024;
const QRegularExpression versionPattern(QStringLiteral("^([0-9]{1,6}\\.){2}[0-9]{1,6}$"));

QString installation() {
#ifdef Q_OS_WIN
  const QSettings key(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{D47D88D5-B58A-49D2-868C-68A55BC3B4E2}_is1"),
                      QSettings::NativeFormat);
  const auto registered = QFileInfo(key.value(QStringLiteral("InstallLocation")).toString()).canonicalFilePath();
  const auto running = QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath();
  if (!registered.isEmpty() && registered.compare(running, Qt::CaseInsensitive) == 0)
    return running;
#endif
  return {};
}

QNetworkRequest request(const QUrl &url) {
  QNetworkRequest out(url);
  out.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  out.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
  out.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
  out.setTransferTimeout(20000);
  out.setRawHeader("User-Agent", "Tonalify-Desktop-Updater");
  return out;
}
}

DesktopUpdate::DesktopUpdate(QObject *parent)
    : QObject(parent), m_file(QDir::tempPath() + QStringLiteral("/Tonal-Update-XXXXXX.exe")),
      m_installPath(installation()) {}

DesktopUpdate::Release DesktopUpdate::parseRelease(const QByteArray &data) {
  if (data.size() > maxMetadata)
    return {};
  const auto json = QJsonDocument::fromJson(data).object();
  const auto tag = json["version"].toString();
  const auto version = tag.mid(1);
  if (!tag.startsWith(QLatin1Char('v')) || !versionPattern.match(version).hasMatch())
    return {};
  const auto expected = QStringLiteral("Tonal-Hub-%1.0-Windows-x64-Setup.exe").arg(version);
  const auto hash = json["sha256"].toString().toLatin1();
  static const QRegularExpression hex(QStringLiteral("^[0-9a-fA-F]{64}$"));
  const auto url = QString::fromLatin1(assets) + tag + '/' + expected;
  const auto size = json["size"].toInteger();
  if (json["name"].toString() != expected || json["url"].toString() != url ||
      size < 1 || size > maxInstaller || !hex.match(QString::fromLatin1(hash)).hasMatch())
    return {};
  return {version, expected, QByteArray::fromHex(hash), size, QUrl(url)};
}

bool DesktopUpdate::newer(const QString &candidate, const QString &installed) {
  return versionPattern.match(candidate).hasMatch() && versionPattern.match(installed).hasMatch() &&
         QVersionNumber::compare(QVersionNumber::fromString(candidate), QVersionNumber::fromString(installed)) > 0;
}

bool DesktopUpdate::allowedAssetRedirect(const QUrl &url) {
  const auto host = url.host();
  return url.isValid() && url.scheme() == "https" && url.userInfo().isEmpty() &&
         url.port() == -1 && !url.hasFragment() &&
         (host == "release-assets.githubusercontent.com" || host == "objects.githubusercontent.com");
}

void DesktopUpdate::status(const QString &text) {
  m_status = text;
  emit changed();
}

void DesktopUpdate::fail(const QString &text) {
  if (m_file.isOpen()) m_file.close();
  if (!m_file.fileName().isEmpty()) m_file.remove();
  m_busy = false;
  m_ready = false;
  status(text);
}

void DesktopUpdate::check() {
  if (m_busy || m_ready) return;
  m_release = {};
  m_busy = true;
  status(tr("Consultando a versão no portal…"));
  auto *reply = m_network.get(request(QUrl(QString::fromLatin1(portal))));
  reply->setReadBufferSize(maxMetadata + 1);
  connect(reply, &QNetworkReply::readyRead, this, [reply] {
    if (reply->bytesAvailable() > maxMetadata) reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    m_busy = false;
    if (reply->error() != QNetworkReply::NoError ||
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
      status(tr("Não foi possível consultar o portal. Tente novamente."));
      return;
    }
    auto release = parseRelease(reply->read(maxMetadata + 1));
    if (release.version.isEmpty()) {
      status(tr("O portal retornou dados inválidos para a atualização."));
      return;
    }
    if (!newer(release.version, QCoreApplication::applicationVersion())) {
      status(tr("O Tonalify está atualizado."));
      return;
    }
    m_release = std::move(release);
    status(installed() ? tr("Versão %1 disponível.").arg(m_release.version)
                       : tr("Versão %1 disponível no portal. A instalação automática requer a edição instalada.").arg(m_release.version));
  });
}

void DesktopUpdate::download() {
  if (m_busy || m_ready || !installed() || m_release.version.isEmpty()) return;
  if (!m_file.open()) {
    fail(tr("Não foi possível criar o arquivo da atualização."));
    return;
  }
  m_hash.reset();
  m_received = 0;
  m_busy = true;
  status(tr("Baixando a versão %1…").arg(m_release.version));
  fetchAsset(m_release.url);
}

void DesktopUpdate::fetchAsset(const QUrl &url, int redirects) {
  auto *reply = m_network.get(request(url));
  reply->setReadBufferSize(256 * 1024);
  auto consume = [this, reply] {
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200)
      return;
    while (reply->bytesAvailable() > 0) {
      const auto chunk = reply->read(64 * 1024);
      if (chunk.isEmpty() || chunk.size() > m_release.size - m_received || m_file.write(chunk) != chunk.size()) {
        reply->setProperty("badPayload", true);
        if (reply->isRunning()) reply->abort();
        return;
      }
      m_received += chunk.size();
      m_hash.addData(chunk);
    }
  };
  connect(reply, &QNetworkReply::readyRead, this, consume);
  connect(reply, &QNetworkReply::finished, this, [this, reply, url, redirects, consume] {
    reply->deleteLater();
    const auto redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!redirect.isEmpty() && redirects < 3 && allowedAssetRedirect(url.resolved(redirect))) {
      fetchAsset(url.resolved(redirect), redirects + 1);
      return;
    }
    consume();
    if (reply->error() != QNetworkReply::NoError || reply->property("badPayload").toBool() ||
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200 ||
        m_received != m_release.size || m_hash.result() != m_release.sha256 || !m_file.flush()) {
      fail(tr("O download falhou ou o instalador não passou na verificação."));
      return;
    }
    m_file.close();
    m_busy = false;
    m_ready = true;
    status(tr("Instalador verificado. Iniciando a atualização…"));
    install();
  });
}

void DesktopUpdate::install() {
  if (!m_ready || m_busy) return;
  if (installation() != m_installPath) {
    fail(tr("A instalação do Tonal Hub mudou. Verifique a atualização novamente."));
    return;
  }
  // Recheck the bytes immediately before starting the installer.
  QFile verified(m_file.fileName());
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!verified.open(QIODevice::ReadOnly) || verified.size() != m_release.size ||
      !hash.addData(&verified) || hash.result() != m_release.sha256) {
    verified.close();
    fail(tr("O arquivo da atualização mudou. Baixe o instalador novamente."));
    return;
  }
  verified.close();
  if (!QProcess::startDetached(m_file.fileName(),
       {"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS",
        "/TONALRESTART=Tonalify", "/DIR=" + QDir::toNativeSeparators(m_installPath)})) {
    status(tr("Não foi possível iniciar o instalador."));
    return;
  }
  m_file.setAutoRemove(false);
  QCoreApplication::quit();
}
