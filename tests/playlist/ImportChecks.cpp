#include "CoreCases.hpp"
#include "playlist/TextPlaylistImport.hpp"
#include "companion/MusicCompanion.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QImage>
#include <QFile>
#include <iostream>

void waitFor(const std::function<bool()> &predicate){QElapsedTimer t;t.start();while(!predicate()&&t.elapsed()<5000){QCoreApplication::processEvents();QThread::msleep(2);}expectCore(predicate(),"asynchronous import timed out");}
QJsonObject spotifyTrack(){return {{"uri","spotify:track:1234567890123456789012"},{"name","Seether"},{"artists",QJsonArray{QJsonObject{{"name","Veruca Salt"}}}},{"album",QJsonObject{{"name","American Thighs"}}}};}
int main(int argc,char **argv){QCoreApplication app(argc,argv);app.setOrganizationName("TonalTests");app.setApplicationName("TextPlaylist");
    try{
        playlistCoreCases();QTemporaryDir storage;QString saved;
        {
        MusicCompanion music(nullptr,storage.path());int writes=0,creates=0,puts=0,covers=0,remoteCount=0;bool dropCreate=false,dropBatch=false,dropCover=true;
        TextPlaylistImport importer(&music,[&](const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &body,const QByteArray &mime,TextPlaylistImport::Reply cb,std::function<bool()> alive){
            expectCore(alive(),"request generation guard");int status=200;QJsonDocument result;
            if(method=="GET"&&path=="/search"){
                expectCore(query.value("limit").toInt()==10,"development-mode search limit");result=QJsonDocument(QJsonObject{{"tracks",QJsonObject{{"items",QJsonArray{spotifyTrack()}}}}});
            }else if(method=="POST"&&path=="/me/playlists"){
                ++creates;++writes;expectCore(QJsonDocument::fromJson(body).object()["public"]==false,"private publication explicit");status=dropCreate?503:201;result=QJsonDocument(QJsonObject{{"id","abcdefghijklmnopqrstuv"}});
            }else if(method=="GET"&&path.contains("/items")){
                QJsonArray items;for(int i=0;i<remoteCount;++i)items.append(QJsonObject{{"item",spotifyTrack()}});result=QJsonDocument(QJsonObject{{"items",items}});
            }else if(path.endsWith("/items")){
                ++writes;auto items=QJsonDocument::fromJson(body).object()["uris"].toArray();expectCore(items.size()<=100,"Spotify batch bound");remoteCount=method=="PUT"?int(items.size()):remoteCount+int(items.size());if(method=="PUT")++puts;status=dropBatch?503:201;
            }else if(path.endsWith("/images")){
                ++writes;++covers;expectCore(mime=="image/jpeg"&&body.startsWith("/9j/")&&body.size()<=256*1024,"base64 JPEG cover contract");status=dropCover?403:202;
            }
            QTimer::singleShot(0,&app,[=]{cb(status,result);});
        });
        importer.parse("Veruca Salt - Seether\nVeruca Salt - Seether");expectCore(writes==0,"parse writes to account");importer.search();waitFor([&]{return !importer.busy();});expectCore(importer.selectedCount()==2&&writes==0,"review has no remote side effects");
        QImage image(64,64,QImage::Format_RGB32);image.fill(Qt::blue);auto coverPath=storage.filePath("cover.png");expectCore(image.save(coverPath),"test cover fixture");expectCore(importer.setCover(QUrl::fromLocalFile(coverPath)),"bounded native cover conversion");
        expectCore(importer.saveLocal("Teste local","Descrição",false),"local atomic save");waitFor([&]{return !importer.busy();});saved=importer.savedId();expectCore(music.playlistTracks(saved).size()==2&&writes==0,"local preserves reprise without Spotify write");
        importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(creates==1&&puts==1&&remoteCount==2,"cloud create/items");
        expectCore(covers==1&&!music.playlistDetails(saved)["coverSynced"].toBool(),"cover failure should preserve successful tracks");dropCover=false;
        importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(creates==1&&puts==1&&covers==2&&music.playlistDetails(saved)["coverSynced"].toBool(),"cover retry duplicated tracks/playlist");
        QVariantList many;for(int i=0;i<201;++i)many<<music.playlistTracks(saved).first();auto bulk=music.importPlaylist("201 faixas","",QString(),many);expectCore(importer.loadSaved(bulk),"resume local bulk playlist");importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(creates==2&&remoteCount==201,"multi-batch sync preserves all 201 tracks");
        importer.parse("Veruca Salt - Seether");importer.search();waitFor([&]{return !importer.busy();});expectCore(importer.saveLocal("Resposta perdida","",false),"second local save");waitFor([&]{return !importer.busy();});dropCreate=true;importer.sync(false);waitFor([&]{return !importer.busy();});int previous=creates;importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(creates==previous,"uncertain POST repeated creation");
        importer.acknowledgeUncertainCreate();dropCreate=false;dropBatch=true;importer.sync(false);waitFor([&]{return !importer.busy();});previous=puts;dropBatch=false;importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(puts==previous&&remoteCount==1,"uncertain items must be verified, not blindly repeated");
        importer.parse("Veruca Salt - Seether");importer.search();importer.cancel();importer.parse("L7 - Shove");QCoreApplication::processEvents();expectCore(importer.rows().first().toMap()["title"]=="Shove"&&importer.selectedCount()==0,"stale search corrupted a new draft");
        }
        {
            MusicCompanion music(nullptr,storage.path());QVariantList remote;remote<<QVariantMap{{"uri","spotify:track:AAAAAAAAAAAAAAAAAAAAAA"}}<<QVariantMap{{"uri","spotify:track:BBBBBBBBBBBBBBBBBBBBBB"}};int posts=0,puts=0,creates=0;bool drop=true;
            TextPlaylistImport importer(&music,[&](const QByteArray &method,const QString &path,const QVariantMap &,const QByteArray &body,const QByteArray &,TextPlaylistImport::Reply cb,std::function<bool()> alive){
                expectCore(alive(),"append request generation");int status=200;QJsonDocument result;
                if(method=="GET"&&path=="/search")result=QJsonDocument(QJsonObject{{"tracks",QJsonObject{{"items",QJsonArray{spotifyTrack()}}}}});
                else if(method=="GET"&&path.contains("/items")){QJsonArray items;for(const auto &track:remote)items.append(QJsonObject{{"item",QJsonObject::fromVariantMap(track.toMap())}});result=QJsonDocument(QJsonObject{{"items",items}});}
                else if(method=="POST"&&path.endsWith("/items")){++posts;for(auto uri:QJsonDocument::fromJson(body).object()["uris"].toArray())remote<<QVariantMap{{"uri",uri.toString()}};status=drop?503:201;}
                else if(method=="PUT"&&path.endsWith("/items")){++puts;status=500;}
                else if(method=="POST"&&path=="/me/playlists"){++creates;status=500;}
                QTimer::singleShot(0,&app,[=]{cb(status,result);});
            });
            importer.parse("Veruca Salt - Seether");importer.search();waitFor([&]{return !importer.busy();});expectCore(importer.setTargetPlaylist("abcdefghijklmnopqrstuv"),"existing playlist target");expectCore(importer.saveLocal("Adições locais","",false),"append local checkpoint");waitFor([&]{return !importer.busy();});
            importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(posts==1,"one append POST before lost receipt");expectCore(puts==0,"append never replaces remote items");expectCore(creates==0,"append never creates another playlist");expectCore(remote.size()==3,"lost append response was applied by fixture");expectCore(importer.details()["cloudUncertain"].toBool(),"lost append response marked for verification");
            drop=false;importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(posts==1&&puts==0&&creates==0&&remote.size()==3&&importer.details()["cloudComplete"].toBool(),"append resume verifies base plus suffix without duplicate or overwrite");
            expectCore(importer.details()["appendBase"].toList().size()==2&&remote[2].toMap()["uri"].toString()=="spotify:track:1234567890123456789012","existing prefix preserved and reviewed track appended");
            remote.clear();posts=0;drop=true;importer.parse("Veruca Salt - Seether");importer.search();waitFor([&]{return !importer.busy();});expectCore(importer.setTargetPlaylist("abcdefghijklmnopqrstuv"),"empty existing playlist target");expectCore(importer.saveLocal("Adição em playlist vazia","",false),"empty base checkpoint");waitFor([&]{return !importer.busy();});importer.sync(false);waitFor([&]{return !importer.busy();});drop=false;importer.sync(false);waitFor([&]{return !importer.busy();});
            expectCore(posts==1&&remote.size()==1&&importer.details()["appendBase"].toList().isEmpty()&&importer.details()["appendBaseReady"].toBool()&&importer.details()["cloudComplete"].toBool(),"empty remote base is distinguishable from an unconfirmed base and resumes without duplicates");
        }
        {MusicCompanion reopened(nullptr,storage.path());expectCore(reopened.playlistTracks(saved).size()==2&&reopened.playlistDetails(saved)["description"]=="Descrição","playlist/description persistence");
         const auto expected=reopened.playlistDetails(saved);auto backup=QUrl::fromLocalFile(storage.filePath("roundtrip.tonal-backup.json"));
         expectCore(reopened.exportBackup(backup),"imported playlist backup export");reopened.deletePlaylist(saved);
         expectCore(reopened.restoreBackup(backup)&&reopened.playlistTracks(saved).size()==2,"backup restore lost intentional reprise");
         const auto actual=reopened.playlistDetails(saved);for(auto field:{"description","cover","spotifyId","syncedCount","coverSynced","cloudComplete"})expectCore(actual[field]==expected[field],"backup lost imported playlist metadata");}
        QTemporaryDir blocked;QFile blocker(blocked.filePath("file-not-folder"));expectCore(blocker.open(QIODevice::WriteOnly),"disk failure fixture");blocker.close();
        {MusicCompanion failed(nullptr,blocker.fileName());int writes=0;TextPlaylistImport importer(&failed,[&](const auto &,const auto &,const auto &,const auto &,const auto &,auto,auto){++writes;});
         QVariantMap t{{"provider","Spotify"},{"id","spotify:track:1234567890123456789012"},{"uri","spotify:track:1234567890123456789012"},{"title","Seether"}};
         auto id=failed.importPlaylist("Falha de disco","",QString(),{t});expectCore(importer.loadSaved(id),"failed-disk draft still accessible");importer.sync(false);waitFor([&]{return !importer.busy();});expectCore(writes==0,"external write occurred before durable local checkpoint");}
        std::cout<<"TEXT_PLAYLIST_IMPORT_PASS native parsing, local-first, cloud transactions and interruption recovery\n";return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
