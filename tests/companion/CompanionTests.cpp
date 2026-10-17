#include "companion/MusicCompanion.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QThread>
#include <QElapsedTimer>
#include <QNetworkReply>
#include <QSslSocket>
#include <iostream>
#include <stdexcept>

#include "MuseTests.h"

class CompanionTests {
  static void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
public:
  static void run(){
    testMuse();
    QTemporaryDir dir;require(dir.isValid(),"temporary store unavailable");
    const QVariantMap song{{"provider","Internet Archive"},{"id","concert/01.mp3"},{"title","Synthetic Fire"},{"artist","Example"},{"genre","punk"},{"durationMs",2000},{"playable",true},{"stream","https://archive.org/download/concert/01.mp3"},{"password","never-store-this"}};
    {
      MusicCompanion m(nullptr,dir.path());m.selectTrack(song);
      m.favorite(song);require(m.favorites().size()==1,"favorite missing");
      auto playlist=m.createPlaylist("Tests");m.addToPlaylist(playlist,song);m.addToPlaylist(playlist,song);
      require(m.playlistTracks(playlist).size()==1,"playlist duplicates accepted");
      m.renamePlaylist(playlist,"Renamed");require(m.playlists()[0].toMap()["name"]=="Renamed","rename lost");
      QVariantMap signedTrack=song;signedTrack["stream"]="https://example.org/audio?token=private";
      require(!MusicCompanion::safeTrack(signedTrack).contains("stream"),"signed stream persisted");
      require(!MusicCompanion::safeTrack(song).contains("password"),"unknown sensitive field persisted");
      auto lrc=MusicCompanion::parseLyrics("[ar:Fixture]\n[offset:-100]\n[00:01.20][00:02.345]Synthetic line\n[00:00.50]Earlier line");
      require(lrc.size()==3&&lrc[0].toMap()["time"].toInt()==400&&lrc[2].toMap()["time"].toInt()==2245,"LRC fractions, multiple stamps or offset broken");
      m.importLyrics("[00:00.00]fire rage fight\n[00:01.00]dance");
      require(m.retryLyrics(503,m.m_generation)&&m.retryLyrics(429,m.m_generation)&&!m.retryLyrics(503,m.m_generation)&&!m.retryLyrics(401,m.m_generation),"lyric retries not bounded to temporary failures");
      ++m.m_generation; // invalidate the queued retries before continuing with another version
      auto displayed=m.lines();displayed.insert(1,QVariantMap{{"time",500},{"text",""}});
      const int generation=m.m_generation;
      require(!m.useDisplayedLyrics("spotify:track:stale",displayed)&&m.m_generation==generation,"stale screen replaced current lyrics");
      auto screenSong=song;screenSong["uri"]="spotify:track:fixture";m.selectTrack(screenSong);
      require(m.useDisplayedLyrics("spotify:track:fixture",displayed)&&m.lines()==displayed,"displayed line times or blank verses changed");
      m.importLyrics("fogo\n\ndança",true);auto translatedLines=m.lines();
      require(m.useDisplayedLyrics("spotify:track:fixture",displayed)&&m.lines()==translatedLines,"reopening screen erased translation");
      auto other=displayed;auto changed=other[0].toMap();changed["text"]="Another version";other[0]=changed;
      const int oldGeneration=m.m_generation;m.m_busy=true;
      require(m.useDisplayedLyrics("spotify:track:fixture",other)&&!m.busy()&&m.m_generation>oldGeneration&&!m.lines()[0].toMap().contains("translation"),"changed lyric version kept stale translation or request");
      m.selectTrack(song);m.importLyrics("[00:00.00]fire rage fight\n[00:01.00]dance");
      require(m.current()["lyricsAnalyzed"].toBool(),"lyric evidence not analyzed");
      m.setFavorite(song,true);require(m.m_data["tracks"].toObject()[MusicCompanion::key(song)].toObject()["lyricsAnalyzed"].toBool(),"favorite erased lyric evidence");
      m.pulse(true,0);QThread::msleep(20);m.pulse(true,20);require(m.m_pendingMs>0&&m.m_pendingMs<=20,"actual progress not bounded by elapsed time");
      m.pulse(false,20);auto before=m.m_sessionMs;QThread::msleep(10);m.pulse(false,5000);require(m.m_sessionMs==before,"pause counted as listening");
      m.pulse(true,5000);QThread::msleep(10);m.pulse(true,30000);require(m.m_sessionMs==before,"seek jump counted as listening");
      auto events=QJsonArray();auto now=QDateTime::currentMSecsSinceEpoch();
      for(int i=0;i<3;i++)events.append(QJsonObject{{"session",QString::number(i)},{"key",MusicCompanion::key(song)},{"at",now-1000-i*60000},{"ms",60000},{"play",true}});
      events.append(QJsonObject{{"key",MusicCompanion::key(song)},{"at",now-20LL*86400000},{"ms",120000},{"play",true}});
      m.m_data["events"]=events;auto weekly=m.statistics(7,"artists"),monthly=m.statistics(30,"genres");
      require(weekly["plays"].toInt()==3&&weekly["ms"].toDouble()==180000&&monthly["ms"].toDouble()==300000,"period or duration totals incorrect");
      require(weekly["rows"].toList()[0].toMap()["tracks"].toList().size()==1,"chart drilldown duplicated tracks");
      require(weekly["vibe"].toString()=="Clima intenso"&&!weekly["recommendations"].toList().isEmpty(),"lexical evidence or genre recommendations missing");
      auto detail=weekly["rows"].toList()[0].toMap();require(detail["trackCount"].toInt()==1&&detail["plays"].toInt()==3&&detail["tracks"].toList()[0].toMap()["listenedMs"].toDouble()==180000,"detailed listening time or count missing");
      m.rememberSearch("  Nirvana  ");m.rememberSearch("L7");m.rememberSearch("nirvana");require(m.searches().size()==2&&m.searches()[0]=="nirvana","search history deduplication failed");m.forgetSearch("L7");require(m.searches().size()==1,"search history removal failed");
      m.setLyricStyle("lyricFont","Mono");m.setLyricStyle("translationColor","#FFBBD9");m.setLyricStyle("lyricSize",999);m.setLyricStyle("translationFont","invalid");require(m.lyricStyle()["lyricFont"]=="Mono"&&m.lyricStyle()["lyricSize"].toInt()==44&&m.lyricStyle()["translationFont"]=="Sans","lyric preferences validation failed");
      require(m.insights(7)["modelCoverage"].toInt()==1,"trained topic coverage missing");
      m.addToPlaylist(playlist,song);require(m.m_data["tracks"].toObject()[MusicCompanion::key(song)].toObject()["topics"].toObject().size()==8,"playlist erased trained analysis");
      m.m_pendingMs=0;m.flush();
    }
    QFile file(dir.path()+"/music-library.sqlite");require(file.open(QIODevice::ReadOnly),"SQLite database missing");auto bytes=file.readAll();require(!bytes.contains("never-store-this")&&!bytes.contains("token=private")&&!bytes.contains("fire rage fight"),"private payload or lyric text leaked to library");
    MusicCompanion restored(nullptr,dir.path());require(restored.favorites().size()==1&&restored.playlists().size()==1,"library did not survive restart");
    require(restored.searches().size()==1&&restored.lyricStyle()["translationColor"]=="#FFBBD9","search or style did not persist");
    auto backup=QUrl::fromLocalFile(dir.path()+"/portable.tonal-backup.json");require(restored.exportBackup(backup),"backup export failed");
    restored.deletePlaylist(restored.playlists()[0].toMap()["id"].toString());require(restored.restoreBackup(backup)&&restored.playlists().size()==1,"backup restore failed");
    require(restored.lyricStyle()["lyricFont"]=="Mono"&&restored.searches().size()==1,"preferences absent from restored backup");
    auto args=QCoreApplication::arguments();int exportAt=args.indexOf("--export");if(exportAt>=0)require(restored.exportBackup(QUrl::fromLocalFile(args.value(exportAt+1))),"interop export failed");
    int importAt=args.indexOf("--import");if(importAt>=0)require(restored.restoreBackup(QUrl::fromLocalFile(args.value(importAt+1)))&&!restored.favorites().isEmpty(),"Android backup import failed");
    restored.clearHistory();require(restored.statistics(7,"artists")["plays"].toInt()==0&&restored.favorites().size()==1,"clear history removed library or kept counts");
    if(args.contains("--live-translation")){
      QObject::connect(&restored.m_net,&QNetworkAccessManager::finished,&restored,[](QNetworkReply *r){std::cerr<<"Synthetic translation HTTP="<<r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()<<" error="<<int(r->error())<<" bytes="<<r->bytesAvailable()<<" ssl="<<QSslSocket::supportsSsl()<<"\n";});
      restored.selectTrack(song);restored.importLyrics("[00:00.00]Hello world");restored.translate("auto");QElapsedTimer deadline;deadline.start();
      while(restored.busy()&&deadline.elapsed()<30000){QCoreApplication::processEvents();QThread::msleep(20);}
      auto translated=restored.lines().value(0).toMap()["translation"].toString();if(!translated.contains("Olá",Qt::CaseInsensitive))std::cerr<<"Translation status: "<<restored.lyricStatus().toStdString()<<"\n";require(translated.contains("Olá",Qt::CaseInsensitive),"live pt-BR translation failed");std::cout<<"LIVE_TRANSLATION_PASS: synthetic phrase translated to Portuguese\n";
    }
    std::cout<<"COMPANION_PASS: persistence, deduplication, privacy, actual progress, pause, seek, periods, evidence, chart drilldown, LRC\n";
  }
};
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{CompanionTests::run();return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
