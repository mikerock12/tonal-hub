#include "HubController.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>
namespace {
bool launch(const QString &name, const QStringList &args = {}) {
  return QProcess::startDetached(QCoreApplication::applicationDirPath() + "/" +
                                     name + ".exe",
                                 args, QCoreApplication::applicationDirPath());
}
void pause(const QString &name) {
  auto hash = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                       QCryptographicHash::Sha256)
                  .toHex()
                  .left(16);
  QLocalSocket socket;
  socket.connectToServer(name + "-" + hash);
  if (socket.waitForConnected(100)) {
    socket.write("{\"action\":\"pause\"}");
    socket.waitForBytesWritten(250);
  }
}
} // namespace
void HubController::indie(const QString &query) {
  pause("Tonalify");
  if (!launch("IndieTonal", {"--query", query.left(160)}))
    emit error("Não foi possível abrir IndieTonal. Compile o hub primeiro.");
}
void HubController::spotify() {
  pause("IndieTonal");
  QString executable =
      QFileInfo::exists(QCoreApplication::applicationDirPath() +
                        "/Tonalify.exe")
          ? "Tonalify"
          : "Tonal";
  if (!launch(executable))
    emit error("Tonalify não encontrado. Compile e empacote o hub primeiro.");
}
void HubController::hub() {
  if (!launch("TonalHub"))
    emit error("Tonal Hub não encontrado");
}
void HubController::external(const QUrl &u) {
  if (u.scheme() == "https" && u.userInfo().isEmpty())
    QDesktopServices::openUrl(u);
}
