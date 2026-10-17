#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "Player.h"
#include "SpotifyApi.h"
#include "audio/AudioEngine.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

struct Response {
  int status = 204;
  QByteArray body;
  bool hang = false;
  int delay = 0;
};
class FakeReply : public QNetworkReply {
public:
  FakeReply(const QNetworkRequest &request, Response response, QObject *parent)
      : QNetworkReply(parent), m_body(response.body) {
    setRequest(request);
    setUrl(request.url());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, response.status);
    if (response.status >= 400 || response.status == 0)
      setError(ConnectionRefusedError, "simulated response");
    open(QIODevice::ReadOnly);
    if (!response.hang)
      QTimer::singleShot(response.delay, this, [this] { finish(); });
  }
  void abort() override {
    if (isFinished())
      return;
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 0);
    setError(OperationCanceledError, "simulated deadline abort");
    finish();
  }
  qint64 bytesAvailable() const override {
    return m_body.size() - m_offset + QIODevice::bytesAvailable();
  }
  bool isSequential() const override { return true; }

protected:
  qint64 readData(char *data, qint64 max) override {
    const qint64 n = std::min(max, qint64(m_body.size()) - m_offset);
    if (n <= 0)
      return -1;
    std::memcpy(data, m_body.constData() + m_offset, size_t(n));
    m_offset += n;
    return n;
  }

private:
  void finish() {
    if (isFinished())
      return;
    setFinished(true);
    emit readyRead();
    emit finished();
  }
  QByteArray m_body;
  qint64 m_offset = 0;
};
class FakeNetwork : public QNetworkAccessManager {
public:
  struct Sent {
    QUrl url;
    QByteArray body;
  };
  QList<Sent> sent;
  std::function<Response(const QNetworkRequest &, const QByteArray &)> respond;

protected:
  QNetworkReply *createRequest(Operation, const QNetworkRequest &request,
                               QIODevice *outgoing) override {
    auto body = outgoing ? outgoing->readAll() : QByteArray();
    sent.append({request.url(), body});
    return new FakeReply(request, respond ? respond(request, body) : Response{},
                         this);
  }
};

class SpotifyRecoveryTests {
public:
  static QJsonObject liveSession(bool readyOnly = false) {
    QJsonObject report{{"passed", false},
                       {"real_spotify_session", true},
                       {"physical_audio_tested", false},
                       {"credentials_logged", false}};
    report["readiness_only"] = readyOnly;
    QTemporaryDir storage;
    try {
      QNetworkAccessManager network;
      AuthManager auth(&network);
      SpotifyApi api(&network, &auth);
      if (!auth.isAuthenticated()) {
        report["skipped"] = "No saved Web API session";
        return report;
      }
      require(spin(
                  [&] {
                    return auth.hasValidAccessToken() || !auth.isRefreshing();
                  },
                  25000) &&
                  auth.hasValidAccessToken(),
              "Saved API session could not be refreshed");
      bool received = false;
      int status = 0;
      QJsonObject state;
      api.get("/me/player", {}, [&](int code, const QJsonDocument &doc) {
        status = code;
        state = doc.object();
        received = true;
      });
      require(spin([&] { return received; }, 15000) &&
                  (status == 200 || status == 204),
              "Could not inspect existing playback before live test");
      if (state["is_playing"].toBool() && !readyOnly) {
        report["skipped"] =
            "Playback already active; left the user's session untouched";
        return report;
      }
      QStringList tracks;
      if (!readyOnly) {
        received = false;
        api.get("/me/player/recently-played", {{"limit", 10}},
                [&](int code, const QJsonDocument &doc) {
                  status = code;
                  for (const auto value : doc.object()["items"].toArray()) {
                    auto uri =
                        value.toObject()["track"].toObject()["uri"].toString();
                    if (uri.startsWith("spotify:track:") &&
                        !tracks.contains(uri))
                      tracks << uri;
                  }
                  received = true;
                });
        require(spin([&] { return received; }, 15000) && status == 200 &&
                    tracks.size() >= 2,
                "Could not obtain two tracks for the live playback test");
      }
      const auto saved = QStandardPaths::writableLocation(
                             QStandardPaths::AppLocalDataLocation) +
                         "/librespot/credentials.json";
      if (!QFileInfo::exists(saved)) {
        report["skipped"] =
            "No cached librespot session; interactive login was not started";
        return report;
      }
      QDir().mkpath(storage.path() + "/librespot");
      require(
          QFile::copy(saved, storage.path() + "/librespot/credentials.json"),
          "Could not prepare isolated live-session cache");
      // The temporary cache is deleted after child shutdown. Nothing is copied
      // into the build, portable package, report, console, or repository.
      tonal::audio::AudioEngine audio(nullptr, storage.path() + "/audio");
      LibrespotProcess engine(nullptr, storage.path());
      engine.m_deviceName = "Tonalify diagnostico local";
      engine.setAudioEngine(&audio);
      int recoveries = 0;
      QObject::connect(&engine, &LibrespotProcess::sessionLost, &engine,
                       [&] { ++recoveries; });
      engine.start();
      const bool ready = spin(
          [&] { return engine.state() == LibrespotProcess::Ready; }, 25000);
      if (!ready) {
        report["engine_state"] = int(engine.state());
        report["engine_status"] = engine.statusText();
        report["engine_log"] = engine.recentLog();
      }
      require(ready, "Live engine/device did not become ready");
      if (readyOnly) {
        bool found = false;
        for (int attempt = 0; attempt < 8 && !found; ++attempt) {
          received = false;
          api.get("/me/player/devices", {},
                  [&](int code, const QJsonDocument &doc) {
                    if (code == 200)
                      for (const auto value : doc.object()["devices"].toArray())
                        if (value.toObject()["name"].toString() ==
                            engine.deviceName())
                          found = true;
                    received = true;
                  });
          require(spin([&] { return received; }, 10000),
                  "Device query timed out");
          if (!found)
            wait(1000);
        }
        report["engine_ready"] = true;
        report["device_discovered"] = found;
        report["playback_commands_sent"] = 0;
        report["passed"] = found;
        engine.stop();
        return report;
      }
      Player player(&api, &engine, &auth);
      player.setAudioEngine(&audio);
      player.findLocalDevice(8);
      require(spin([&] { return !player.localDeviceId().isEmpty(); }, 20000),
              "Live device not discovered");
      player.m_activeDeviceId.clear();
      player.playUris(tracks.mid(0, 2));
      require(spin(
                  [&] {
                    return audio.spotifyFramesRead() > 96000 &&
                           player.isPlaying() && player.playingHere();
                  },
                  20000),
              "Initial real Spotify PCM did not advance");
      const auto firstPid = engine.m_process.processId();
      engine.m_process.kill();
      require(spin(
                  [&] {
                    return engine.state() == LibrespotProcess::Ready &&
                           engine.m_process.processId() != firstPid &&
                           audio.spotifyFramesRead() > 96000 &&
                           player.playingHere() && player.isPlaying();
                  },
                  30000),
              "Real Spotify playback did not resume after killing librespot");
      report["unexpected_exit_resumed"] = true;
      const auto secondPid = engine.m_process.processId();
      engine.m_process.kill();
      require(spin([&] { return engine.state() == LibrespotProcess::Starting; },
                   3000),
              "Second injected exit was not detected");
      player.playUris({tracks[1]});
      require(
          spin(
              [&] {
                return engine.state() == LibrespotProcess::Ready &&
                       engine.m_process.processId() != secondPid &&
                       audio.spotifyFramesRead() > 96000 &&
                       player.trackUri() == tracks[1] && player.playingHere() &&
                       player.isPlaying();
              },
              30000),
          "New track selected during recovery did not play on the real engine");
      report["new_track_during_recovery_played"] = true;
      report["recovery_events"] = recoveries;
      report["final_pcm_frames"] = qint64(audio.spotifyFramesRead());
      // End only this diagnostic device's playback, with no changes to volume,
      // playlists, library, or other devices.
      player.cancelResume();
      player.m_isPlaying = false;
      received = false;
      api.put("/me/player/pause", {{"device_id", player.localDeviceId()}}, {},
              [&](int code, const QJsonDocument &) {
                status = code;
                received = true;
              });
      require(spin([&] { return received; }, 15000) && status >= 200 &&
                  status < 300,
              "Diagnostic device did not acknowledge pause");
      engine.stop();
      report["passed"] = true;
    } catch (const std::exception &error) {
      report["error"] = QString::fromUtf8(error.what());
    }
    return report;
  }
  static void require(bool ok, const char *message) {
    if (!ok)
      throw std::runtime_error(message);
  }
  static bool spin(const std::function<bool()> &done, int timeout = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout) {
      QCoreApplication::processEvents();
      if (done())
        return true;
      QThread::msleep(5);
    }
    return done();
  }
  static void wait(int ms) {
    QElapsedTimer timer;
    timer.start();
    spin([&] { return timer.elapsed() >= ms; }, ms + 1000);
  }
  static void fakeAuth(AuthManager &auth) {
    auth.m_clientId = "local-test-client";
    auth.m_refreshToken = "local-test-refresh";
    auth.m_accessToken = "local-test-access";
    auth.m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(600);
  }
  static void networkAndResume() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network); // --local-only disables all credential reads.
    SpotifyApi api(&network, &auth);
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    fakeAuth(auth);
    engine.m_state = LibrespotProcess::Ready;
    player.m_trackUri = "spotify:track:old";
    player.m_durationMs = 200000;
    player.m_positionBase = 12345;
    player.m_isPlaying = true;
    player.m_localDeviceId = player.m_activeDeviceId = "local-device";
    player.m_lastUris = {"spotify:track:first", "spotify:track:old",
                         "spotify:track:last"};
    player.rememberForResume();
    const auto resume = player.m_resumeBody;
    require(resume["uris"].toArray().size() == 3 &&
                resume["position_ms"].toInt() == 12345,
            "lost position or queue during engine restart");
    int playStatus = 0, plays = 0;
    network.respond = [&](const QNetworkRequest &req, const QByteArray &) {
      if (req.url().path().endsWith("/play")) {
        ++plays;
        return Response{playStatus};
      }
      if (req.url().path().endsWith("/devices"))
        return Response{
            200,
            QJsonDocument(
                QJsonObject{{"devices", QJsonArray{QJsonObject{
                                            {"id", "local-device"},
                                            {"name", engine.deviceName()}}}}})
                .toJson()};
      return Response{};
    };
    player.resumeAfterEngineRestart();
    require(spin([&] { return !player.m_resumeInFlight; }),
            "failed resume callback stuck");
    require(player.m_resumeBody == resume && player.m_resumeAttempts == 1,
            "network status zero discarded resume intent");
    playStatus = 204;
    player.resumeAfterEngineRestart();
    require(spin([&] { return player.m_resumeBody.isEmpty(); }),
            "successful resume retained stale intent");
    require(plays == 2, "resume retry missing");

    engine.m_state = LibrespotProcess::Starting;
    player.m_localDeviceId.clear();
    player.m_activeDeviceId.clear();
    player.playUris({"spotify:track:new-choice"});
    require(plays == 2 && !player.m_resumeBody.isEmpty(),
            "new selection lost while engine reconnecting");
    engine.m_state = LibrespotProcess::Ready;
    player.findLocalDevice(2);
    require(spin([&] { return player.m_resumeBody.isEmpty() && plays == 3; }),
            "new selection did not play after rediscovery");
    require(network.sent.last().body.contains("new-choice"),
            "old track overwrote new selection");

    // A scheduled API retry for the OLD play must not run after a new choice.
    player.m_resumeBody = resume;
    player.m_resumeInFlight = false;
    int oldPlays = 0, newPlays = 0;
    network.respond = [&](const QNetworkRequest &req, const QByteArray &body) {
      if (req.url().path().endsWith("/play")) {
        if (body.contains("old")) {
          ++oldPlays;
          return Response{503};
        }
        ++newPlays;
      }
      return Response{};
    };
    player.resumeAfterEngineRestart();
    require(spin([&] { return oldPlays == 1; }), "old request not issued");
    wait(20); // Let SpotifyApi schedule the transient retry.
    player.playUris({"spotify:track:newer-choice"});
    wait(1000);
    require(oldPlays == 1 && newPlays == 1 && player.m_resumeBody.isEmpty(),
            "cancelled resume reissued an old track");
    player.m_resumeBody = resume;
    player.m_isPlaying = true;
    player.togglePlay();
    require(player.m_resumeBody.isEmpty() && !engine.m_expectedPlayback,
            "pause did not cancel resume/watchdog intent");
    player.m_resumeBody = resume;
    player.m_isPlaying = false;
    player.pause();
    require(player.m_resumeBody.isEmpty(),
            "hub pause retained a pending resume while already stopped");
    std::cout << "PASS resume position/queue, failed request retained, new "
                 "choice during reconnect, cancelled API retries, pause\n";
  }
  static void deadlines() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network);
    SpotifyApi api(&network, &auth);
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    fakeAuth(auth);
    api.m_requestDeadlineMs = 80;
    network.respond = [](const auto &, const auto &) {
      return Response{0, {}, true};
    };
    player.refresh();
    require(player.m_requestInFlight, "hung GET not started");
    require(spin([&] { return !player.m_requestInFlight; }, 1000),
            "API deadline did not release Player");
    auth.m_tokenDeadlineMs = 80;
    auth.m_expiresAt = QDateTime::currentDateTimeUtc().addSecs(-1);
    player.refresh();
    require(auth.isRefreshing() && player.m_requestInFlight,
            "hung refresh not started");
    require(
        spin([&] { return !auth.isRefreshing() && !player.m_requestInFlight; },
             1000),
        "token deadline left all player commands waiting");
    fakeAuth(auth);
    network.respond = [](const auto &, const auto &) { return Response{}; };
    player.refresh();
    require(spin([&] { return !player.m_requestInFlight; }, 1000),
            "player did not recover after token timeout");
    std::cout << "PASS total deadlines for API/token; pending Player requests "
                 "recover\n";
  }
  static int countRuns(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      return 0;
    return file.readAll().trimmed().toInt();
  }
  static void pipeline(const QByteArray &mode) {
    QTemporaryDir storage;
    tonal::audio::AudioEngine audio(nullptr, storage.path() + "/audio");
    LibrespotProcess engine(nullptr, storage.path());
    QDir().mkpath(storage.path() + "/librespot");
    QFile credentials(storage.path() + "/librespot/credentials.json");
    require(credentials.open(QIODevice::WriteOnly), "fake cache file");
    credentials.write("{}");
    credentials.close();
    const auto counter = storage.filePath("child-runs.txt");
    qputenv("TONAL_TEST_RECOVERY_MODE", mode);
    qputenv("TONAL_TEST_RECOVERY_COUNTER", counter.toUtf8());
    engine.m_exe = QCoreApplication::applicationFilePath();
    engine.m_restartBaseMs = 20;
    if (mode == "stall-once" || mode == "paused")
      engine.m_stallTimeoutMs = 400;
    engine.m_healthTimer.setInterval(20);
    engine.setAudioEngine(&audio);
    int lost = 0, beats = 0;
    QObject::connect(&engine, &LibrespotProcess::sessionLost, &engine,
                     [&] { ++lost; });
    QTimer heartbeat;
    heartbeat.setInterval(10);
    QObject::connect(&heartbeat, &QTimer::timeout, &engine, [&] { ++beats; });
    heartbeat.start();
    engine.start();
    if (mode == "paused") {
      require(spin([&] { return engine.state() == LibrespotProcess::Ready; }),
              "paused child not ready");
      wait(800);
      require(lost == 0 && countRuns(counter) == 1,
              "watchdog restarted an intentionally paused player");
    } else if (mode == "always-crash") {
      require(spin(
                  [&] {
                    return engine.m_autoRestartBlocked &&
                           engine.m_process.state() == QProcess::NotRunning;
                  },
                  8000),
              "crash loop not bounded");
      require(countRuns(counter) == 4, "authentication reset crash budget");
      qputenv("TONAL_TEST_RECOVERY_MODE", "healthy");
      engine.restart();
      require(spin([&] {
                return engine.state() == LibrespotProcess::Ready &&
                       audio.spotifyFramesRead() > 4096;
              }),
              "explicit restart failed after crash budget");
    } else {
      require(spin(
                  [&] {
                    return countRuns(counter) >= 2 &&
                           engine.state() == LibrespotProcess::Ready &&
                           audio.spotifyFramesRead() > 4096;
                  },
                  8000),
              "audio did not recover from injected failure");
      require(lost == 1 && countRuns(counter) == 2 && beats > 2,
              "recovery duplicated restarts or blocked GUI heartbeat");
    }
    engine.stop();
    require(!engine.m_restartTimer.isActive() &&
                !engine.m_healthTimer.isActive(),
            "stop retained a recovery timer");
    const auto safe = LibrespotProcess::safeLog(
        "https://accounts.spotify.com/?code=private-code bearer secret-token "
        "access_token=private-access refresh_token=private-refresh "
        "password=private-password");
    require(!safe.contains("private-") && !safe.contains("secret-token"),
            "diagnostics contain credentials");
    QFile log(storage.path() + "/diagnostics/playback.log");
    require(log.open(QIODevice::ReadOnly) && log.size() > 0,
            "persistent diagnostics missing");
    qunsetenv("TONAL_TEST_RECOVERY_MODE");
    qunsetenv("TONAL_TEST_RECOVERY_COUNTER");
    std::cout << "PASS native PCM/process recovery mode=" << mode.constData()
              << " heartbeat=" << beats << '\n';
  }
};

static int producer() {
#ifdef _WIN32
  const auto mode = qgetenv("TONAL_TEST_RECOVERY_MODE");
  const QString counter =
      QString::fromUtf8(qgetenv("TONAL_TEST_RECOVERY_COUNTER"));
  const int run = SpotifyRecoveryTests::countRuns(counter) + 1;
  QFile file(counter);
  if (!file.open(QIODevice::WriteOnly))
    return 20;
  file.write(QByteArray::number(run));
  file.close();
  if (qgetenv("RUST_LOG").contains("librespot_core::session=info"))
    std::cerr << "[INFO librespot_core] Authenticated as fake-local-test\n";
  std::cerr
      << "[DEBUG librespot_playback::player] command=Load { play: true }\n"
      << std::flush;
  if (mode == "always-crash" || (mode == "crash-once" && run == 1))
    return 17;
  if (mode == "paused")
    std::cerr << "[DEBUG librespot_playback::player] command=Pause\n"
              << std::flush;
  if (mode == "panic-once" && run == 1)
    std::cerr << "thread 'audio' panicked at simulated local sink\n"
              << std::flush;
  HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  if (mode == "eof-once" && run == 1) {
    CloseHandle(output);
    output = INVALID_HANDLE_VALUE;
  }
  const bool silent =
      mode == "paused" ||
      ((mode == "stall-once" || mode == "panic-once" || mode == "eof-once") &&
       run == 1);
  float pcm[1024];
  std::fill(std::begin(pcm), std::end(pcm), .001f);
  for (int n = 0; n < 3000; ++n) {
    if (!silent) {
      DWORD written = 0;
      if (!WriteFile(output, pcm, sizeof(pcm), &written, nullptr))
        return 18;
    }
    Sleep(10);
  }
#endif
  return 0;
}
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const bool live = app.arguments().contains("--live-session") ||
                    app.arguments().contains("--ready-only");
  app.setOrganizationName(live ? "Tonal" : "TonalRecoveryTests");
  app.setApplicationName(live ? "Tonal" : "Playback");
  if (app.arguments().contains("--name"))
    return producer();
  if (!live && !app.arguments().contains("--local-only"))
    return 30;
  qputenv("ALSOFT_DRIVERS", "null");
  if (!live)
    qputenv("RUST_LOG", "warn");
  QJsonObject report;
  if (live)
    report = SpotifyRecoveryTests::liveSession(
        app.arguments().contains("--ready-only"));
  else {
    try {
      SpotifyRecoveryTests::networkAndResume();
      SpotifyRecoveryTests::deadlines();
#ifdef _WIN32
      for (const auto *mode : {"crash-once", "eof-once", "stall-once",
                               "panic-once", "paused", "always-crash"})
        SpotifyRecoveryTests::pipeline(mode);
#endif
      std::cout << "SPOTIFY_RECOVERY_PASS\n";
      report = {{"passed", true},
                {"real_spotify_session", false},
                {"credentials_read", false},
                {"scenarios",
                 QJsonArray{"unexpected_process_exit",
                            "PCM_EOF_while_child_alive", "no_PCM_progress",
                            "Rust_panic", "intentional_pause_not_restarted",
                            "bounded_crash_loop_and_explicit_restart",
                            "API_and_token_total_deadlines",
                            "resume_position_and_queue",
                            "new_selection_during_reconnect",
                            "cancelled_resume_does_not_reissue_play",
                            "GUI_heartbeat_during_recovery"}}};
    } catch (const std::exception &error) {
      std::cerr << "FAIL " << error.what() << '\n';
      report = {{"passed", false}, {"error", QString::fromUtf8(error.what())}};
    }
  }
  report["recorded_at_utc"] =
      QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
  int reportArg = app.arguments().indexOf("--report");
  if (reportArg >= 0) {
    QFile file(app.arguments().value(reportArg + 1));
    if (!file.open(QIODevice::WriteOnly))
      return 31;
    file.write(QJsonDocument(report).toJson());
  }
  std::cout << QJsonDocument(report).toJson().constData();
  return report["passed"].toBool() ? 0 : report.contains("skipped") ? 3 : 1;
}
