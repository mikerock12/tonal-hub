#include "companion/MusicCompanion.h"
#include "companion/PersonalDatabase.h"
#include "companion/StoryReport.h"
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
 QJsonArray batch{QJsonObject{{"title","<Synthetic track>"},{"artist","Synthetic artist"}}};
 require(personal::storyReport(QJsonObject{{"summary","O conjunto reúne letras que contrastam e contrastam."}},batch).isEmpty(),"reject old generic cached sentence");
 QJsonObject reading{{"track",0},{"code","A"},{"pending",false},{"reviewed",true},{"accepted",true},{"theme","Perda e saudade"},{"meaning","Lembranças, ausência ou ligação com o que ficou para trás."}};
 QJsonObject typed{{"schema",3},{"format","lyric-reading-v5"},{"complete",true},{"resumable",false},{"progressTrack",0},{"progressDone",0},{"progressTotal",0},{"progressStage",""},{"error",""},{"summary","Neste lote, aparecem Perda e saudade em 1 música."},{"items",QJsonArray{reading}},{"total",1},{"qualified",1}};
 auto checked=personal::storyReport(typed,batch);require(!checked.isEmpty()&&checked["items"].toArray()[0].toObject()["title"]=="<Synthetic track>","structured report attaches exact track");
 auto invalid=typed;auto bad=reading;bad["track"]=0.5;invalid["items"]=QJsonArray{bad};require(personal::storyReport(invalid,batch).isEmpty(),"reject fractional index");
 bad=reading;bad["track"]=1;invalid["items"]=QJsonArray{bad};require(personal::storyReport(invalid,batch).isEmpty(),"reject out-of-batch index");
 bad=reading;bad["accepted"]=false;invalid["items"]=QJsonArray{bad};require(personal::storyReport(invalid,batch).isEmpty(),"reject contradictory acceptance");
 invalid=typed;invalid["qualified"]=0;require(personal::storyReport(invalid,batch).isEmpty(),"reject false coverage");
 invalid=typed;invalid["schema"]=1;require(personal::storyReport(invalid,batch).isEmpty(),"invalidate legacy report schema");
 invalid=typed;invalid["error"]="timeout";require(personal::storyReport(invalid,batch).isEmpty(),"reject partial error result");
 require(personal::storyReport(typed,QJsonArray{}).isEmpty(),"reject metadata mismatch");
 auto pending=reading;pending["accepted"]=false;pending["reviewed"]=false;pending["pending"]=true;pending["code"]="L";pending["theme"]="Aguardando leitura";pending["meaning"]="Esta faixa ainda aguarda uma interpretação conferida.";
 auto resume=typed;resume["items"]=QJsonArray{pending};resume["complete"]=false;resume["resumable"]=true;resume["qualified"]=0;resume["summary"]="";resume["progressDone"]=32;resume["progressTotal"]=200;resume["progressStage"]="Preparando leitura";
 require(!personal::storyReport(resume,batch).isEmpty(),"zero accepted readings must still preserve valid resumable preparation");
 auto invalidProgress=resume;invalidProgress["progressDone"]=201;require(personal::storyReport(invalidProgress,batch).isEmpty(),"reject progress beyond prepared prompt");
 invalidProgress=resume;invalidProgress["resumable"]=false;require(personal::storyReport(invalidProgress,batch).isEmpty(),"pending work must be explicitly resumable");
 invalidProgress=resume;invalidProgress["summary"]="Invented summary without any accepted readings.";require(personal::storyReport(invalidProgress,batch).isEmpty(),"never fill zero-coverage progress with a narrative");
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
 LocalStories stories(dir.path()+"/test-stories");require(!stories.state()["ready"].toBool(),"missing model ready");stories.analyze({},0);require(!stories.state()["busy"].toBool()&&stories.state()["summary"].toString().isEmpty(),"missing model must not invent summary");
 for(int i=0;i<140;++i){auto t=song;t["id"]=QString::number(i);stories.capture(t,"Original synthetic lyric evidence for a bounded cache.");}QDir cache(dir.path()+"/test-stories/lyrics");wait.restart();while(cache.entryList({"*.json"},QDir::Files).size()!=128&&wait.elapsed()<10000){QCoreApplication::processEvents();QThread::msleep(5);}require(cache.entryList({"*.json"},QDir::Files).size()==128,"lyric evidence bound");stories.setPlaying(true);stories.download();wait.restart();while(!stories.state()["downloading"].toBool()&&wait.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(5);}require(stories.state()["status"].toString().contains("pausa"),"download must yield to audio");stories.cancel();wait.restart();while(stories.state()["downloading"].toBool()&&wait.elapsed()<3000){QCoreApplication::processEvents();QThread::msleep(5);}require(!stories.state()["busy"].toBool()&&!stories.state()["downloading"].toBool(),"cancel pending inference and deferred download");
 std::cout<<"HISTORY_STORIES_PASS: archive 15001, periods 6, full backup roundtrip, bytes, explicit clear, bounded evidence\n";
}};
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{CompanionTests::run();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
