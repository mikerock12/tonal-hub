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
#include <QCryptographicHash>
#include <QDir>
#include <QRegularExpression>
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
                    result=QJsonDocument(QJsonObject{{"total",query.value("include_groups")=="appears_on"?0:1},{"items",query.value("include_groups")!="appears_on"?QJsonArray{QJsonObject{{"id","AAAAAAAAAAAAAAAAAAAAA1"},{"name","Dream City"},{"album_type","single"},{"images",QJsonArray{QJsonObject{{"url","https://i.scdn.co/image/dream"}}}}}}:QJsonArray{}}});}
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
            expectCore(searches>=before&&importer.rows()[0].toMap()["requests"].toInt()>0&&importer.rows()[1].toMap()["selected"].toBool()&&importer.rows()[2].toMap()["selected"].toBool(),"pending search kept resolved rows");
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
        {   // V3.1: drafts (save, reopen after restart, discard, two drafts, storage failure) and more songs for the same list.
            QTemporaryDir draftStorage;const QString drafts=draftStorage.filePath("text-drafts");
            auto idFor=[](const QString &v){const auto h=QCryptographicHash::hash(v.toUtf8(),QCryptographicHash::Md5).toHex();QString out;for(auto c:h)if(out.size()<22)out+=QChar(c);while(out.size()<22)out+='Z';return out;};
            QMap<QString,int> searchesByTitle;int failTitle=0;
            auto server=[&](const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &,const QByteArray &,TextPlaylistImport::Reply cb,std::function<bool()>){
                expectCore(method=="GET","lookup never writes");int status=200;QJsonObject out;const auto q=query.value("q").toString();
                auto track=[&](const QString &title,const QString &artist,const QString &artistId,const QString &album){return QJsonObject{{"uri","spotify:track:"+idFor(artist+"|"+title+"|"+album)},{"id",idFor(artist+"|"+title+"|"+album)},{"name",title},
                    {"artists",QJsonArray{QJsonObject{{"name",artist},{"id",artistId}}}},{"album",QJsonObject{{"name",album},{"album_type","album"}}},{"is_playable",true}};};
                if(path=="/search"&&query.value("type")=="artist"){
                    QJsonArray items;if(q.contains("Scrawl")){items.append(QJsonObject{{"id",idFor("Scrawl A")},{"name","Scrawl"}});items.append(QJsonObject{{"id",idFor("Scrawl B")},{"name","Scrawl"}});}
                    else{const auto name=QRegularExpression("artist:\"([^\"]+)\"").match(q).captured(1);items.append(QJsonObject{{"id",idFor(name)},{"name",name}});}
                    out={{"artists",QJsonObject{{"items",items}}}};
                } else if(path=="/search"){
                    const auto m=QRegularExpression("^track:\"([^\"]+)\" artist:\"([^\"]+)\"$").match(q);QJsonArray items;
                    if(m.hasMatch()){const auto title=m.captured(1),artist=m.captured(2);searchesByTitle[title]++;
                        if(failTitle>0&&title.startsWith("Broken")){status=503;--failTitle;}
                        else if(artist=="Scrawl"){items.append(track(title,"Scrawl",idFor("Scrawl A"),"Nature Film"));items.append(track(title,"Scrawl",idFor("Scrawl B"),"Smallmouth"));}
                        else if(!title.startsWith("Missing")&&!title.startsWith("Broken"))items.append(track(title,artist,idFor(artist),"Álbum"));}
                    out={{"tracks",QJsonObject{{"items",items}}}};
                } else if(path.startsWith("/artists/")){
                    QJsonArray items;if(query.value("include_groups")!="appears_on"){if(path.contains(idFor("Scrawl A")))items.append(QJsonObject{{"id",idFor("Nature Film")},{"name","Nature Film"},{"album_type","album"}});
                        if(path.contains(idFor("Scrawl B")))items.append(QJsonObject{{"id",idFor("Smallmouth")},{"name","Smallmouth"},{"album_type","album"}});}
                    out={{"items",items},{"total",int(items.size())}};
                } else if(path.startsWith("/albums/")){
                    QJsonArray items;if(path.contains(idFor("Nature Film"))){items.append(track("Charles","Scrawl",idFor("Scrawl A"),"Nature Film"));items.append(track("Other A","Scrawl",idFor("Scrawl A"),"Nature Film"));}
                    if(path.contains(idFor("Smallmouth"))){items.append(track("Charles","Scrawl",idFor("Scrawl B"),"Smallmouth"));items.append(track("Other B","Scrawl",idFor("Scrawl B"),"Smallmouth"));}
                    for(qsizetype k=0;k<items.size();++k){auto o=items[k].toObject();o.remove("album");items[k]=o;}
                    out={{"items",items},{"total",int(items.size())}};
                } else status=404;
                QTimer::singleShot(0,&app,[=]{cb(status,QJsonDocument(out));});
            };
            MusicCompanion music(nullptr,storage.path());
            auto list=[](int from,int to,const QString &prefix){QString t;for(int i=from;i<=to;++i)t+=QString("Banda %1 - %2 %3\n").arg(i%17).arg(prefix).arg(i);return t;};
            QString two;for(int i=1;i<=200;++i)two+=QString("Banda %1 - %2 %3\n").arg(i%17).arg(i%67==0||i==13||i==150?"Missing":"Song").arg(i);
            QVariantList savedRows;QString firstDraft;
            {
                TextPlaylistImport importer(&music,server);importer.setRequestSpacing(0);importer.setDraftFolder(drafts);
                importer.parse(two);importer.search();{QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<60000)QCoreApplication::processEvents();}
                const int selected=importer.selectedCount();const int pending=importer.summary()["pending"].toInt();
                expectCore(selected==196&&pending==4&&importer.dirty(),"200 references with pending lines");
                importer.choose(0,0,false);   // a manual decision that must survive
                expectCore(importer.saveDraft("Ohio teste")&&!importer.dirty()&&importer.drafts().size()==1,"DRAFT: explicit save confirmed");
                firstDraft=importer.draftId();savedRows=importer.rows();
                auto d=importer.drafts()[0].toMap();expectCore(d["name"]=="Ohio teste"&&d["total"].toInt()==200&&d["selected"].toInt()==195&&d["pending"].toInt()==4,"draft list shows name and counters");
                importer.ignore(5);expectCore(importer.dirty(),"later change is unsaved");
                importer.discardChanges();expectCore(importer.rows().isEmpty()&&importer.drafts().size()==1,"DRAFT-04: discard keeps the saved draft");
            }
            {   // DRAFT-02: another process instance (restart) restores order, choices and pendings exactly.
                TextPlaylistImport importer(&music,server);importer.setRequestSpacing(0);importer.setDraftFolder(drafts);
                expectCore(importer.drafts().size()==1&&importer.loadDraft(firstDraft),"draft reopened after restart");
                const auto rows=importer.rows();expectCore(rows.size()==savedRows.size(),"same number of references");
                bool same=true;for(int i=0;i<rows.size();++i){const auto a=rows[i].toMap(),b=savedRows[i].toMap();const auto ca=a["candidates"].toList(),cb=b["candidates"].toList();
                    same&=a["requestId"]==b["requestId"]&&a["title"]==b["title"]&&a["selected"]==b["selected"]&&a["manual"]==b["manual"]&&a["match"]==b["match"]&&a["choice"]==b["choice"]&&
                        (a["choice"].toInt()<0||ca[a["choice"].toInt()].toMap()["uri"]==cb[b["choice"].toInt()].toMap()["uri"]);}
                expectCore(same&&importer.selectedCount()==195&&!importer.dirty(),"order, manual choice, matches and track URIs restored");
                // APPEND-01: three more songs; only the new lines are searched; old choices untouched.
                searchesByTitle.clear();
                expectCore(importer.addBatch("Nova A - Extra 1\nNova B - Extra 2\nNova C - Extra 3",0,{},"add",{},false),"batch added");
                {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                auto sum=importer.summary();
                expectCore(sum["total"].toInt()==203&&importer.selectedCount()==198&&searchesByTitle.keys()==QStringList({"Extra 1","Extra 2","Extra 3"}),"APPEND-01: 203 lines, only the new ones searched");
                expectCore(!importer.rows()[0].toMap()["selected"].toBool()&&importer.rows()[0].toMap()["manual"].toBool(),"manual decision kept after append");
                // APPEND-03/04: duplicate reference and the same list pasted twice.
                auto preview=importer.previewBatch("Banda 1 - Song 1\nNova D - Extra 4",0,{});
                expectCore(preview["count"].toInt()==2&&preview["duplicates"].toList().size()==1&&preview["newCount"].toInt()==1,"APPEND-03: duplicate detected before adding");
                expectCore(importer.previewBatch("Nova A - Extra 1\nNova B - Extra 2\nNova C - Extra 3",0,{})["repeatedList"].toBool(),"the same list is recognised");
                expectCore(!importer.addBatch("Nova A - Extra 1\nNova B - Extra 2\nNova C - Extra 3",0,{},"add",{},false)&&importer.summary()["total"].toInt()==203,"APPEND-04: no silent second copy");
                expectCore(importer.addBatch("Banda 1 - Song 1\nNova D - Extra 4",0,{},"add",{},false),"duplicates skipped, new song added");
                {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                expectCore(importer.summary()["total"].toInt()==204,"only the new reference appended");
                expectCore(importer.undoLastBatch()&&importer.summary()["total"].toInt()==203&&importer.selectedCount()==198,"undo the last import only");
                // Recording already selected on an earlier line: not selected twice by accident.
                expectCore(importer.addBatch("Banda 2 - Song 2",0,{},"add",{},true),"intentional duplicate accepted for review");
                {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                const auto dup=importer.rows().last().toMap();
                expectCore(!dup["selected"].toBool()&&dup["duplicateOf"].toInt()>0&&dup["section"]=="duplicate","same track URI flagged as already in the playlist");
                expectCore(importer.undoLastBatch(),"undo duplicate");
                // APPEND-02/07: replace pending lines; a replacement that is not found stays pending and can be undone.
                const int pendingBefore=importer.summary()["pending"].toInt();
                failTitle=99;
                QVariantList notFound;for(auto v:importer.rows())if(v.toMap()["section"]=="notFound"&&notFound.size()<3)notFound<<v.toMap()["requestId"];
                expectCore(notFound.size()==3&&importer.addBatch("Nova E - Extra 5\nNova F - Missing 6\nNova G - Broken 7",0,{},"replace",notFound,false),"replacement batch mapped to chosen pending lines");
                {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                sum=importer.summary();
                expectCore(sum["replacedTotal"].toInt()==3&&sum["active"].toInt()==203&&sum["total"].toInt()==206&&importer.selectedCount()==199,"APPEND-02: active lines kept, three replaced, one valid substitute");
                int failed=0,missing=0;for(auto v:importer.rows()){const auto r=v.toMap();if(r["title"]=="Broken 7"&&r["match"]=="LOOKUP_ERROR")++failed;if(r["title"]=="Missing 6"&&r["selected"].toBool()==false&&r["replacementOf"].toInt()>0)++missing;}
                expectCore(failed==1&&missing==1&&sum["pending"].toInt()==pendingBefore-1,"APPEND-06/07: failure and not-found substitutes stay pending; old lines untouched");
                expectCore(importer.undoLastBatch()&&importer.summary()["replacedTotal"].toInt()==0&&importer.summary()["total"].toInt()==203,"replacement undone");
                failTitle=0;
                // Saved again and reopened: the third batch survives.
                expectCore(importer.saveDraft()&&importer.batches().size()==2,"draft with two batches saved");
            }
            {   // DRAFT-05: a second draft; deleting one keeps the other. DRAFT-03: storage failure is reported, not claimed.
                TextPlaylistImport importer(&music,server);importer.setRequestSpacing(0);importer.setDraftFolder(drafts);
                importer.parse("Scrawl - Charles\nScrawl - Clock Song");importer.search();{QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                auto row=importer.rows()[0].toMap();
                expectCore(row["match"]=="ARTIST_AMBIGUOUS"&&row["section"]=="ambiguous"&&!row["selected"].toBool(),"two Scrawl profiles without shared discography stay to review");
                int b=-1;const auto cands=row["candidates"].toList();for(int i=0;i<cands.size();++i)if(cands[i].toMap()["album"]=="Smallmouth")b=i;
                expectCore(b>=0&&importer.pinArtist(0,b),"profile pinned for the list");
                {QElapsedTimer t;t.start();while(importer.busy()&&t.elapsed()<20000)QCoreApplication::processEvents();}
                for(auto v:importer.rows()){const auto r=v.toMap();const auto c=r["candidates"].toList();expectCore(r["selected"].toBool()&&c[r["choice"].toInt()].toMap()["album"]=="Smallmouth","pinned profile applied to every Scrawl line");}
                expectCore(importer.saveDraft("Segundo")&&importer.drafts().size()==2,"second draft");
                const QString second=importer.draftId();
                TextPlaylistImport other(&music,server);other.setDraftFolder(drafts);
                QString first;for(auto v:other.drafts())if(v.toMap()["id"]!=second)first=v.toMap()["id"].toString();
                expectCore(other.deleteDraft(first)&&other.drafts().size()==1&&other.loadDraft(second)&&other.rows().size()==2&&other.pins().size()==1,"DRAFT-05: deleting one draft keeps the other, pins included");
                QFile blocker(draftStorage.filePath("blocked"));expectCore(blocker.open(QIODevice::WriteOnly),"blocker");blocker.close();
                other.setDraftFolder(draftStorage.filePath("blocked")+"/inside");other.ignore(1);
                expectCore(!other.saveDraft()&&other.status().contains("Não foi possível salvar")&&other.dirty(),"DRAFT-03: storage failure is not reported as saved");
                // Autosave protects a crash without replacing the explicit question.
                other.setDraftFolder(drafts);other.ignore(0);{QElapsedTimer t;t.start();while(!QFileInfo::exists(drafts+"/"+second+".autosave.json")&&t.elapsed()<6000)QCoreApplication::processEvents();}
                expectCore(QFileInfo::exists(drafts+"/"+second+".autosave.json"),"autosave copy written");
            }
            {   TextPlaylistImport after(&music,server);after.setDraftFolder(drafts);bool recovery=false;for(auto v:after.drafts())recovery|=v.toMap()["recovery"].toBool();
                expectCore(recovery,"interrupted session offered as automatic recovery");}
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
