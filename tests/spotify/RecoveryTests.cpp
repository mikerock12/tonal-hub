#include "AuthManager.h"
#include "LibrespotProcess.h"
#include "Player.h"
#include "SpotifyApi.h"
#include "playback/ListeningMix.hpp"
#include "companion/MusicCompanion.h"
#include "audio/AudioEngine.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
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
#include <limits>
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
  // Catalog and queue reads only. Never starts an engine or changes playback.
  static QJsonObject discoveryReadOnly() {
    QJsonObject report{{"passed", false}, {"read_only", true},
                       {"credentials_logged", false}};
    try {
      QNetworkAccessManager network;
      AuthManager auth(&network);
      SpotifyApi api(&network, &auth);
      if (!auth.isAuthenticated()) {
        report["skipped"] = "No saved Web API session";
        return report;
      }
      require(spin([&] { return auth.hasValidAccessToken() || !auth.isRefreshing(); }, 25000)
                  && auth.hasValidAccessToken(), "Cannot refresh saved catalog session");
      bool received = false;
      int status = 0;
      QJsonObject queue;
      api.get("/me/player/queue", {}, [&](int s, const QJsonDocument &d) {
        status = s; queue = d.object(); received = true;
      });
      require(spin([&] { return received; }, 15000), "Queue read timed out");
      report["queue_http"] = status;
      // Exercise the production queue resolver, including partial Connect queues.
      received = false;
      QJsonObject playerState;
      api.get("/me/player", {}, [&](int s, const QJsonDocument &d) {
        if (s == 200) playerState = d.object(); received = true;
      });
      require(spin([&] { return received; }, 15000), "Playback state read timed out");
      if (!playerState.isEmpty()) {
        QTemporaryDir processStorage;
        LibrespotProcess engine(nullptr, processStorage.path());
        Player player(&api, &engine, &auth);
        player.m_trackUri = playerState["item"].toObject()["uri"].toString();
        player.m_contextUri = playerState["context"].toObject()["uri"].toString();
        int resolved = -1;
        QObject::connect(&player, &Player::queueRefreshFinished, &player, [&](int s) { resolved = s; });
        player.refreshQueue();
        require(spin([&] { return resolved >= 0; }, 25000), "Resolved queue read timed out");
        report["resolved_queue_http"] = resolved;
        report["resolved_queue_count"] = player.queue().size();
        report["context_type"] = player.m_contextUri.section(':', 1, 1);
        if (resolved == 200) {
          QJsonArray items;
          for (const auto &v : player.queue()) {
            const auto t = v.toMap();
            items.append(QJsonObject{{"uri", t["uri"].toString()}, {"name", t["name"].toString()},
              {"artists", QJsonArray{QJsonObject{{"name", t["artists"].toString()}, {"id", t["artistId"].toString()}}}},
              {"album", QJsonObject{{"id", t["albumId"].toString()}, {"name", t["album"].toString()}, {"release_date", QString::number(t["year"].toInt())}}}});
          }
          queue["queue"] = items;
          queue["currently_playing"] = playerState["item"];
        }
      }
      QJsonArray probes;
      for (const QString q : {QString("genre:\"grunge\""), QString("grunge"), QString("genre:\"alternative rock\"")}) {
        received = false;
        api.get("/search", {{"q", q}, {"type", "track"}, {"limit", 10}},
          [&](int s, const QJsonDocument &d) {
            const auto tracks = d.object()["tracks"].toObject();
            QJsonArray titles;
            for (const auto &v : tracks["items"].toArray()) titles.append(v.toObject()["name"]);
            probes.append(QJsonObject{{"query", q}, {"http", s}, {"total", tracks["total"]}, {"titles", titles}});
            received = true;
          });
        require(spin([&] { return received; }, 15000), "Catalog probe timed out");
      }
      report["catalog_probes"] = probes;
      QJsonArray cases;
      auto check = [&](QVariantList source, const QString &name) {
        QTemporaryDir storage;
        MusicCompanion music(nullptr, storage.path());
        ListeningMix mix(&api, &music);
        mix.preview(source, 0, false, true);
        require(spin([&] { return !mix.busy(); }, 30000), "Discovery preview timed out");
        int outside = 0;
        for (const auto &row : mix.previewQueue())
          outside += row.toMap()["mixExternal"].toBool();
        cases.append(QJsonObject{{"case", name}, {"source_count", source.size()},
          {"queue_count", mix.previewQueue().size()}, {"outside_count", outside},
          {"status", mix.status()}});
      };
      if (status == 200) {
        QVariantList source;
        auto items = queue["queue"].toArray();
        items.prepend(queue["currently_playing"]);
        for (const auto &v : items) {
          const auto t = v.toObject(), album = t["album"].toObject();
          const auto artists = t["artists"].toArray();
          if (!t["uri"].toString().startsWith("spotify:track:")) continue;
          QStringList names;
          for (const auto &a : artists) names << a.toObject()["name"].toString();
          source << QVariantMap{{"provider", "Spotify"}, {"id", t["uri"].toString()},
            {"uri", t["uri"].toString()}, {"title", t["name"].toString()},
            {"artist", names.join(", ")}, {"artistId", artists.first().toObject()["id"].toString()},
            {"albumId", album["id"].toString()}, {"album", album["name"].toString()},
            {"year", album["release_date"].toString().left(4).toInt()}};
          if (source.size() == 6) break;
        }
        if (!source.isEmpty()) check(source, "current_queue");
      }
      // Stable user-requested artist; inspect the same catalog path used by the UI.
      received = false;
      QVariantList source;
      api.get("/search", {{"q", "artist:Hole"}, {"type", "track"}, {"limit", 6}},
        [&](int s, const QJsonDocument &doc) {
          status = s;
          for (const auto &v : doc.object()["tracks"].toObject()["items"].toArray()) {
            const auto t = v.toObject(), album = t["album"].toObject();
            source << QVariantMap{{"provider", "Spotify"}, {"id", t["uri"].toString()},
              {"uri", t["uri"].toString()}, {"artist", "Hole"}, {"title", t["name"].toString()},
              {"artistId", t["artists"].toArray().first().toObject()["id"].toString()},
              {"albumId", album["id"].toString()},
              {"year", album["release_date"].toString().left(4).toInt()}};
          }
          received = true;
        });
      require(spin([&] { return received; }, 15000) && status == 200, "Cannot read test artist tracks");
      check(source, "Hole_catalog");
      report["cases"] = cases;
      report["passed"] = std::all_of(cases.begin(), cases.end(), [](const auto &v) {
        return v.toObject()["source_count"].toInt() < 2 || v.toObject()["outside_count"].toInt() > 0;
      });
    } catch (const std::exception &e) { report["error"] = QString::fromUtf8(e.what()); }
    return report;
  }
  // Real Connect session on the isolated diagnostic device (null audio
  // driver): the ordered mix, a reapplied order and the repeat modes must be
  // what Spotify reports back, not only what the UI shows.
  static QStringList liveQueue(SpotifyApi &api) {
    bool received = false;
    QStringList uris;
    api.get("/me/player/queue", {}, [&](int code, const QJsonDocument &doc) {
      if (code == 200)
        for (const auto value : doc.object()["queue"].toArray())
          uris << value.toObject()["uri"].toString();
      received = true;
    });
    spin([&] { return received; }, 15000);
    return uris;
  }
  static QJsonObject liveState(SpotifyApi &api) {
    bool received = false;
    QJsonObject state;
    api.get("/me/player", {}, [&](int, const QJsonDocument &doc) {
      state = doc.object();
      received = true;
    });
    spin([&] { return received; }, 15000);
    return state;
  }
  static QJsonArray toJson(const QStringList &list) {
    QJsonArray out;
    for (const auto &s : list)
      out.append(s);
    return out;
  }
  static void verifyMixLive(SpotifyApi &api, Player &player,
                            tonal::audio::AudioEngine &audio,
                            const QStringList &tracks, QJsonObject &report) {
    require(tracks.size() >= 4, "Need four recent tracks for the mix check");
    const QStringList order = tracks.mid(0, 4);
    player.playOrderedUris(order);
    require(spin([&] { return player.trackUri() == order[0] && player.isPlaying(); }, 20000),
            "Ordered mix did not start on its first track");
    wait(2500);
    const auto first = liveQueue(api);
    report["ordered_queue"] = toJson(first.mid(0, 3));
    report["ordered_queue_expected"] = toJson(order.mid(1, 3));
    report["ordered_shuffle_state"] = liveState(api)["shuffle_state"].toBool();
    require(first.mid(0, 3) == order.mid(1, 3), "Ordered mix not reflected in the Spotify queue");
    const QStringList shaken{order[0], order[3], order[1], order[2]};
    auto frames = audio.spotifyFramesRead();
    player.playOrderedUrisAt(shaken, player.positionMs());
    wait(3000);
    const auto second = liveQueue(api);
    report["requeue_queue"] = toJson(second.mid(0, 3));
    report["requeue_expected"] = toJson(shaken.mid(1, 3));
    report["requeue_kept_track"] = player.trackUri() == order[0];
    report["requeue_pcm_frames_3s"] = qint64(audio.spotifyFramesRead() - frames);
    require(second.mid(0, 3) == shaken.mid(1, 3), "Reapplied order not reflected in the Spotify queue");
    QJsonArray repeats;
    for (const char *expected : {"context", "track", "off"}) {
      player.cycleRepeat();
      wait(2500);
      const auto state = liveState(api);
      repeats.append(QJsonObject{{"expected", expected},
                                 {"spotify", state["repeat_state"].toString()},
                                 {"ui", player.repeatMode()}});
      require(state["repeat_state"].toString() == QLatin1String(expected) &&
                  player.repeatMode() == QLatin1String(expected),
              "Repeat mode not applied by the real session");
      if (QLatin1String(expected) == QLatin1String("track")) {
        // Track repeat: after the end, the same track starts again.
        const int duration = player.durationMs();
        player.seek(qMax(0, duration - 4000));
        wait(9000);
        const auto after = liveState(api);
        const bool same = after["item"].toObject()["uri"].toString() == order[0];
        report["repeat_track_restarted_same"] = same;
        report["repeat_track_progress_after_end"] = after["progress_ms"].toInt();
        require(same && after["progress_ms"].toInt() < 9000, "Repeat track did not restart the same track");
      }
    }
    report["repeat_cycle"] = repeats;
  }
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
      const bool verifyTransitions = QCoreApplication::arguments().contains("--verify-transitions");
      if (verifyTransitions) {
        // Exercise a COPY of the user's cache/settings; never remove or change
        // production cached audio, saved login, or DSP preferences.
        const auto root = QFileInfo(saved).absolutePath();
        QDirIterator files(root + "/files", QDir::Files, QDirIterator::Subdirectories);
        qint64 bytes = 0;
        int count = 0;
        while (files.hasNext()) {
          const auto input = files.next();
          bytes += QFileInfo(input).size();
          require(++count <= 128 && bytes <= 128 * 1024 * 1024,
                  "Diagnostic cache copy exceeds its safety bound");
          const auto output = storage.path() + "/librespot/" + QDir(root).relativeFilePath(input);
          require(QDir().mkpath(QFileInfo(output).absolutePath()) && QFile::copy(input, output),
                  "Could not copy isolated cached audio");
        }
        report["copied_cache_files"] = count;
        report["copied_cache_bytes"] = bytes;
        const auto settings = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/audio.json";
        if (QFileInfo::exists(settings)) {
          QDir().mkpath(storage.path() + "/audio");
          require(QFile::copy(settings, storage.path() + "/audio/audio.json"), "DSP settings copy failed");
        }
      }
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
      if (QCoreApplication::arguments().contains("--verify-mix")) {
        verifyMixLive(api, player, audio, tracks, report);
        player.cancelResume();
        player.m_isPlaying = false;
        received = false;
        api.put("/me/player/pause", {{"device_id", player.localDeviceId()}}, {},
                [&](int code, const QJsonDocument &) { status = code; received = true; });
        spin([&] { return received; }, 15000);
        engine.stop();
        report["passed"] = true;
        return report;
      }
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
      if (verifyTransitions) {
        const int before = recoveries;
        int transitions = 0;
        for (int n = 0; n < 4; ++n) {
          const auto uri = tracks[n % 2];
          auto frames = audio.spotifyFramesRead();
          player.playUris({uri});
          require(spin([&] { return player.trackUri() == uri &&
                       audio.spotifyFramesRead() > frames + 96000 &&
                       player.isPlaying() && player.playingHere(); }, 20000),
                  "Real track change/return did not produce PCM");
          frames = audio.spotifyFramesRead();
          player.seek(30000 + n * 1000);
          require(spin([&] { return audio.spotifyFramesRead() > frames + 96000; }, 15000),
                  "Real seek stopped PCM");
          require(audio.metrics().value("invalidSamples").toDouble() == 0 &&
                      audio.metrics().value("peakDbfs", -120).toDouble() > -100,
                  "Real PCM was invalid or remained silent");
          ++transitions;
        }
        require(recoveries == before, "Unexpected recovery during track/seek verification");
        report["track_returns_and_seeks"] = transitions;
        report["metrics"] = QJsonObject::fromVariantMap(audio.metrics());
        report["unexpected_recoveries_during_transitions"] = recoveries - before;
        wait(2000); // exclude deliberate seeks/restarts from the steady phase
        const auto steadyFrames = audio.spotifyFramesRead();
        const int steadyUnderruns = audio.metrics().value("underruns").toInt();
        wait(15000);
        const auto steadyAdvanced = audio.spotifyFramesRead() - steadyFrames;
        const int underrunDelta = audio.metrics().value("underruns").toInt() - steadyUnderruns;
        report["steady_seconds"] = 15;
        report["steady_pcm_frames"] = qint64(steadyAdvanced);
        report["steady_underruns"] = underrunDelta;
        require(steadyAdvanced > 44100 * 14 && underrunDelta == 0 &&
                    recoveries == before && audio.metrics().value("invalidSamples").toDouble() == 0,
                "Steady playback stalled, underrun, or produced invalid PCM");
      }
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
  static void volumeControl() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network);
    SpotifyApi api(&network, &auth);
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    fakeAuth(auth);
    player.m_trackUri = "spotify:track:volume-fixture";
    QJsonObject state{{"is_playing",false},{"progress_ms",0},
      {"device",QJsonObject{{"id","volume-device"},{"volume_percent",60}}},
      {"item",QJsonObject{{"uri",player.m_trackUri},{"duration_ms",200000}}}};
    player.applyState(state);
    QList<int> values;
    QList<QString> devices;
    int responseDelay = 320, responseStatus = 204, pollDelay = 0;
    network.respond = [&](const QNetworkRequest &req, const QByteArray &) {
      if (req.url().path().endsWith("/volume")) {
        QUrlQuery query(req.url());
        values << query.queryItemValue("volume_percent").toInt();
        devices << query.queryItemValue("device_id");
        return Response{responseStatus,{},false,responseDelay};
      }
      if (req.url().path() == "/v1/me/player")
        return Response{200,QJsonDocument(state).toJson(),false,pollDelay};
      return Response{};
    };
    // A long gesture must send while still moving, without overlapping PUTs.
    for (int i=0;i<24;++i) {
      player.setVolume(65+i%18);
      spin([]{return false;},30);
      player.applyState(state);
      require(player.volume()==65+i%18,"stale volume changed a live drag");
    }
    require(values.size()>=2 && values.size()<=3,"volume is debounced until release or requests overlap");
    player.setVolume(23);
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},1800),"latest volume never dispatched");
    require(values.last()==23 && player.volume()==23,"last drag value was lost");
    require(std::all_of(devices.begin(),devices.end(),[](const auto &d){return d=="volume-device";}),"volume targeted another device");
    player.applyState(state);
    require(player.volume()==23,"post-ACK stale poll pulled volume back");
    player.m_pollTimer.stop();

    // A GET created before the write must remain obsolete even after the guard.
    responseDelay=20;pollDelay=450;
    player.refresh();player.setVolume(41);
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},700),"second volume write stuck");
    player.m_volumeIntentClock.invalidate();
    require(spin([&]{return !player.m_requestInFlight;},700),"delayed volume GET stuck");
    require(player.volume()==41,"pre-write GET applied after the settle window");
    player.m_pollTimer.stop();
    state["device"]=QJsonObject{{"id","volume-device"},{"volume_percent",52}};
    player.applyState(state);
    require(player.volume()==52,"external Connect volume never reconciled");

    // Coalesce zero/100 endpoints, and do not leak a pending write on transfer.
    player.setVolume(-500);player.setVolume(500);
    require(player.volume()==100,"volume endpoints not clamped");
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},800),"clamped volume write stuck");
    require(values.last()==100,"clamped target changed in transport");
    player.setVolume(0);
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},800)&&values.last()==0,"mute not exact zero");
    int before=values.size();
    player.setVolume(61);
    state["device"]=QJsonObject{{"id","other-device"},{"volume_percent",17}};
    player.applyState(state);spin([]{return false;},180);
    require(values.size()==before && player.volume()==17,"pending volume leaked across devices");

    responseDelay=320;player.setVolume(44);
    require(spin([&]{return values.size()>before;},600),"old route request missing");
    state["device"]=QJsonObject{{"id","third-device"},{"volume_percent",15}};
    player.applyState(state);responseDelay=20;player.setVolume(42);
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},800),"new route volume stuck");
    spin([]{return false;},350);
    require(player.volume()==42 && values.last()==42 && devices.last()=="third-device",
            "old route ACK affected the new route");
    before=values.size();player.m_pollTimer.stop();

    // A superseded 429 must release the serialized slot, not retry old loudness.
    responseStatus=429;responseDelay=20;pollDelay=0;
    player.setVolume(67);
    require(spin([&]{return values.size()>before;},600),"rate-limited write missing");
    spin([]{return false;},60);responseStatus=204;player.setVolume(29);
    require(spin([&]{return !player.m_volumeInFlight && player.m_pendingVolume<0;},1800),"superseded retry blocked latest volume");
    require(values.size()==before+2 && values.last()==29,"obsolete volume retried after supersession");
    player.m_pollTimer.stop();
    responseStatus=403;QString notice;
    QObject::connect(&player,&Player::message,&player,[&](const auto &s){notice=s;});
    player.setVolume(30);
    require(spin([&]{return !notice.isEmpty();},800),"refused volume failed silently");
    require(!player.m_volumeInFlight && !player.m_volumeIntentClock.isValid(),"refused volume left reconciliation blocked");
    player.cancelVolume();player.m_pollTimer.stop();
    std::cout << "PASS volume: throttle, serialized latest value, stale GETs, endpoints, device transfer, retry supersession and refusal\n";
  }
  static void remoteTransport() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network);
    SpotifyApi api(&network, &auth);
    tonal::audio::AudioEngine audio(nullptr, storage.path()+"/audio");
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    player.setAudioEngine(&audio);
    fakeAuth(auth);
    player.m_localDeviceId="local-device";
    player.m_trackUri="spotify:track:fixture";
    QJsonObject state{{"is_playing",true},{"progress_ms",30000},
      {"device",QJsonObject{{"id","local-device"}}},
      {"item",QJsonObject{{"uri","spotify:track:fixture"}}}};
    audio.setSpotifyPaused(true);
    player.applyState(state);
    require(!audio.spotifyPaused(),"remote resume left the native sink paused");
    state["is_playing"]=false;player.applyState(state);
    require(audio.spotifyPaused(),"remote pause left the native sink running");
    state["is_playing"]=true;state["device"]=QJsonObject{{"id","other-device"}};
    player.applyState(state);
    require(audio.spotifyPaused(),"foreign device resumed the local sink");
    state["device"]=QJsonObject{{"id","local-device"}};
    player.applyState(state);player.pause();player.applyState(state);
    require(audio.spotifyPaused(),"eventually consistent state undid a recent pause");
    player.m_transportIntentClock.invalidate();player.applyState(state);
    require(!audio.spotifyPaused(),"confirmed remote resume failed after intent guard");
    const auto oldState=QJsonDocument(state).toJson();
    network.respond=[oldState](const QNetworkRequest &req,const QByteArray &){
      return req.url().path().endsWith("/player") ? Response{200,oldState,false,100} : Response{};
    };
    player.refresh();player.pause();
    require(spin([&]{return !player.m_requestInFlight;}),"stale state request stuck");
    require(!player.m_isPlaying&&audio.spotifyPaused(),"in-flight GET overrode explicit pause");
    int plays=0,shuffles=0;QByteArray played;network.respond=[&](const QNetworkRequest&req,const QByteArray&body){if(req.url().path().endsWith("/shuffle")){++shuffles;return Response{};}if(req.url().path().endsWith("/play")){++plays;played=body;return Response{204,{},false,65};}return Response{};};
    player.playOrderedUris({"spotify:track:old-mix"});player.pause();spin([]{return false;},120);require(plays==1&&shuffles==0&&audio.spotifyPaused(),"pending mix overrode explicit pause");
    plays=0;player.playOrderedUris({"spotify:track:old-mix"});player.playUris({"spotify:track:new-selection"});spin([]{return false;},120);require(plays==2&&shuffles==0&&played.contains("new-selection"),"late ordered response changed new selection");
    std::cout<<"PASS remote Connect pause/resume, foreign device and stale poll guards\n";
  }
  // 0.13.0: drag seeks become one request, a late GET cannot pull the bar
  // back, a pending seek never reaches the next track, and reapplying the
  // queue at the current track uses "now + measured latency".
  static void seekAndRequeue() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network);
    SpotifyApi api(&network, &auth);
    tonal::audio::AudioEngine audio(nullptr, storage.path()+"/audio");
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    player.setAudioEngine(&audio);
    fakeAuth(auth);
    player.m_localDeviceId="local-device";
    QJsonObject state{{"is_playing",true},{"progress_ms",30000},
      {"device",QJsonObject{{"id","local-device"}}},
      {"item",QJsonObject{{"uri","spotify:track:fixture"},{"duration_ms",200000}}}};
    player.applyState(state);
    QList<int> seeks;QByteArray playBody;
    network.respond=[&](const QNetworkRequest &req,const QByteArray &body){
      if(req.url().path().endsWith("/seek"))seeks<<QUrlQuery(req.url()).queryItemValue("position_ms").toInt();
      if(req.url().path().endsWith("/play"))playBody=body;
      return Response{};
    };
    player.seek(60000);player.seek(90000);player.seek(120000);
    require(qAbs(player.positionMs()-120000)<250,"seek target not shown immediately");
    require(spin([&]{return seeks.size()==1;},800)&&seeks.first()==120000,"drag seeks were not coalesced into the last position");
    spin([]{return false;},150);
    require(seeks.size()==1,"extra seek request after release");
    player.applyState(state);
    require(qAbs(player.positionMs()-120000)<1000,"stale GET pulled the bar back after seek");
    state["progress_ms"]=120400;player.applyState(state);
    require(qAbs(player.positionMs()-120400)<250,"confirmed remote position ignored");
    player.seek(150000);player.next();spin([]{return false;},150);
    require(seeks.size()==1,"pending seek leaked into the next track");
    player.applyState(state);player.m_commandLatencyMs=300;
    const int before=player.positionMs();
    player.playOrderedUrisAt({"spotify:track:fixture","spotify:track:next"},1000);
    require(spin([&]{return !playBody.isEmpty();},800),"requeue request not sent");
    const int sent=QJsonDocument::fromJson(playBody).object()["position_ms"].toInt();
    require(sent>=before+300&&sent<before+300+400,"requeue did not use current position plus latency");
    std::cout<<"PASS seek coalescing, stale poll guard and requeue position\n";
  }
  // 0.13.1: repeat survives new selections and stale polls, a failed change
  // is reverted and explained, skipping leaves track repeat, the applied mix
  // is the queue shown, and the player buttons apply modes directly.
  static void repeatAndMixQueue() {
    QTemporaryDir storage;
    FakeNetwork network;
    AuthManager auth(&network);
    SpotifyApi api(&network, &auth);
    tonal::audio::AudioEngine audio(nullptr, storage.path()+"/audio");
    LibrespotProcess engine(nullptr, storage.path());
    Player player(&api, &engine, &auth);
    player.setAudioEngine(&audio);
    fakeAuth(auth);
    player.m_localDeviceId="local-device";
    QJsonObject state{{"is_playing",true},{"progress_ms",30000},{"repeat_state","off"},
      {"device",QJsonObject{{"id","local-device"}}},
      {"item",QJsonObject{{"uri","spotify:track:a"},{"duration_ms",200000}}}};
    player.applyState(state);
    QStringList repeats;int plays=0,queueGets=0,repeatStatus=204;
    network.respond=[&](const QNetworkRequest &req,const QByteArray &){
      const auto path=req.url().path();
      if(path.endsWith("/repeat")){repeats<<QUrlQuery(req.url()).queryItemValue("state");return Response{repeatStatus};}
      if(path.endsWith("/play"))++plays;
      if(path.endsWith("/queue"))++queueGets;
      return Response{};
    };
    player.cycleRepeat();
    require(player.repeatMode()=="context"&&spin([&]{return repeats.size()==1;},800)&&repeats[0]=="context","repeat change not sent");
    player.applyState(state);
    require(player.repeatMode()=="context","stale GET undid the repeat change");
    player.m_repeatClock.invalidate();
    player.playUris({"spotify:track:b"});
    require(spin([&]{return plays==1&&repeats.size()==2;},800)&&repeats[1]=="context","repeat not reapplied after a new selection");
    player.applyState(state);
    require(player.repeatMode()=="context","selection reset shown as repeat off");
    player.m_repeatClock.invalidate();player.m_selectionClock.invalidate();
    state["repeat_state"]="context";player.applyState(state);
    player.cycleRepeat();
    require(player.repeatMode()=="track"&&spin([&]{return repeats.size()==3;},800)&&repeats[2]=="track","track repeat not sent");
    player.next();
    require(player.repeatMode()=="context","skipping ahead must leave track repeat");
    QString notice;QObject::connect(&player,&Player::message,&player,[&](const QString &m){notice=m;});
    repeatStatus=403;player.cycleRepeat();
    require(spin([&]{return !notice.isEmpty();},800)&&player.repeatMode()=="context","failed repeat change not reverted and explained");
    repeatStatus=204;
    player.m_trackUri="spotify:track:a";
    auto row=[](const QString &id,bool external){return QVariant(QVariantMap{{"provider","Spotify"},{"uri","spotify:track:"+id},{"title","Title "+id},{"artist","Artist "+id},{"image","https://i/"+id},{"durationMs",180000},{"mixExternal",external}});};
    player.setOrderedQueue({row("a",false),row("x",true),row("c",false)});
    int finished=-1;QObject::connect(&player,&Player::queueRefreshFinished,&player,[&](int s){finished=s;});
    queueGets=0;player.refreshQueue();
    require(finished==200&&queueGets==0&&player.queue().size()==2&&player.queue()[0].toMap()["mixExternal"].toBool()&&player.queue()[0].toMap()["name"]=="Title x","applied mix not shown as the queue");
    bool replaced=false;QObject::connect(&player,&Player::selectionReplaced,&player,[&]{replaced=true;});
    player.playOrderedUrisAt({"spotify:track:a","spotify:track:c"},1000);
    require(!replaced&&!player.m_orderedRows.isEmpty(),"an applied mix was treated as a plain selection");
    player.playUris({"spotify:track:z"});
    require(replaced&&player.m_orderedRows.isEmpty(),"a plain selection kept the old mix queue");

    MusicCompanion music(nullptr, storage.path()+"/companion");
    ListeningMix mix(&api,&music);
    QVariantList source;for(int i=0;i<6;++i)source<<QVariantMap{{"provider","Spotify"},{"id",QString("spotify:track:s%1").arg(i)},{"uri",QString("spotify:track:s%1").arg(i)},{"title",QString("Song %1").arg(i)},{"artist","Hole"},{"albumId","album"},{"year",1994}};
    int resumed=0;QVariantList applied;QObject::connect(&mix,&ListeningMix::resumeReady,&mix,[&](const QVariantList &rows,int){++resumed;applied=rows;});
    bool reordered=false;
    for(int attempt=0;attempt<6&&!reordered;++attempt){
      mix.applyModes(source,true,false,false);
      require(resumed==attempt+1&&applied.size()==6&&applied[0]==source[0],"shake must apply now and keep the current song first");
      reordered=applied!=source;
    }
    require(reordered&&mix.queueFor("spotify:track:s0").size()==6,"shake did not change the applied queue");
    mix.applyModes(source,false,true,false);
    require(mix.randomEnabled()&&applied[0]==source[0],"random mode not applied from the current song");
    mix.forgetApplied();
    require(!mix.randomEnabled()&&mix.queueFor("spotify:track:s0").isEmpty(),"a plain selection kept the mix modes");
    network.respond=[](const QNetworkRequest &,const QByteArray &){return Response{200,{},true};};
    const int before=resumed;
    mix.applyModes(source,false,false,true);mix.cancel();spin([]{return false;},150);
    require(resumed==before&&!mix.busy(),"cancelled discovery still replaced the queue");
    std::cout<<"PASS repeat persistence, ordered mix queue and direct mode buttons\n";
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
  static void listeningMix() {
    FakeNetwork network;AuthManager auth(&network);SpotifyApi api(&network,&auth);fakeAuth(auth);QTemporaryDir storage;MusicCompanion music(nullptr,storage.path());ListeningMix mix(&api,&music);int mutations=0,requests=0;bool hang=false,blocked=false;
    network.respond=[&](const QNetworkRequest&req,const QByteArray&body){++requests;if(!body.isEmpty())++mutations;if(hang)return Response{200,{},true};if(blocked)return Response{403,{}};require(req.url().path()=="/v1/search","mix used unsupported recommendation endpoint");QJsonArray candidates;for(int i=0;i<8;++i)candidates<<QJsonObject{{"uri",QString("spotify:track:external%1").arg(i)},{"name",QString("External song %1").arg(i)},{"artists",QJsonArray{QJsonObject{{"name",QString("New Band %1").arg(i)}}}},{"album",QJsonObject{{"id","other-album"},{"release_date","1994"}}}};return Response{200,QJsonDocument(QJsonObject{{"tracks",QJsonObject{{"total",80},{"items",candidates}}}}).toJson()};};
    QVariantList source;for(int i=0;i<6;++i)source<<QVariantMap{{"provider","Spotify"},{"id",QString("spotify:track:source%1").arg(i)},{"uri",QString("spotify:track:source%1").arg(i)},{"artist","Hole"},{"albumId","source-album"},{"genre","grunge"},{"year",1994}};int completions=0;QVariantList result;QObject::connect(&mix,&ListeningMix::ready,&mix,[&](const auto&v){result=v;++completions;});
    mix.play(source,0,false,true,true);require(spin([&]{return !mix.busy();},8000),"mix failed to complete");require(completions==1&&result.size()==9&&result[0]==source[0]&&result[1]==source[1]&&result[2].toMap()["uri"].toString().contains("external"),"2+1 queue cadence failed");require(mutations==0&&requests<=12,"mix changed remote playlist or exceeded query budget");
    const auto accepted=result;const int catalogBefore=requests;
    mix.preview(accepted,0,true,true,true);require(!mix.busy()&&mix.previewQueue().size()==9&&requests==catalogBefore,"random mode replaced accepted discoveries or fetched again");
    QSet<QString> expectedOutside,actualOutside;for(auto v:accepted)if(v.toMap()["mixExternal"].toBool())expectedOutside.insert(v.toMap()["uri"].toString());for(auto v:mix.previewQueue())if(v.toMap()["mixExternal"].toBool())actualOutside.insert(v.toMap()["uri"].toString());require(actualOutside==expectedOutside,"accepted discovery identities changed");
    auto shaken=source;std::reverse(shaken.begin(),shaken.end());mix.preview(shaken,0,false,true,true);require(!mix.busy()&&mix.previewQueue().size()==9&&requests==catalogBefore,"collection shake lost discoveries");
    mix.preview(accepted,0,true,false,true);require(mix.previewQueue().size()==6,"discovery toggle did not remove future extras");
    QVariantList longList;for(int i=0;i<650;++i){auto t=source[0].toMap();t["uri"]=QString("spotify:track:long%1").arg(i);t["id"]=t["uri"];longList<<t;}mix.play(longList,600,false,false,false);require(completions==2&&result.size()==50&&result.first().toMap()["uri"]=="spotify:track:long600","long personal playlist lost selected track");
    hang=true;mix.play(source,0,false,true,true);mix.cancel();spin([]{return false;},150);require(!mix.busy()&&completions==2,"cancelled discovery stole playback");hang=false;blocked=true;mix.play(source,0,false,true,true);require(spin([&]{return !mix.busy();})&&result==source&&completions==3,"API failure did not preserve source selection");
    const int beforePreview=completions;mix.preview(source,0,true,false);require(!mix.busy()&&completions==beforePreview&&mix.previewQueue().size()==6,"queue preview started playback");
    int applied=0,resume=-1;QObject::connect(&mix,&ListeningMix::resumeReady,&mix,[&](const auto& rows,int p){require(rows==mix.previewQueue(),"applied queue differs from preview");++applied;resume=p;});mix.applyPreview(12345);require(applied==1&&resume==12345,"queue position not preserved");
    blocked=false;
    auto single=source.mid(0,1);const int prior=requests;mix.preview(single,0,false,true);
    require(!mix.busy()&&requests==prior&&mix.status().contains("pelo menos duas"),"single-song queue silently failed discovery");
    mix.preview(source,0,false,true);require(spin([&]{return !mix.busy();},8000)&&mix.previewQueue().size()==9,"preview discovery failed");
    require(completions==beforePreview,"preview changed current playback");
    std::cout<<"PASS listening mix: 2+1, readonly bounded discovery, stale callbacks and offline fallback\n";
  }
  static void contextQueue() {
    FakeNetwork network; AuthManager auth(&network); fakeAuth(auth);
    SpotifyApi api(&network, &auth); QTemporaryDir storage;
    LibrespotProcess engine(nullptr, storage.path()); Player player(&api, &engine, &auth);
    player.m_contextUri="spotify:album:album-fixture";player.m_trackUri="spotify:track:source0";
    bool deny=false;
    network.respond=[&](const QNetworkRequest &request,const QByteArray &body){
      require(body.isEmpty(),"queue resolution mutated playback");
      if(request.url().path()=="/v1/me/player/queue")return Response{deny?403:200,"{\"queue\":[]}"};
      if(request.url().path()=="/v1/albums/album-fixture"){
        QJsonArray items;
        for(int i=0;i<6;++i)items.append(QJsonObject{{"uri",QString("spotify:track:source%1").arg(i)},
          {"name",QString("Source %1").arg(i)}, {"artists",QJsonArray{QJsonObject{{"id","artist-fixture"},{"name","Hole"}}}}});
        return Response{200,QJsonDocument(QJsonObject{{"id","album-fixture"},{"name","Live Through This"},{"release_date","1994-04-12"},
          {"tracks",QJsonObject{{"items",items}}}}).toJson()};
      }
      return Response{};
    };
    int done=-1;QObject::connect(&player,&Player::queueRefreshFinished,&player,[&](int status){done=status;});
    player.refreshQueue();require(spin([&]{return done>=0;})&&done==200&&player.queue().size()==5,"empty Connect queue did not resolve album context");
    auto row=player.queue()[0].toMap();require(row["uri"]=="spotify:track:source1"&&row["year"]==1994&&row["artistId"]=="artist-fixture"&&row["albumId"]=="album-fixture","queue lost discovery metadata");
    deny=true;done=-1;player.refreshQueue();require(spin([&]{return done>=0;})&&done==403,"queue failure left preview waiting");
    std::cout<<"PASS partial Connect queue resolves exact context and preserves discovery metadata\n";
  }
  static void playlistTransport() {
    FakeNetwork network;AuthManager auth(&network);SpotifyApi api(&network,&auth);fakeAuth(auth);
    int requests=0,received=-1;
    network.respond=[&](const QNetworkRequest &request,const QByteArray &body){
      ++requests;
      if(request.url().path().endsWith("/images")){
        require(request.header(QNetworkRequest::ContentTypeHeader).toString()=="image/jpeg"&&body=="/9j/test-base64","cover transport changed raw JPEG payload");return Response{202,{}};
      }
      return Response{503,{}};
    };
    api.raw("POST","/me/playlists",{},"{}","application/json",[&](int s,const auto &){received=s;});
    require(spin([&]{return received!=-1;}),"uncertain playlist POST did not finish");
    require(received==503&&requests==1,"uncertain playlist POST retried automatically");
    received=-1;api.raw("PUT","/playlists/fixture/images",{},"/9j/test-base64","image/jpeg",[&](int s,const auto &){received=s;});
    require(spin([&]{return received!=-1;})&&received==202,"cover upload failed");
    received=-1;api.raw("GET","https://other-origin.example/v1/playlists",{}, {},{},[&](int s,const auto &){received=s;});
    require(received==400&&requests==2,"catalog token could reach another origin");
    std::cout<<"PASS playlist raw transport: no uncertain POST retry, JPEG MIME and same-origin bearer guard\n";
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
    } else if (mode == "always-crash" || mode == "invalid-always") {
      require(spin(
                  [&] {
                    return engine.m_autoRestartBlocked &&
                           engine.m_process.state() == QProcess::NotRunning;
                  },
                  8000),
              "crash loop not bounded");
      require(countRuns(counter) == 4, "authentication reset crash budget");
      require(audio.advancedEnabled(), "recovery disabled the user's DSP settings");
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
      if ((mode == "invalid-once" && run == 1) || mode == "invalid-always")
        pcm[128] = std::numeric_limits<float>::quiet_NaN();
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
                    app.arguments().contains("--ready-only") ||
                    app.arguments().contains("--discovery-readonly");
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
  if (app.arguments().contains("--discovery-readonly"))
    report = SpotifyRecoveryTests::discoveryReadOnly();
  else if (live)
    report = SpotifyRecoveryTests::liveSession(
        app.arguments().contains("--ready-only"));
  else {
    try {
      SpotifyRecoveryTests::volumeControl();
      if (!app.arguments().contains("--volume-only")) {
        SpotifyRecoveryTests::networkAndResume();
        SpotifyRecoveryTests::remoteTransport();
        SpotifyRecoveryTests::seekAndRequeue();
        SpotifyRecoveryTests::repeatAndMixQueue();
        SpotifyRecoveryTests::deadlines();
        SpotifyRecoveryTests::playlistTransport();
        SpotifyRecoveryTests::listeningMix();
        SpotifyRecoveryTests::contextQueue();
#ifdef _WIN32
        for (const auto *mode : {"crash-once", "eof-once", "stall-once",
                                 "panic-once", "paused", "always-crash",
                                 "invalid-once", "invalid-always"})
          SpotifyRecoveryTests::pipeline(mode);
#endif
      }
      std::cout << "SPOTIFY_RECOVERY_PASS\n";
      report = {{"passed", true},
                {"real_spotify_session", false},
                {"credentials_read", false},
                {"scenarios",
                 QJsonArray{"unexpected_process_exit",
                            "volume_throttle_serialization_stale_polls_and_device_guard",
                            "PCM_EOF_while_child_alive", "no_PCM_progress",
                            "Rust_panic", "intentional_pause_not_restarted",
                            "invalid_PCM_recovered_without_disabling_DSP",
                            "persistent_invalid_PCM_recovery_bounded",
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
  if (app.arguments().contains("--volume-only")) {
    report["suite"] = "volume_only";
    report["scenarios"] = QJsonArray{"continuous_drag_throttled_not_debounced",
      "single_inflight_latest_value", "stale_poll_after_ack",
      "pre_write_GET_after_guard_expiry", "external_volume_reconciliation",
      "zero_and_100_percent", "pending_device_transfer_cancelled", "late_ACK_after_device_transfer",
      "superseded_429_not_retried", "refusal_visible_and_unblocked"};
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
