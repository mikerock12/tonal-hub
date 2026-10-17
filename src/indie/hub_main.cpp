#include "HubController.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <memory>

// The idle hub has no audio engine, catalog, credentials or polling thread.
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setFont(QFont("Segoe UI", 10));
  app.setOrganizationName("Tonal");
  app.setApplicationName("TonalHub");
  QQuickStyle::setStyle("Basic");
  auto args = app.arguments();
  bool smoke = args.contains("--smoke-test");
  auto hash = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                       QCryptographicHash::Sha256)
                  .toHex()
                  .left(16);
  QString name = "TonalHub-" + hash + (smoke ? "-smoke" : "");
  QLocalSocket client;
  client.connectToServer(name);
  if (client.waitForConnected(150)) {
    client.write("activate");
    client.waitForBytesWritten(500);
    return 0;
  }
  QLocalServer server;
  server.setSocketOptions(QLocalServer::UserAccessOption);
  QLocalServer::removeServer(name);
  if (!server.listen(name))
    return 3;
  HubController hub;
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("hub", &hub);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
  engine.load(QUrl("qrc:/indie/HubMain.qml"));
  QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
    while (auto *socket = server.nextPendingConnection()) {
      auto handle = [&, socket] {
        if (socket->property("handled").toBool())
          return;
        socket->setProperty("handled", true);
        socket->read(128);
        if (auto *window =
                qobject_cast<QWindow *>(engine.rootObjects().value(0))) {
          window->showNormal();
          window->requestActivate();
        }
        socket->disconnectFromServer();
      };
      QObject::connect(socket, &QLocalSocket::readyRead, &app, handle);
      if (socket->bytesAvailable())
        QTimer::singleShot(0, socket, handle);
      QObject::connect(socket, &QLocalSocket::disconnected, socket,
                       &QObject::deleteLater);
      QTimer::singleShot(2000, socket, [socket] {
        socket->abort();
        socket->deleteLater();
      });
    }
  });
  int holdArg = args.indexOf("--hold-ms");
  if (smoke)
    QTimer::singleShot(
        holdArg >= 0 ? qBound(900, args.value(holdArg + 1).toInt(), 20000)
                     : 900,
        &app, [&] {
          int screenshot = args.indexOf("--screenshot");
          if (screenshot >= 0)
            if (auto *window =
                    qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
              window->grabWindow().save(args.value(screenshot + 1));
          app.exit(engine.rootObjects().size() == 1 ? 0 : 4);
        });
  return app.exec();
}
