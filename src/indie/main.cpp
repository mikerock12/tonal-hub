#include "CatalogService.h"
#include "HubController.h"
#include "audio/AudioEngine.hpp"
#include "companion/MusicCompanion.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>
extern "C" {
#include <libavutil/log.h>
}
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QGuiApplication::setFont(QFont(QStringLiteral("Segoe UI"), 10));
  QCoreApplication::setOrganizationName("Tonal");
#ifdef TONAL_HUB_SHELL
  const bool hubMode = true;
#else
  const bool hubMode = false;
#endif
  QCoreApplication::setApplicationName(hubMode ? "TonalHub" : "IndieTonal");
  QQuickStyle::setStyle("Basic");
  const auto args = app.arguments();
  int index = args.indexOf("--query");
  QString query = index >= 0 ? args.value(index + 1).left(160) : QString();
  const bool smoke = args.contains("--smoke-test");
  auto userHash = QCryptographicHash::hash(QDir::homePath().toUtf8(),
                                           QCryptographicHash::Sha256)
                      .toHex()
                      .left(16);
  QString name =
      app.applicationName() + "-" + userHash + (smoke ? "-smoke" : "");
  QLocalSocket client;
  client.connectToServer(name);
  if (client.waitForConnected(150)) {
    client.write(QJsonDocument(QJsonObject{{"query", query}})
                     .toJson(QJsonDocument::Compact));
    client.waitForBytesWritten(500);
    return 0;
  }
  QLocalServer server;
  server.setSocketOptions(QLocalServer::UserAccessOption);
  QLocalServer::removeServer(name);
  if (!server.listen(name))
    return 3;
  CatalogService catalog(nullptr, !smoke);
  HubController hub;
  MusicCompanion music(nullptr,smoke ? QDir::tempPath()+"/TonalIndieCompanionSmoke" : QString());
  std::unique_ptr<tonal::audio::AudioEngine> audio;
  if (!hubMode)
    audio = std::make_unique<tonal::audio::AudioEngine>(
        nullptr, smoke ? QDir::tempPath() + "/TonalIndieSmoke" : QString());
  // URL query parameters can hold library tokens. Diagnostics use safe
  // messages.
  av_log_set_level(AV_LOG_QUIET);
  if (audio)
    QObject::connect(&catalog, &CatalogService::playable, audio.get(),
                     &tonal::audio::AudioEngine::openStream);
  QQmlApplicationEngine engine;
  engine.rootContext()->setContextProperty("catalog", &catalog);
  engine.rootContext()->setContextProperty("hub", &hub);
  engine.rootContext()->setContextProperty("music", &music);
  QTimer historyTimer;historyTimer.setInterval(1000);
  QObject::connect(&historyTimer,&QTimer::timeout,&app,[&] {if(audio)music.pulse(audio->localPlaying(),audio->positionMs());});historyTimer.start();
  if(audio)QObject::connect(&music,&MusicCompanion::seekRequested,audio.get(),&tonal::audio::AudioEngine::seekLocal);
  engine.rootContext()->setContextProperty("audio", audio.get());
  engine.rootContext()->setContextProperty("hubMode", hubMode);
  engine.rootContext()->setContextProperty("initialQuery", query);
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      [] { QCoreApplication::exit(2); }, Qt::QueuedConnection);
  engine.load(
      QUrl(hubMode ? "qrc:/indie/HubMain.qml" : "qrc:/indie/IndieMain.qml"));
  int localFileArg = args.indexOf("--local-file");
  if (audio && localFileArg >= 0)
    QTimer::singleShot(0, &app, [&] {
      audio->openFile(QUrl::fromLocalFile(args.value(localFileArg + 1)));
    });
  int panelArg = args.indexOf("--panel");
  if (smoke && panelArg >= 0)
    QTimer::singleShot(50, &app, [&] {
      if (auto *root = engine.rootObjects().value(0))
        QMetaObject::invokeMethod(
            root, "showPanel",
            Q_ARG(QVariant, QVariant(args.value(panelArg + 1))));
    });
  QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
    while (auto *socket = server.nextPendingConnection()) {
      auto buffer = std::make_shared<QByteArray>();
      auto handle = [&, socket, buffer] {
        if (socket->property("handled").toBool())
          return;
        buffer->append(socket->readAll());
        if (buffer->size() > 4096) {
          socket->abort();
          return;
        }
        auto doc = QJsonDocument::fromJson(*buffer);
        if (!doc.isObject())
          return;
        socket->setProperty("handled", true);
        if (doc.object()["action"] == "pause") {
          if (audio && audio->localPlaying())
            audio->toggleLocalPlay();
          socket->disconnectFromServer();
          return;
        }
        QString q = doc.object()["query"].toString().left(160);
        if (!hubMode && !q.isEmpty())
          catalog.search(q);
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
                     : (query.isEmpty() ? 900 : 12000),
        &app, [&] {
          int screenshot = args.indexOf("--screenshot");
          if (screenshot >= 0)
            if (auto *window =
                    qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
              window->grabWindow().save(args.value(screenshot + 1));
          int stateArg = args.indexOf("--state-file");
          if (stateArg >= 0) {
            QFile file(args.value(stateArg + 1));
            if (file.open(QIODevice::WriteOnly))
              file.write(
                  QJsonDocument(
                      QJsonObject{
                          {"query", catalog.query()},
                          {"items",
                           QJsonArray::fromVariantList(catalog.items())},
                          {"liveStatus", catalog.liveStatus()},
                          {"messages",
                           QJsonArray::fromStringList(catalog.messages())},
                          {"busy", catalog.busy()},
                          {"playing", audio && audio->localPlaying()},
                          {"positionMs", audio ? audio->positionMs() : 0},
                          {"durationMs", audio ? audio->durationMs() : 0}})
                      .toJson());
          }
          app.exit(engine.rootObjects().size() == 1 ? 0 : 4);
        });
  return app.exec();
}
