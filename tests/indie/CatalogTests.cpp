#include "audio/AudioEngine.hpp"
#include "indie/CatalogService.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

// Exercise the real request/dispatch/queue code without depending on a public
// service outage or bypassing the production HTTPS checks.
class TestReply : public QNetworkReply {
public:
  TestReply(const QNetworkRequest &request, int status, QByteArray body,
            QByteArray retryAfter, QObject *parent)
      : QNetworkReply(parent), m_body(body) {
    setRequest(request);
    setUrl(request.url());
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
    if (status >= 400)
      setError(QNetworkReply::ServiceUnavailableError,
               "simulated HTTP failure");
    if (!retryAfter.isEmpty())
      setRawHeader("Retry-After", retryAfter);
    open(QIODevice::ReadOnly);
    QTimer::singleShot(0, this, [this] {
      setFinished(true);
      emit readyRead();
      emit finished();
    });
  }
  void abort() override {
    setFinished(true);
    emit finished();
  }
  qint64 bytesAvailable() const override {
    return m_body.size() - m_offset + QIODevice::bytesAvailable();
  }
  bool isSequential() const override { return true; }

protected:
  qint64 readData(char *data, qint64 max) override {
    const auto size = std::min(max, qint64(m_body.size()) - m_offset);
    if (size <= 0)
      return -1;
    std::memcpy(data, m_body.constData() + m_offset, size);
    m_offset += size;
    return size;
  }

private:
  QByteArray m_body;
  qint64 m_offset = 0;
};
class IndieCatalogTests {
public:
  static void require(bool ok, const char *message) {
    if (!ok)
      throw std::runtime_error(message);
  }
  static void unit() {
    require(!CatalogService::publicUrl(QUrl("file:///C:/test.wav")),
            "file URL accepted");
    require(!CatalogService::publicUrl(QUrl("http://example.com")),
            "cleartext URL accepted");
    require(!CatalogService::publicUrl(QUrl("https://user:pass@example.com")),
            "embedded credentials accepted");
    require(CatalogService::publicUrl(QUrl("https://example.com/audio.mp3")),
            "HTTPS rejected");
    CatalogService c(nullptr, false);
    QJsonObject track{{"id", "test"},
                      {"title", "Example (Live)"},
                      {"is_streamable", true},
                      {"is_stream_gated", false},
                      {"user", QJsonObject{{"name", "Example Band"}}}};
    c.audius({{"data", QJsonArray{track, track}}}, "track");
    require(c.items().size() == 1, "provider duplicate not removed");
    require(c.items()[0].toMap()["playable"].toBool(),
            "public audio unavailable");
    track["id"] = "gated";
    track["is_stream_gated"] = true;
    c.audius({{"data", QJsonArray{track}}}, "track");
    require(!c.items()[1].toMap()["playable"].toBool(), "gated audio playable");
    track["id"] = "studio";
    track["title"] = "Example";
    track["is_stream_gated"] = false;
    c.audius({{"data", QJsonArray{track}}}, "track");
    require(c.items().size() == 3, "studio/live merged");
    track["id"] = "unknown";
    track.remove("is_streamable");
    c.audius({{"data", QJsonArray{track}}}, "track");
    require(!c.items().last().toMap()["playable"].toBool(),
            "missing stream permission assumed");
    const auto count = c.items().size();
    track.remove("id");
    c.audius({{"data", QJsonArray{track}}}, "track");
    require(c.items().size() == count,
            "missing ID became a synthetic recording");
    track["id"] = "bad-cover";
    track["artwork"] = QJsonObject{{"480x480", "file:///C:/private.png"}};
    c.audius({{"data", QJsonArray{track}}}, "track");
    require(c.items().last().toMap()["image"].toString().isEmpty(),
            "non-HTTPS cover accepted");
    c.musicbrainz({{"artists", QJsonArray{QJsonObject{
                                   {"id", "mb-1"},
                                   {"name", "Example Band"},
                                   {"disambiguation", "another artist"}}}}},
                  "artist");
    require(!c.items().last().toMap()["playable"].toBool(),
            "metadata treated as audio");
    c.subsonic({{"subsonic-response", QJsonObject{{"status", "failed"}}}});
    require(!c.messages().isEmpty(), "library auth failure hidden");
    c.m_server = "https://library.example/base";
    c.m_user = "tester";
    c.m_password = "example-test-password";
    auto url = c.subUrl("stream", {{"id", "track1"}});
    QUrlQuery auth(url);
    require(url.path() == "/base/rest/stream.view", "server base path lost");
    require(!url.toString().contains(c.m_password),
            "password exposed in request URL");
    require(auth.queryItemValue("t") ==
                QString::fromLatin1(
                    QCryptographicHash::hash(
                        (c.m_password + auth.queryItemValue("s")).toUtf8(),
                        QCryptographicHash::Md5)
                        .toHex()),
            "Subsonic token invalid");
    bool resolved = false;
    QObject::connect(&c, &CatalogService::playable, &c,
                     [&](const QUrl &, const QString &) { resolved = true; });
    QVariantMap previous{{"provider", "OpenSubsonic"},
                         {"id", "song"},
                         {"playable", true},
                         {"server", c.m_server},
                         {"account", "different-user"}};
    c.resolve(previous);
    require(!resolved, "old library account reused for playback");
    previous["account"] = c.m_user;
    previous["server"] = "https://old-library.example";
    c.resolve(previous);
    require(!resolved, "old library server reused for playback");
    c.jamendo({{"headers", QJsonObject{{"status", "error"}}}}, "track");
    require(c.messages().last().startsWith("Jamendo"),
            "Jamendo API error hidden");
    c.reset("new query");
    require(c.items().isEmpty() && !c.busy(), "reset failed");
    recovery();
    ranking();
    std::cout << "INDIE_UNIT_PASS\n";
  }
  static void pumpUntil(const std::function<bool()> &done,
                        int timeout = 10000) {
    QElapsedTimer time;
    time.start();
    while (!done() && time.elapsed() < timeout) {
      QEventLoop loop;
      QTimer::singleShot(10, &loop, &QEventLoop::quit);
      loop.exec();
    }
    require(done(), "request did not complete within test deadline");
  }
  static void recovery() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    require(CatalogService::retryDelay("12", 0, now) == 12000,
            "Retry-After seconds ignored");
    require(CatalogService::retryDelay("invalid", 1, now) == 5000,
            "backoff missing");
    auto date = QDateTime::currentDateTimeUtc()
                    .addSecs(60)
                    .toString(Qt::RFC2822Date)
                    .toLatin1();
    require(CatalogService::retryDelay(date, 0, now) >= 59000,
            "Retry-After date ignored");
    CatalogService c(nullptr, false);
    // Accelerate the queue tick only; backoff still uses the real 2/5 seconds.
    c.m_mbTimer.setInterval(10);
    c.m_provider = "MusicBrainz";
    const QUrl url("https://musicbrainz.org/ws/2/artist?query=test&fmt=json");
    const QByteArray body =
        R"({"count":1,"artists":[{"id":"test-artist","name":"Test Artist"}]})";
    int calls = 0;
    c.m_get = [&](const QNetworkRequest &req) {
      ++calls;
      return new TestReply(req, calls == 1 ? 503 : 200, body, {}, &c);
    };
    auto parse = [&](auto json) { c.musicbrainz(json, "artist"); };
    c.fetch("MusicBrainz", url, parse);
    pumpUntil([&] { return calls == 1 && !c.m_messages.isEmpty(); });
    require(c.busy() && !c.canRetry() && c.items().isEmpty(),
            "503 prematurely ended request");
    pumpUntil([&] { return !c.busy(); });
    require(calls == 2 && c.items().size() == 1 && c.messages().isEmpty() &&
                !c.canRetry(),
            "503 -> 200 recovery failed");
    require(!c.hasMore(), "last artist page offered pagination");
    c.reset("cached");
    c.fetch("MusicBrainz", url, parse);
    require(calls == 2 && c.items().size() == 1 && !c.busy(),
            "fresh metadata cache missed");
    c.m_mbCache.object(url.toString())->savedAt = now - 16 * 60 * 1000;
    c.reset("stale");
    c.m_get = [&](const QNetworkRequest &req) {
      ++calls;
      return new TestReply(req, 503, {}, "31", &c);
    };
    c.fetch("MusicBrainz", url, parse);
    pumpUntil([&] { return !c.busy(); });
    require(calls == 3 && c.canRetry() && c.items().size() == 1,
            "stale metadata fallback or long Retry-After failed");
    require(c.messages().join(" ").contains("anteriormente"),
            "stale data not labeled");
    c.m_get = [&](const QNetworkRequest &req) {
      ++calls;
      return new TestReply(req, 200, body, {}, &c);
    };
    c.retryFailed();
    const auto waitingCalls = calls;
    QEventLoop wait;
    QTimer::singleShot(40, &wait, &QEventLoop::quit);
    wait.exec();
    require(calls == waitingCalls && c.busy(),
            "manual retry bypassed server cooldown");
    c.m_mbNotBefore = 0; // Only the test advances the provider's cooldown.
    pumpUntil([&] { return !c.busy(); });
    require(calls == 4 && !c.canRetry() && c.messages().isEmpty() &&
                c.items().size() == 1,
            "manual retry duplicated results or retained failure");
    c.reset("bad request");
    c.m_mbCache.clear();
    c.m_get = [&](const QNetworkRequest &req) {
      ++calls;
      return new TestReply(req, 400, {}, {}, &c);
    };
    c.fetch("MusicBrainz", url, parse);
    pumpUntil([&] { return !c.busy(); });
    require(calls == 5 && c.canRetry(),
            "permanent 400 was automatically retried");
    c.reset("cancel");
    c.m_get = [&](const QNetworkRequest &req) {
      ++calls;
      return new TestReply(req, 503, {}, {}, &c);
    };
    c.fetch("MusicBrainz", url, parse);
    pumpUntil([&] { return calls == 6 && !c.messages().isEmpty(); });
    c.reset("new page");
    // Let the full old backoff expire: it must never touch the new page.
    QEventLoop cancelled;
    QTimer::singleShot(2100, &cancelled, &QEventLoop::quit);
    cancelled.exec();
    require(calls == 6 && !c.busy() && !c.canRetry() && c.items().isEmpty(),
            "cancelled retry leaked into new page");
    c.m_mbNotBefore = 0;
    c.fetch("MusicBrainz", url, parse);
    pumpUntil([&] { return !c.busy(); }, 10000);
    require(calls == 9 && c.canRetry(),
            "persistent 503 exceeded two automatic retries");
    c.m_route = {{"provider", "MusicBrainz"}, {"kind", "album"}};
    c.m_mbHasMore = true;
    require(!c.hasMore() && c.inDetail(),
            "non-paginated album detail offered more");
    require(!c.cacheable("OpenSubsonic",
                         QUrl("https://musicbrainz.org/ws/2/artist")),
            "private provider entered public metadata cache");
    std::cout << "INDIE_RECOVERY_PASS: 503 recovery, Retry-After, "
                 "cancellation, cache, retry limit, pagination\n";
  }
  static void ranking() {
    CatalogService c(nullptr, false);
    c.m_query = "Veruca Salt";
    const QVariantMap unrelated{{"provider", "Audius"},
                                {"kind", "artist"},
                                {"id", "noise"},
                                {"title", "DJ unrelated"}};
    const QVariantMap band{{"provider", "MusicBrainz"},
                           {"kind", "artist"},
                           {"id", "band"},
                           {"title", "Veruca Salt"}};
    const QVariantMap profile{{"provider", "Audius"},
                              {"kind", "artist"},
                              {"id", "profile"},
                              {"title", "Veruca Salt"}};
    const QVariantMap show{{"provider", "Live Music Archive"},
                           {"kind", "show"},
                           {"id", "show"},
                           {"title", "Live 1995"},
                           {"artist", "Veruca Salt"}};
    c.append(unrelated);
    c.append(profile);
    c.append(show);
    c.append(band);
    require(c.items().first().toMap()["id"] == "band" &&
                c.items().last().toMap()["id"] == "noise",
            "exact catalog artist was buried by arrival order");
    const auto ordered = c.items();
    c.reset("reversed arrival");
    c.append(band);
    c.append(show);
    c.append(profile);
    c.append(unrelated);
    require(c.items() == ordered, "ranking depends on HTTP completion order");
    require(CatalogService::relevance({{"kind", "artist"}, {"title", "Björk"}},
                                      "bjork") >= 1000,
            "accent-insensitive artist matching failed");
    require(CatalogService::relevance(show, c.query()) >
                CatalogService::relevance(unrelated, c.query()),
            "matching live show buried by unrelated profile");
    c.reset("detail");
    c.m_route = band;
    c.append(unrelated);
    c.append(band);
    require(c.items().first().toMap()["id"] == "noise",
            "detail track order was ranked");
    c.musicbrainz(
        {{"release-groups",
          QJsonArray{QJsonObject{{"id", "album"}, {"title", "Album"}}}}},
        "album");
    require(c.items().last().toMap()["artist"] == "Veruca Salt",
            "artist name lost on detail navigation");
    c.m_route.clear();
    c.reset("live search");
    c.archive(
        {{"response",
          QJsonObject{
              {"numFound", 3},
              {"docs", QJsonArray{QJsonObject{{"identifier", "collection"},
                                              {"mediatype", "collection"},
                                              {"creator", "Veruca Salt"}},
                                  QJsonObject{{"identifier", "live"},
                                              {"mediatype", "etree"},
                                              {"creator", "Veruca Salt"},
                                              {"title", "Live 1995"}},
                                  QJsonObject{{"identifier", "tribute"},
                                              {"mediatype", "etree"},
                                              {"creator", "Tribute Band"},
                                              {"title",
                                               "Tribute to Veruca Salt"}}}}}}});
    require(c.items().size() == 2 && c.items().first().toMap()["id"] == "live",
            "collections or mentions ranked as band concerts");
    require(c.items().last().toMap()["detail"].toString().contains("tributo") &&
                c.liveStatus().contains("1 nesta"),
            "tribute or exact artist coverage not labeled");
    c.reset("no shows");
    c.archive(
        {{"response", QJsonObject{{"numFound", 0}, {"docs", QJsonArray{}}}}});
    require(c.items().isEmpty() && c.liveStatus().contains("Nenhum show"),
            "zero live coverage hidden");
    c.reset("fan recordings");c.m_query="L7";
    c.archive({{"response",QJsonObject{{"numFound",2},{"docs",QJsonArray{
      QJsonObject{{"identifier","ajc02607_l7-1989-09-14"},{"mediatype","audio"},{"creator","L7"},{"title","L7 Live at Dreamerz 1989-09-14"}},
      QJsonObject{{"identifier","demo"},{"mediatype","audio"},{"creator","L7"},{"title","L7 Demos and Rehearsals"}}
    }}}}},"Internet Archive");
    require(c.items().size()==2&&c.items()[0].toMap()["curated"].toBool(),"Aadam audio concerts excluded or unranked");
    require(c.items()[1].toMap()["kind"]=="demo","demo incorrectly labeled as concert");
    c.reset("file variants");
    QJsonObject metadata{{"metadata",QJsonObject{{"creator","L7"},{"title","Example live recording"}}},{"files",QJsonArray{
      QJsonObject{{"name","01.flac"},{"format","24bit Flac"},{"title","Synthetic one"}},
      QJsonObject{{"name","01.mp3"},{"original","01.flac"},{"format","VBR MP3"},{"title","Synthetic one"},{"length","12.5"}},
      QJsonObject{{"name","02.ogg"},{"format","Ogg Vorbis"}},
      QJsonObject{{"name","03.mp3"},{"format","MP3"},{"private","true"}},
      QJsonObject{{"name","checksums.ffp"},{"format","Flac FingerPrint"}},
      QJsonObject{{"name","../escape.mp3"},{"format","MP3"}}
    }}};
    c.archiveFiles(metadata,"example","Internet Archive");
    require(c.items().size()==2&&c.items()[0].toMap()["id"]=="example/01.mp3"&&c.items()[0].toMap()["durationMs"]==12500,"audio variants not deduplicated or MP3 preference lost");
    c.reset("restricted recording");metadata["is_dark"]=true;c.archiveFiles(metadata,"example","Internet Archive");
    require(c.items().isEmpty(),"dark recording became playable");
    c.m_query = "Nirvana";
    c.musicbrainz({{"artists", QJsonArray{QJsonObject{{"id", "aaa-less-known"},
                                                      {"name", "Nirvana"},
                                                      {"score", 61}},
                                          QJsonObject{{"id", "zzz-best-match"},
                                                      {"name", "Nirvana"},
                                                      {"score", 100}}}}},
                  "artist");
    require(c.items().first().toMap()["id"] == "zzz-best-match",
            "provider relevance lost among homonymous bands");
    std::cout << "INDIE_RANKING_PASS: exact artist, live shows, arrival "
                 "independence, accents, detail order\n";
  }
};
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  app.setOrganizationName("TonalTests");
  app.setApplicationName("IndieCatalog");
  try {
    IndieCatalogTests::unit();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  if (!app.arguments().contains("--live"))
    return 0;
  CatalogService catalog(nullptr, false);
  QJsonArray phases;
  QStringList providers{"Audius", "MusicBrainz", "Live Music Archive"};
  int providerArg = app.arguments().indexOf("--provider");
  if (providerArg >= 0)
    providers = {app.arguments().value(providerArg + 1)};
  int phase = 0;
  int mbDepth = 0;
  const int queryArg = app.arguments().indexOf("--query");
  const QString queryOverride =
      queryArg >= 0 ? app.arguments().value(queryArg + 1) : QString();
  const int titleArg = app.arguments().indexOf("--mb-title");
  const QString mbTitle =
      titleArg >= 0 ? app.arguments().value(titleArg + 1) : QString();
  bool inspecting = false, playing = false, done = false;
  tonal::audio::AudioEngine audio(nullptr,
                                  QDir::tempPath() + "/TonalIndieLiveTest");
  QElapsedTimer clock;
  clock.start();
  QString current;
  auto save = [&](bool success) {
    if (done)
      return;
    done = true;
    QJsonObject report{
        {"passed", success},
        {"phases", phases},
        {"recordedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"spotifyCredentialsRead", false}};
    QString path =
        app.arguments().value(app.arguments().indexOf("--output") + 1);
    if (!path.isEmpty() && path != app.arguments().first()) {
      QFile f(path);
      if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(report).toJson());
    }
    std::cout << QJsonDocument(report).toJson().toStdString();
    app.exit(success ? 0 : 2);
  };
  QObject::connect(&catalog, &CatalogService::playable, &audio,
                   [&](auto url, auto title) {
                     playing = true;
                     clock.restart();
                     audio.openStream(url, title);
                   });
  auto search = [&] {
    current = providers[phase];
    inspecting = false;
    mbDepth = 0;
    catalog.search(!queryOverride.isEmpty()   ? queryOverride
                   : current == "Audius"      ? "Candyland Nirvana"
                   : current == "MusicBrainz" ? "Nirvana"
                                              : "Goose",
                   current);
  };
  QTimer poll;
  poll.setInterval(200);
  QObject::connect(&poll, &QTimer::timeout, &app, [&] {
    if (done)
      return;
    if (playing) {
      if (audio.positionMs() > 1800) {
        phases.append(QJsonObject{
            {"provider", current},
            {"pcmPlaybackPassed", true},
            {"positionMs", audio.positionMs()},
            {"metrics", QJsonObject::fromVariantMap(audio.metrics())}});
        audio.returnToSpotify();
        playing = false;
        ++phase;
        if (phase >= providers.size()) {
          save(true);
          return;
        }
        search();
      } else if (clock.elapsed() > 35000) {
        phases.append(QJsonObject{{"provider", current},
                                  {"pcmPlaybackPassed", false},
                                  {"diagnostics", audio.diagnostics()}});
        save(false);
      }
      return;
    }
    if (catalog.busy())
      return;
    if (catalog.items().isEmpty()) {
      phases.append(QJsonObject{{"provider", current},
                                {"error", catalog.messages().join("; ")}});
      save(false);
      return;
    }
    if (current == "MusicBrainz") {
      if (!inspecting) {
        auto artist = catalog.items().first().toMap();
        for (auto v : catalog.items())
          if (v.toMap().value("id") == "5b11f4ce-a62d-471e-81fc-a69a8278c7da")
            artist = v.toMap();
        catalog.open(artist);
        inspecting = true;
        mbDepth = 1;
      } else if (!mbTitle.isEmpty() && mbDepth < 3) {
        QVariantMap selected;
        if (mbDepth == 1) {
          for (const auto &value : catalog.items())
            if (value.toMap().value("title").toString().compare(
                    mbTitle, Qt::CaseInsensitive) == 0)
              selected = value.toMap();
          if (selected.isEmpty() && catalog.hasMore()) {
            catalog.more();
            return;
          }
        } else
          selected = catalog.items().first().toMap();
        if (selected.isEmpty()) {
          phases.append(
              QJsonObject{{"provider", current},
                          {"error", "Requested release group not found"}});
          save(false);
          return;
        }
        phases.append(
            QJsonObject{{"provider", current},
                        {"heading", catalog.heading()},
                        {"itemsLoaded", catalog.items().size()},
                        {"opening", QJsonObject::fromVariantMap(selected)}});
        catalog.open(selected);
        ++mbDepth;
      } else {
        phases.append(QJsonObject{{"provider", current},
                                  {"heading", catalog.heading()},
                                  {"detailDepth", mbDepth},
                                  {"releasesLoaded", catalog.items().size()}});
        ++phase;
        if (phase >= providers.size()) {
          save(true);
          return;
        }
        search();
      }
      return;
    }
    if ((current == "Live Music Archive" || current == "Internet Archive" || current == "Aadam Jacobs") && !inspecting) {
      catalog.open(catalog.items().first().toMap());
      inspecting = true;
      return;
    }
    for (auto v : catalog.items())
      if (v.toMap().value("playable").toBool()) {
        catalog.resolve(v.toMap());
        return;
      }
    phases.append(QJsonObject{{"provider", current},
                              {"error", "No directly playable candidate"}});
    save(false);
  });
  QTimer::singleShot(120000, &app, [&] { save(false); });
  search();
  poll.start();
  return app.exec();
}
