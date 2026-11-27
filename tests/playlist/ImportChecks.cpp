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
                expectCore(query.value("limit").toInt()==10,"development-mode search limit");
                if(query.value("type")=="artist")result=QJsonDocument(QJsonObject{{"artists",QJsonObject{{"items",QJsonArray{QJsonObject{{"id","VVVVVVVVVVVVVVVVVVVVV1"},{"name","Veruca Salt"}}}}}}});
                else {expectCore(query.value("market")=="from_token","track search uses the account market");result=QJsonDocument(QJsonObject{{"tracks",QJsonObject{{"items",QJsonArray{spotifyTrack()}}}}});}
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
        {const auto audit=music.playlistDetails(saved)["importAudit"].toList();expectCore(audit.size()==2&&audit[0].toMap()["match"]=="EXACT_MATCH"&&audit[0].toMap()["requested"]=="Veruca Salt - Seether","import audit keeps requested reference and decision");}
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
        {   // Spec 0.18.2: artist-centred recovery, homonyms, technical errors, UserOverride and large lists.
            MusicCompanion music(nullptr,storage.path());int searches=0,artistSearches=0,albumCalls=0,albumTrackCalls=0;bool fail503=false,fail429=false;
            auto track=[](const QString &name,const QString &artist,const QString &artistId,const QString &uri){return QJsonObject{{"uri",uri},{"id",uri.mid(14)},{"name",name},
                {"artists",QJsonArray{QJsonObject{{"name",artist},{"id",artistId}}}},{"album",QJsonObject{{"name","Álbum"},{"album_type","album"}}},{"is_playable",true}};};
            TextPlaylistImport importer(&music,[&](const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &,const QByteArray &,TextPlaylistImport::Reply cb,std::function<bool()>){
                expectCore(method=="GET","lookup never writes");int status=200;QJsonDocument result;const auto q=query.value("q").toString();
                if(path=="/search"&&query.value("type")=="artist"){++artistSearches;
                    result=QJsonDocument(QJsonObject{{"artists",QJsonObject{{"items",QJsonArray{QJsonObject{{"id","FFFFFFFFFFFFFFFFFFFFF1"},{"name","Free Energy"}},QJsonObject{{"id","XXXXXXXXXXXXXXXXXXXXX9"},{"name","Free Energy Orchestra"}}}}}}});}
                else if(path=="/search"){++searches;QJsonArray items;
                    if(fail503&&q.contains("Broken")){status=503;}
                    else if(fail429&&q.contains("Limited")){status=429;result=QJsonDocument(QJsonObject{{"error",QJsonObject{{"message","O Spotify limitou as consultas. Aguarde 40 s para tentar novamente."}}}});}
                    else if(q.contains("Something In Common")){items.append(track("Something In Common","Bobby Brown","BBBBBBBBBBBBBBBBBBBBB1","spotify:track:SSSSSSSSSSSSSSSSSSSSS1"));items.append(track("Something in Common","Kendra Morris","KKKKKKKKKKKKKKKKKKKKK1","spotify:track:SSSSSSSSSSSSSSSSSSSSS2"));}
                    else if(q.contains("Dream City")){items.append(track("Dream City","Balcony Sunrise","CCCCCCCCCCCCCCCCCCCCC1","spotify:track:DDDDDDDDDDDDDDDDDDDDD0"));}
                    else if(q.contains("Song ")){const auto n=q.section("Song ",1).section('"',0,0).trimmed();items.append(track("Song "+n,"Free Energy","FFFFFFFFFFFFFFFFFFFFF1",QString("spotify:track:%1").arg(n.toInt(),22,10,QChar('0'))));}
                    else items.append(track("Girls Want Rock","Free Energy","FFFFFFFFFFFFFFFFFFFFF1","spotify:track:GGGGGGGGGGGGGGGGGGGGG1"));
                    if(status==200)result=QJsonDocument(QJsonObject{{"tracks",QJsonObject{{"items",items}}}});}
                else if(path.startsWith("/artists/")){++albumCalls;expectCore(path=="/artists/FFFFFFFFFFFFFFFFFFFFF1/albums","discography of the confirmed artist only");
                    result=QJsonDocument(QJsonObject{{"items",query.value("include_groups")=="single"?QJsonArray{QJsonObject{{"id","AAAAAAAAAAAAAAAAAAAAA1"},{"name","Dream City"},{"album_type","single"},{"images",QJsonArray{QJsonObject{{"url","https://i.scdn.co/image/dream"}}}}}}:QJsonArray{}}});}
                else if(path.startsWith("/albums/")){++albumTrackCalls;
                    result=QJsonDocument(QJsonObject{{"items",QJsonArray{QJsonObject{{"uri","spotify:track:DDDDDDDDDDDDDDDDDDDDD1"},{"name","Dream City"},{"artists",QJsonArray{QJsonObject{{"name","Free Energy"},{"id","FFFFFFFFFFFFFFFFFFFFF1"}}}},{"is_playable",true}}}}});}
                else status=404;
                QTimer::singleShot(0,&app,[=]{cb(status,result);});
            });
            importer.setRequestSpacing(0);
            importer.parse("Free Energy - Something In Common\nFree Energy - Dream City\nFree Energy - Girls Want Rock");importer.search();waitFor([&]{return !importer.busy();});
            auto rows=importer.rows();auto common=rows[0].toMap(),dream=rows[1].toMap(),girls=rows[2].toMap();
            expectCore(artistSearches==1,"artist resolved once per import (cache)");
            expectCore(!common["selected"].toBool()&&common["choice"].toInt()==-1&&common["match"]=="NOT_FOUND"&&common["section"]=="wrongArtist","homonyms never marked as found");
            {int wrong=0;for(auto c:common["candidates"].toList())wrong+=c.toMap()["match"]=="WRONG_ARTIST";expectCore(wrong==2&&common["candidates"].toList().last().toMap()["match"]=="WRONG_ARTIST","other performers listed apart, last");}
            expectCore(dream["selected"].toBool()&&dream["match"]=="EXACT_MATCH"&&dream["candidates"].toList()[0].toMap()["album"]=="Dream City"&&
                       dream["candidates"].toList()[0].toMap()["image"]=="https://i.scdn.co/image/dream","discography recovers the requested artist's recording");
            expectCore(albumCalls>=2&&albumTrackCalls>=1&&girls["selected"].toBool(),"progressive lookup reached the discography");
            auto sum=importer.summary();expectCore(sum["selectedTotal"].toInt()+sum["pending"].toInt()+sum["ignoredTotal"].toInt()==3&&sum["selectedTotal"].toInt()==importer.selectedCount(),"counters derive from row state");
            // "Buscar pendentes" re-queries only unresolved references.
            const int before=searches;importer.search();waitFor([&]{return !importer.busy();});
            expectCore(searches>before&&importer.rows()[1].toMap()["selected"].toBool()&&importer.rows()[2].toMap()["selected"].toBool(),"pending search kept resolved rows");
            // Manual choice of a homonym is a UserOverride kept across searches.
            {int kendra=-1;const auto list=importer.rows()[0].toMap()["candidates"].toList();for(int i=0;i<list.size();++i)if(list[i].toMap()["uri"]=="spotify:track:SSSSSSSSSSSSSSSSSSSSS2")kendra=i;expectCore(kendra>=0,"homonym listed");importer.choose(0,kendra,true);}importer.search();waitFor([&]{return !importer.busy();});common=importer.rows()[0].toMap();
            expectCore(common["manual"].toBool()&&common["selected"].toBool()&&common["candidates"].toList()[common["choice"].toInt()].toMap()["uri"]=="spotify:track:SSSSSSSSSSSSSSSSSSSSS2","manual choice survives pending searches");
            importer.ignore(1);expectCore(importer.summary()["ignoredTotal"].toInt()==1&&importer.rows()[1].toMap()["section"]=="ignored","ignore is explicit");
            // Technical failure: LOOKUP_ERROR (never NOT_FOUND); 503 continues with the next row, 429 pauses the search.
            fail503=true;importer.parse("Free Energy - Broken Song\nFree Energy - Girls Want Rock");importer.search();waitFor([&]{return !importer.busy();});
            expectCore(importer.rows()[0].toMap()["match"]=="LOOKUP_ERROR"&&importer.rows()[0].toMap()["section"]=="error"&&importer.rows()[1].toMap()["selected"].toBool(),"503 marks one row and continues");
            fail503=false;importer.retry(0);waitFor([&]{return !importer.busy();});expectCore(importer.rows()[0].toMap()["match"]!="LOOKUP_ERROR","retry clears a transient error");
            fail429=true;importer.parse("Free Energy - Limited Song\nFree Energy - Girls Want Rock");importer.search();waitFor([&]{return !importer.busy();});
            expectCore(importer.rows()[0].toMap()["match"]=="LOOKUP_ERROR"&&importer.rows()[1].toMap()["match"]=="PENDING"&&importer.status().contains("pausada"),"rate limit pauses the remaining references");
            fail429=false;
            // A late reply for a cancelled lookup never touches the new draft.
            importer.parse("Free Energy - Dream City");importer.search();importer.choose(0,0,true);importer.cancel();importer.parse("Free Energy - Girls Want Rock");
            for(int i=0;i<20;++i)QCoreApplication::processEvents();expectCore(importer.rows().size()==1&&importer.rows()[0].toMap()["title"]=="Girls Want Rock"&&importer.selectedCount()==0,"stale lookup ignored");
            // 300 references: bounded, consistent counters, no stall.
            QString big;for(int i=0;i<300;++i)big+=QString("Free Energy - Song %1\n").arg(i+1);
            QElapsedTimer timer;timer.start();importer.parse(big);importer.search();
            {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<60000){QCoreApplication::processEvents();}}
            expectCore(!importer.busy()&&importer.selectedCount()==300&&importer.summary()["total"].toInt()==300,"300 references processed");
            std::cout<<"TEXT_PLAYLIST_300_MS "<<timer.elapsed()<<" searches "<<searches<<"\n";
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
