#include "companion/MusicCompanion.h"
#include "companion/PersonalDatabase.h"
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
