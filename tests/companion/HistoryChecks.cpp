#include "companion/MusicCompanion.h"
#include "companion/PersonalDatabase.h"
#include <QFile>
#include "companion/HomeAlbums.h"
#include "jam/JamState.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QDateTime>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QThread>
#include <QFileInfo>
#include <QDir>
#include <iostream>
#include <stdexcept>

static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
class CompanionTests {
public:static void run(){
 QJsonObject jam{{"session_id","synthetic-session"},{"join_session_token","synthetic-token"},{"is_session_owner",true},{"session_members",QJsonArray{QJsonObject{{"id","member-one"},{"display_name","Pessoa sintética"}}}}};
 auto firstJam=tonal::jam::parse(jam);require(firstJam.has_value()&&firstJam->value("link").toString()=="https://open.spotify.com/socialsession/synthetic-token","Jam accepts only canonical invite links");
 require(tonal::jam::joined({},*firstJam).isEmpty(),"initial Jam snapshot must not invent new arrivals");
 auto added=jam.value("session_members").toArray();added.append(QJsonObject{{"id","member-two"},{"display_name","Outro participante"}});jam["session_members"]=added;auto nextJam=tonal::jam::parse(jam);
 require(nextJam&&tonal::jam::joined(*firstJam,*nextJam)==QStringList{"Outro participante"},"Jam reports the actual new member");
 require(tonal::jam::left(*nextJam,*firstJam)==QStringList{"Outro participante"}&&tonal::jam::left(*firstJam,*nextJam).isEmpty(),"Jam reports who left without inventing departures");
 added.append(added[0]);jam["session_members"]=added;require(!tonal::jam::parse(jam),"duplicate member IDs are not valid arrivals");
 jam["session_members"]=QJsonArray{};jam["join_session_token"]="https://untrusted.invalid/path";auto noLink=tonal::jam::parse(jam);require(noLink&&noLink->value("link").toString().isEmpty(),"server content cannot inject a share URL");
 QJsonObject favoriteArtist{{"provider","Audius"},{"id","artist-public"},{"kind","artist"},{"title","Synthetic artist"}};
 QJsonObject favoriteBackup{{"format","tonal-personal-backup"},{"version",1},{"module","indie"},{"data",QJsonObject{{"tracks",QJsonObject{}},{"collectionFavorites",QJsonArray{favoriteArtist}}}}};QString favoriteError;
 auto favoriteClean=MusicCompanion::validateBackup(favoriteBackup,"indie",favoriteError);require(favoriteClean["collectionFavorites"].toArray().size()==1,"collection favorites survive backup validation");
 const qint64 albumNow=QDateTime::currentMSecsSinceEpoch();
 QJsonObject albumTrack{{"provider","Spotify"},{"albumId","AAAAAAAAAAAAAAAAAAAAAA"},{"album","Synthetic album"},{"artist","Synthetic artist"}};
 QJsonObject albumData{{"tracks",QJsonObject{{"one",albumTrack},{"opened",QJsonObject{{"provider","Spotify"},{"albumId","BBBBBBBBBBBBBBBBBBBBBB"},{"album","Only opened"}}}}},
 {"events",QJsonArray{QJsonObject{{"key","one"},{"at",double(albumNow-1000)},{"ms",5000}},QJsonObject{{"key","one"},{"at",double(albumNow-500)},{"ms",7000}},QJsonObject{{"key","opened"},{"at",double(albumNow)},{"ms",0}}}}};
 auto deck=personal::homeAlbums(albumData);require(deck.size()==1&&deck[0].toMap()["listenedMs"].toInt()==12000&&deck[0].toMap()["lastAt"].toLongLong()==albumNow-500,"home uses actual listens, sums albums and sorts by recency");
 auto invalidAlbum=albumTrack;invalidAlbum["albumId"]="bad/id";albumData["tracks"]=QJsonObject{{"one",invalidAlbum}};require(personal::homeAlbums(albumData).isEmpty(),"home rejects invalid album ids");
 QTemporaryDir dir;auto path=dir.path()+"/music-library.sqlite";const qint64 now=QDateTime::currentMSecsSinceEpoch();
 QVariantMap song{{"provider","Spotify"},{"id","spotify:track:1234567890123456789012"},{"uri","spotify:track:1234567890123456789012"},{"title","Synthetic history"},{"artist","History fixture"},{"album","Test album"},{"durationMs",180000}};
 auto key=MusicCompanion::key(song);QJsonObject data{{"tracks",QJsonObject{{key,QJsonObject::fromVariantMap(song)}}}},previous;
 QJsonArray events;for(int i=0;i<15001;++i)events.append(QJsonObject{{"session",QString::number(i)},{"key",key},{"at",double(now-(15000LL-i)*3600000)},{"ms",30000},{"play",true}});data["events"]=events;
 std::cerr<<"stage: initial save\n";require(personal::save(path,data,previous),"initial history persistence");auto recent=personal::load(path);require(recent["events"].toArray().size()==10000,"bounded recent cache");
 require(personal::save(path,recent,previous),"save cache eviction");require(personal::fullSnapshot(path)["events"].toArray().size()==15001,"cache eviction must preserve old history");
 std::cerr<<"stage: totals\n";auto all=personal::historySnapshot(path,recent,0);auto report=MusicCompanion::statisticsFor(all,0,"tracks");require(report["plays"].toInt()==15001&&report["ms"].toDouble()==15001.*30000,"all-time archive totals and active overlay");
 auto year=MusicCompanion::statisticsFor(all,365,"tracks");require(year["plays"].toInt()>8000&&year["plays"].toInt()<9000,"year filter beyond 180 days");
 for(auto days:{7,30,90,180,365})require(MusicCompanion::statisticsFor(all,days,"artists")["plays"].toInt()>0,"period empty");
 std::cerr<<"stage: async stats\n";MusicCompanion companion(nullptr,dir.path());companion.requestStatistics(0,"artists");QElapsedTimer wait;wait.start();while(companion.m_statisticsRunning&&wait.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(5);}require(companion.statisticsReport()["plays"].toInt()==15001,"async history report");
 std::cerr<<"stage: full export\n";auto url=QUrl::fromLocalFile(dir.path()+"/full.json");require(companion.exportFullBackup(url),"full export accepted");wait.restart();while(!companion.storageInfo()["lastBackupBytes"].toDouble()&&wait.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(5);}require(companion.storageInfo()["lastBackupBytes"].toLongLong()==QFileInfo(url.toLocalFile()).size()&&QFileInfo(url.toLocalFile()).size()>0,"actual exported bytes");
 std::cerr<<"stage: full restore\n";QTemporaryDir restoredDir;MusicCompanion restored(nullptr,restoredDir.path());require(restored.restoreFullBackup(url),"full import accepted");wait.restart();while(restored.m_restoring&&wait.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(5);}require(personal::fullSnapshot(restoredDir.path()+"/music-library.sqlite")["events"].toArray().size()==15001,"full archive restore beyond legacy cap");
 restored.clearHistory();restored.persist([](bool){});QMetaObject::invokeMethod(restored.m_disk,[]{},Qt::BlockingQueuedConnection);require(personal::fullSnapshot(restoredDir.path()+"/music-library.sqlite")["events"].toArray().isEmpty(),"explicit clear deletes archive");
 QDir().mkpath(dir.path()+"/test-stories");QFile legacy(dir.path()+"/test-stories/Qwen3.5-2B-Q4_K_M.gguf");require(legacy.open(QIODevice::WriteOnly),"create legacy weight fixture");legacy.write("old");legacy.close();
 LocalStories stories(dir.path()+"/test-stories");require(!QFileInfo::exists(legacy.fileName()),"legacy Qwen weights removed");require(stories.state()["ready"].toBool()&&!stories.state()["busy"].toBool(),"rule summary is immediately ready");
 auto summarized=song;summarized["genre"]="Grunge";summarized["tone"]=QVariantMap{{"intenso",4}};stories.analyze({summarized},0);const auto summary=stories.state()["summary"].toString();require(summary.contains("Grunge")&&summary.contains("intenso")&&summary.contains("sem modelo"),"deterministic genre and mood summary");stories.analyze({},0);require(stories.state()["summary"].toString().isEmpty(),"empty period stays empty");
 std::cout<<"HISTORY_STORIES_PASS: archive 15001, periods 6, full backup and collection favorites roundtrip, explicit clear, deterministic summary without model\n";
}};
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{CompanionTests::run();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
