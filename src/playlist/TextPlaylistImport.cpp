#include "TextPlaylistImport.hpp"
#include "companion/MusicCompanion.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <algorithm>

namespace tp=tonal::playlist;
namespace {
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
bool ok(int s){return s>=200&&s<300;}
QString failure(int status){return status==401||status==403?QStringLiteral("Autorize novamente a conta Spotify nas opções do app. A capa exige a permissão adicional de imagens."):QStringLiteral("Spotify HTTP %1. Sua playlist local continua salva.").arg(status);}
std::string utf8(const QString &s){return s.toUtf8().toStdString();}
QString text(const std::string &s){return QString::fromUtf8(s.data(),qsizetype(s.size()));}
QString now(){return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);}
QString norm(const QString &artist){return text(tp::normalize(utf8(artist)));}
// Lookup failures are technical: never shown as "not found".
QString lookupFailure(int status,const QJsonDocument &doc){
    const auto message=doc.object()["error"].toObject()["message"].toString();
    if(status==401||status==403)return QStringLiteral("Sessão Spotify sem permissão (HTTP %1). Autorize novamente e use Buscar pendentes.").arg(status);
    if(status==429)return message.isEmpty()?QStringLiteral("O Spotify limitou as consultas. Aguarde e use Buscar pendentes."):message;
    if(status<=0)return QStringLiteral("Falha de rede ou tempo esgotado");
    return QStringLiteral("Spotify HTTP %1 na busca").arg(status);
}
struct Parsed { tp::Candidate candidate; QVariantMap map; };
Parsed parseTrack(const QJsonObject &t,const tp::AlbumHit &owner){
    Parsed p;auto &c=p.candidate;QStringList names,ids;
    for(auto value:t["artists"].toArray()){const auto a=value.toObject();names<<a["name"].toString();ids<<a["id"].toString();c.artists.push_back(utf8(a["name"].toString()));c.artistIds.push_back(utf8(a["id"].toString()));}
    const auto album=t["album"].toObject();const auto images=album["images"].toArray();
    c.id=utf8(t["id"].toString());c.uri=utf8(t["uri"].toString());c.title=utf8(t["name"].toString());
    c.album=utf8(album.isEmpty()?text(owner.name):album["name"].toString());c.albumType=utf8(album.isEmpty()?text(owner.type):album["album_type"].toString());
    c.releaseDate=utf8(album.isEmpty()?text(owner.releaseDate):album["release_date"].toString());
    c.durationMs=t["duration_ms"].toInt();c.popularity=t.contains("popularity")?t["popularity"].toInt():-1;
    c.playable=t.contains("is_playable")?(t["is_playable"].toBool()?1:0):-1;
    c.restriction=utf8(t["restrictions"].toObject()["reason"].toString());c.linkedFrom=utf8(t["linked_from"].toObject()["uri"].toString());
    const QString image=images.isEmpty()?text(owner.image):images.first().toObject()["url"].toString();
    p.map={{"provider","Spotify"},{"id",t["uri"].toString()},{"uri",t["uri"].toString()},{"title",t["name"].toString()},{"artist",names.join(", ")},
        {"artists",names},{"artistIds",ids},{"album",text(c.album)},{"year",text(c.releaseDate).left(4)},{"image",image},{"durationMs",c.durationMs},
        {"playable",c.playable!=0},{"popularity",c.popularity},{"linkedFrom",text(c.linkedFrom)},{"restriction",text(c.restriction)}};
    return p;
}
QString statusText(const QVariantMap &row){
    if(row["replacedBy"].toInt()>0)return QStringLiteral("Substituída");
    if(row["duplicateOf"].toInt()>0&&!row["selected"].toBool())return QStringLiteral("Já está na playlist");
    if(row["manual"].toBool())return row["selected"].toBool()?QStringLiteral("Escolha manual"):QStringLiteral("Ignorada");
    const auto m=tp::matchFromCode(utf8(row["match"].toString()));
    if(m==tp::Match::Pending)return row["lookup"].toString()=="searching"?QStringLiteral("Buscando…"):QStringLiteral("Aguardando");
    auto value=text(tp::label(m));
    if(m==tp::Match::Equivalent)value+=row["selected"].toBool()?QStringLiteral(" · selecionada"):QStringLiteral(" · confirme");
    return value;
}
QVariantMap refreshed(QVariantMap row){row["status"]=statusText(row);row["section"]=TextPlaylistImport::sectionOf(row);return row;}
QString cacheKey(const QString &path,const QVariantMap &query){QString k=path;for(auto it=query.cbegin();it!=query.cend();++it)k+="|"+it.key()+"="+it.value().toString();return k;}
QVariantMap compactRow(QVariantMap row){
    // Drafts keep the chosen recording and the first candidates; the rest is found again by "Tentar de novo".
    auto list=row["candidates"].toList();const int choice=row["choice"].toInt();QVariantList kept;
    for(int i=0;i<list.size()&&kept.size()<TextPlaylistImport::kStoredCandidates;++i)kept<<list[i];
    if(choice>=TextPlaylistImport::kStoredCandidates&&choice<list.size()){kept<<list[choice];row["choice"]=int(kept.size())-1;}
    row["candidates"]=kept;row.remove("lookup");return row;
}
}
TextPlaylistImport::TextPlaylistImport(MusicCompanion *music,Request request,QObject *parent):QObject(parent),m_music(music),m_request(std::move(request)){
    m_autosave.setSingleShot(true);m_autosave.setInterval(3000);connect(&m_autosave,&QTimer::timeout,this,&TextPlaylistImport::autosave);
    refreshDrafts();
}
TextPlaylistImport::~TextPlaylistImport()=default;
QString TextPlaylistImport::sectionOf(const QVariantMap &row){
    if(row["replacedBy"].toInt()>0)return "replaced";
    if(row["ignored"].toBool())return "ignored";
    const auto candidates=row["candidates"].toList();const int choice=row["choice"].toInt();
    if(row["selected"].toBool()&&choice>=0&&choice<candidates.size())return "selected";
    if(row["duplicateOf"].toInt()>0)return "duplicate";
    switch(tp::matchFromCode(utf8(row["match"].toString()))){
    case tp::Match::Equivalent:case tp::Match::Ambiguous:case tp::Match::Exact:return "equivalent";
    case tp::Match::ArtistAmbiguous:return "ambiguous";
    case tp::Match::Alternate:return "alternate";
    case tp::Match::Unavailable:return "unavailable";
    case tp::Match::LookupError:case tp::Match::Incomplete:return "error";
    case tp::Match::NotFound:case tp::Match::WrongArtist:case tp::Match::WrongSong:case tp::Match::Cover:
        for(auto v:candidates)if(!v.toMap()["requestedArtist"].toBool())return "wrongArtist";
        return "notFound";
    default:return "waiting";
    }
}
int TextPlaylistImport::selectedCount() const{int n=0;for(auto v:m_rows){auto row=v.toMap();const int c=row["choice"].toInt();if(row["replacedBy"].toInt()<=0&&row["selected"].toBool()&&c>=0&&c<row["candidates"].toList().size())++n;}return n;}
QVariantMap TextPlaylistImport::summary() const{
    // Mutually exclusive sections; total = sum of sections; selected <= active <= total.
    QVariantMap out;int pending=0,ignored=0,selected=0,replaced=0;
    for(auto v:m_rows){const auto row=v.toMap();const auto section=row["section"].toString().isEmpty()?sectionOf(row):row["section"].toString();
        out[section]=out.value(section).toInt()+1;if(section=="selected")++selected;else if(section=="ignored")++ignored;else if(section=="replaced")++replaced;else ++pending;}
    out["selectedTotal"]=selected;out["pending"]=pending;out["ignoredTotal"]=ignored;out["replacedTotal"]=replaced;out["total"]=int(m_rows.size());
    out["active"]=int(m_rows.size())-replaced;out["toSave"]=selected;return out;
}
QVariantMap TextPlaylistImport::details() const{return m_music->playlistDetails(m_saved);}
int TextPlaylistImport::rowOf(int requestId) const{for(int i=0;i<m_rows.size();++i)if(m_rows[i].toMap()["requestId"].toInt()==requestId)return i;return -1;}
void TextPlaylistImport::touch(){if(m_rows.isEmpty()||!m_saved.isEmpty())return;m_dirty=true;m_autosave.start();}
void TextPlaylistImport::call(const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &body,const QByteArray &mime,Reply cb){
    const int serial=m_generation;QPointer<TextPlaylistImport> self(this);
    auto send=[self,serial,method,path,query,body,mime,cb]{if(!self||self->m_generation!=serial)return;self->m_request(method,path,query,body,mime,[self,serial,cb](int s,const QJsonDocument &doc){if(self&&self->m_generation==serial)cb(s,doc);},[self,serial]{return self&&self->m_generation==serial;});};
    if(method=="GET")send();else m_music->persist([self,serial,send](bool ok){if(!self||self->m_generation!=serial)return;if(ok)send();else self->finish("Não foi possível confirmar a gravação local. O envio ao Spotify foi interrompido; confira o espaço em disco e faça backup.");});
}
void TextPlaylistImport::parse(const QString &source,int format,const QString &artist){
    if(m_syncing)return;cancel();m_rows.clear();m_saved.clear();m_targetId.clear();m_artistCache.clear();m_responses.clear();m_batches.clear();m_pins.clear();
    m_draftId=QUuid::createUuid().toString(QUuid::WithoutBraces);m_draftName.clear();m_createdAt=now();m_format=format;m_defaultArtist=artist;m_nextBatch=0;
    try {const auto parsed=tp::parse(utf8(source),format,utf8(artist));m_detected=text(parsed.detectedArtist);
        const QString batch=QString("b%1").arg(++m_nextBatch);QVariantList ids;std::vector<tp::Song> songs;
        for(const auto &song:parsed.songs){m_rows<<refreshed({{"requestId",++m_nextId},{"artist",text(song.artist)},{"title",text(song.title)},{"original",text(song.original)},{"batchId",batch},
            {"match",QString(tp::code(tp::Match::Pending))},{"reason",QString()},{"candidates",QVariantList{}},{"choice",-1},{"selected",false},{"manual",false},{"ignored",false},{"lookup","waiting"}});ids<<m_nextId;songs.push_back(song);}
        if(!m_rows.isEmpty())m_batches<<QVariantMap{{"batchId",batch},{"createdAt",now()},{"label","Lista inicial"},{"fingerprint",text(tp::fingerprint(songs))},{"mode","add"},{"rows",ids},{"replaced",QVariantList{}}};
        m_status=m_rows.isEmpty()?"Nenhuma música reconhecida.":QString("%1 músicas. Revise o artista e busque as gravações.").arg(m_rows.size());
    }catch(const std::exception &e){m_status=QString::fromUtf8(e.what());}
    m_dirty=false;touch();emit changed();
}
void TextPlaylistImport::search(){startSearch({});}
void TextPlaylistImport::retry(int row){
    if(m_busy||m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();if(item["replacedBy"].toInt()>0)return;
    item["manual"]=false;item["ignored"]=false;item.remove("manualUri");item.remove("manualTrack");item.remove("duplicateOf");item["selected"]=false;m_rows[row]=refreshed(item);touch();
    startSearch({item["requestId"].toInt()});
}
void TextPlaylistImport::startSearch(const QSet<int> &only){
    if(m_busy||m_syncing||!m_saved.isEmpty())return;
    ++m_generation;m_busy=true;m_only=only;m_stopReason.clear();m_status="Buscando no Spotify…";emit changed();
    nextRow(0);
}
void TextPlaylistImport::nextRow(int row){
    for(;row<m_rows.size();++row){
        const auto item=m_rows[row].toMap();
        if(item["replacedBy"].toInt()>0)continue;
        if(!m_only.isEmpty()){if(m_only.contains(item["requestId"].toInt()))break;continue;}
        // "Buscar pendentes" re-queries unresolved references only; manual choices and selections are kept.
        if(item["manual"].toBool()||item["selected"].toBool()||item["duplicateOf"].toInt()>0)continue;
        break;
    }
    if(row>=m_rows.size()){finishSearch();return;}
    auto item=m_rows[row].toMap();
    m_lookup=std::make_unique<tp::Lookup>(tp::Song{utf8(item["artist"].toString()),utf8(item["title"].toString()),utf8(item["original"].toString())});
    m_lookupId=item["requestId"].toInt();m_pool.clear();
    const auto key=norm(item["artist"].toString());
    if(!key.isEmpty()&&m_artistCache.contains(key))m_lookup->seedArtists(m_artistCache.value(key));
    if(m_pins.contains(key)){tp::Context ctx;ctx.preferredId=utf8(m_pins[key].toMap()["id"].toString());ctx.preferredReason="cadastro fixado por você para "+utf8(item["artist"].toString())+" nesta lista";m_lookup->setContext(ctx);}
    item["lookup"]="searching";m_rows[row]=refreshed(item);
    m_status=QString("Buscando %1 de %2 • %3 — %4").arg(row+1).arg(m_rows.size()).arg(item["artist"].toString(),item["title"].toString());emit changed();
    step();
}
void TextPlaylistImport::step(){
    if(!m_lookup)return;
    const auto r=m_lookup->next();
    if(r.kind==tp::Request::None){completeRow();return;}
    QVariantMap query;for(const auto &[k,v]:r.query)query[text(k)]=text(v);
    const int id=m_lookupId;const auto kind=r.kind;const auto owner=r.album;const QString path=text(r.path);
    // Same request already answered in this import (the same artist on many lines): no second network call.
    const auto key=cacheKey(path,query);
    if(m_responses.contains(key)){const auto cached=m_responses.value(key);QTimer::singleShot(0,this,[this,id,kind,owner,cached]{handleReply(id,kind,owner,cached.first,cached.second);});return;}
    call("GET",path,query,{},{},[this,id,kind,owner,key](int s,const QJsonDocument &doc){
        if(s==200&&m_responses.size()<6000)m_responses.insert(key,{s,doc});
        handleReply(id,kind,owner,s,doc);
    });
}
void TextPlaylistImport::handleReply(int id,tp::Request::Kind kind,const tp::AlbumHit &owner,int s,const QJsonDocument &doc){
    if(!m_lookup||m_lookupId!=id)return;   // late reply of a cancelled/replaced lookup
    const int serial=m_generation;
    if(s!=200){
        m_lookup->fail(s,utf8(lookupFailure(s,doc)));
        if(m_lookup->finished()&&(s==401||s==403||s==429||s<=0))m_stopReason=lookupFailure(s,doc);
    } else if(kind==tp::Request::Artists){
        std::vector<tp::ArtistHit> hits;for(auto v:doc.object()["artists"].toObject()["items"].toArray()){const auto a=v.toObject();hits.push_back({utf8(a["id"].toString()),utf8(a["name"].toString())});}
        m_lookup->acceptArtists(hits);
    } else if(kind==tp::Request::Albums){
        std::vector<tp::AlbumHit> hits;for(auto v:doc.object()["items"].toArray()){const auto a=v.toObject();const auto images=a["images"].toArray();
            hits.push_back({utf8(a["id"].toString()),utf8(a["name"].toString()),utf8(a["album_type"].toString()),utf8(a["release_date"].toString()),utf8(images.isEmpty()?QString():images.first().toObject()["url"].toString())});}
        m_lookup->acceptAlbums(hits,doc.object().contains("total")?doc.object()["total"].toInt():-1);
    } else {
        const bool album=kind==tp::Request::AlbumTracks;
        const auto box=album?doc.object():doc.object()["tracks"].toObject();const auto items=box["items"].toArray();
        std::vector<tp::Candidate> candidates;QVariantList maps;
        for(auto v:items){const auto t=v.toObject();if(!t["uri"].toString().startsWith("spotify:track:"))continue;auto p=parseTrack(t,owner);candidates.push_back(std::move(p.candidate));maps<<p.map;}
        const auto indices=m_lookup->acceptTracks(candidates,album&&box.contains("total")?box["total"].toInt():-1);
        for(size_t i=0;i<indices.size();++i)if(indices[i]==int(m_pool.size()))m_pool<<maps[qsizetype(i)];
    }
    if(m_lookup->finished()){completeRow();return;}
    // Transient failure: wait the lookup's backoff plus jitter before repeating the same request.
    const int backoff=m_lookup->backoffMs();const int delay=backoff>0&&m_spacing>0?backoff+int(QRandomGenerator::global()->bounded(250)):m_spacing;
    QTimer::singleShot(delay,this,[this,id,serial]{if(m_generation==serial&&m_lookup&&m_lookupId==id)step();});
}
void TextPlaylistImport::applyDecision(int row){
    if(!m_lookup||row<0||row>=m_rows.size())return;
    const auto d=m_lookup->decision();auto item=m_rows[row].toMap();QVariantList ordered;
    for(const auto &e:d.ordered){if(e.index<0||e.index>=m_pool.size())continue;auto m=m_pool[e.index].toMap();
        m["match"]=QString(tp::code(e.match));m["matchLabel"]=text(tp::label(e.match));m["reason"]=text(e.reason);m["safe"]=e.safe;
        m["requestedArtist"]=tp::requestedArtistResult(e.match);m["group"]=QString(tp::group(e.match));ordered<<m;}
    item["match"]=QString(tp::code(d.match));item["reason"]=text(d.reason);item["requests"]=m_lookup->requests();
    item["discographyComplete"]=m_lookup->discographyComplete();item["releasesChecked"]=m_lookup->releasesChecked();
    item["lookup"]=d.match==tp::Match::LookupError?"error":"done";
    if(item["manual"].toBool()){
        // UserOverride: a late automatic result never replaces the user's decision.
        const auto uri=item["manualUri"].toString();int index=-1;
        for(int i=0;i<ordered.size()&&!uri.isEmpty();++i)if(ordered[i].toMap()["uri"].toString()==uri){index=i;break;}
        if(index<0&&item.contains("manualTrack")){ordered<<item["manualTrack"];index=int(ordered.size())-1;}
        item["choice"]=index;
    } else {item["choice"]=d.choice;item["selected"]=d.autoSelect;}
    item["candidates"]=ordered;m_rows[row]=refreshed(item);markDuplicate(row);
}
void TextPlaylistImport::markDuplicate(int row){
    // A recording already selected on an earlier active line of the list is not selected twice by accident (V3.1 14.3).
    auto item=m_rows[row].toMap();const auto list=item["candidates"].toList();const int choice=item["choice"].toInt();
    if(item["manual"].toBool()||!item["selected"].toBool()||choice<0||choice>=list.size())return;
    const auto uri=list[choice].toMap()["uri"].toString();
    for(int i=0;i<m_rows.size();++i){if(i==row)continue;const auto other=m_rows[i].toMap();const auto c=other["candidates"].toList();const int oc=other["choice"].toInt();
        if(other["replacedBy"].toInt()>0||!other["selected"].toBool()||oc<0||oc>=c.size()||c[oc].toMap()["uri"].toString()!=uri)continue;
        if(other["batchId"].toString()==item["batchId"].toString())continue;   // repeats inside one pasted list stay intentional
        item["selected"]=false;item["duplicateOf"]=other["requestId"];item["reason"]=QString("Já está na playlist (linha %1: %2 — %3). Marque para adicionar mesmo assim.").arg(i+1).arg(other["artist"].toString(),other["title"].toString());
        m_rows[row]=refreshed(item);return;}
}
void TextPlaylistImport::completeRow(){
    if(!m_lookup)return;
    const int row=rowOf(m_lookupId);applyDecision(row);
    const auto artist=norm(text(m_lookup->song().artist));
    if(!artist.isEmpty()&&m_lookup->artistsResolved())m_artistCache.insert(artist,m_lookup->artists());
    m_lookup.reset();m_lookupId=-1;m_pool.clear();touch();
    if(!m_stopReason.isEmpty()){finishSearch("Busca pausada: "+m_stopReason+" As referências restantes continuam pendentes.");return;}
    emit changed();
    const int serial=m_generation;QTimer::singleShot(m_spacing,this,[this,row,serial]{if(m_generation==serial&&m_busy&&!m_syncing)nextRow(row+1);});
}
void TextPlaylistImport::finishSearch(const QString &message){
    m_busy=false;m_only.clear();const auto s=summary();
    m_status=!message.isEmpty()?message:QString("Revisão pronta • %1 selecionadas, %2 pendentes, %3 ignoradas de %4. Homônimos de outros artistas ficam separados.")
        .arg(s["selectedTotal"].toInt()).arg(s["pending"].toInt()).arg(s["ignoredTotal"].toInt()).arg(s["active"].toInt());
    emit changed();
}
void TextPlaylistImport::cancel(){
    if(m_syncing)return;++m_generation;
    if(m_lookup){const int row=rowOf(m_lookupId);if(row>=0){auto item=m_rows[row].toMap();item["lookup"]="waiting";m_rows[row]=refreshed(item);}m_lookup.reset();m_lookupId=-1;m_pool.clear();}
    m_busy=false;m_only.clear();emit changed();
}
void TextPlaylistImport::choose(int row,int candidate,bool selected){
    // Allowed during a search: the choice becomes a UserOverride kept until an explicit retry/edit.
    if(m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();const auto values=item["candidates"].toList();if(candidate<0||candidate>=values.size()||item["replacedBy"].toInt()>0)return;
    item["choice"]=candidate;item["selected"]=selected;item["manual"]=true;item["ignored"]=!selected;
    item["manualUri"]=values[candidate].toMap()["uri"];item["manualTrack"]=values[candidate];
    m_rows[row]=refreshed(item);touch();emit changed();
}
void TextPlaylistImport::ignore(int row){
    if(m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();item["selected"]=false;item["manual"]=true;item["ignored"]=true;m_rows[row]=refreshed(item);touch();emit changed();
}
void TextPlaylistImport::edit(int row,const QString &artist,const QString &title){
    if(m_busy||!m_saved.isEmpty()||row<0||row>=m_rows.size()||title.trimmed().isEmpty())return;
    auto item=m_rows[row].toMap();item["artist"]=artist.trimmed().left(300);item["title"]=title.trimmed().left(300);item["choice"]=-1;item["selected"]=false;
    item["manual"]=false;item["ignored"]=false;item.remove("manualUri");item.remove("manualTrack");item.remove("duplicateOf");item["candidates"]=QVariantList{};
    item["match"]=QString(tp::code(tp::Match::Pending));item["reason"]=QString();item["lookup"]="waiting";m_rows[row]=refreshed(item);touch();emit changed();
}
void TextPlaylistImport::move(int row,int direction){if(m_busy||!m_saved.isEmpty())return;int other=row+direction;if(row<0||row>=m_rows.size()||other<0||other>=m_rows.size())return;m_rows.swapItemsAt(row,other);touch();emit changed();}
QVariantList TextPlaylistImport::selectedTracks(bool deduplicate,QVariantList *audit) const{
    QVariantList out;QSet<QString> seen;
    for(auto v:m_rows){auto row=v.toMap();auto values=row["candidates"].toList();int i=row["choice"].toInt();if(row["replacedBy"].toInt()>0||!row["selected"].toBool()||i<0||i>=values.size())continue;
        auto t=values[i].toMap();QString uri=t["uri"].toString();if(deduplicate&&seen.contains(uri))continue;seen.insert(uri);out<<MusicCompanion::safeTrack(t);
        QString reason=row["manual"].toBool()?QStringLiteral("Escolha manual: ")+t["reason"].toString():t["reason"].toString();
        if(row["replacementOf"].toInt()>0)reason+=QString(" (substitui a referência %1)").arg(row["replacementOf"].toInt());
        if(audit)*audit<<QVariantMap{{"requested",(row["artist"].toString()+" - "+row["title"].toString()).left(300)},{"uri",uri},
            {"match",t["match"].toString()},{"reason",reason},{"manual",row["manual"].toBool()}};}
    return out;
}

// ---- drafts ------------------------------------------------------------------------------------
QString TextPlaylistImport::draftFolder() const{return m_draftFolder.isEmpty()?m_music->storageFolder()+"/text-drafts":m_draftFolder;}
bool TextPlaylistImport::writeAtomically(const QString &path,const QByteArray &data){
    QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return false;
    if(file.write(data)!=data.size()){file.cancelWriting();return false;}
    return file.commit();   // QSaveFile writes a temporary file and renames it only after a complete flush
}
QJsonObject TextPlaylistImport::draftJson() const{
    QVariantList rows;for(auto v:m_rows)rows<<compactRow(v.toMap());
    return QJsonObject{{"schemaVersion",kDraftSchema},{"draftId",m_draftId},{"createdAt",m_createdAt.isEmpty()?now():m_createdAt},{"updatedAt",now()},
        {"source","Text to Tonalify"},{"name",m_draftName},{"detectedArtist",m_detected},{"format",m_format},{"defaultArtist",m_defaultArtist},
        {"cover",m_cover},{"market","from_token"},{"nextId",m_nextId},{"nextBatch",m_nextBatch},{"pins",QJsonObject::fromVariantMap(m_pins)},
        {"batches",QJsonArray::fromVariantList(m_batches)},{"rows",QJsonArray::fromVariantList(rows)},
        {"publication","not_started"}};   // drafts are local only: the playlist is created after "Salvar no Tonalify"
}
bool TextPlaylistImport::restore(const QJsonObject &o){
    if(o["schemaVersion"].toInt()!=kDraftSchema||o["draftId"].toString().isEmpty()||!o["rows"].isArray())return false;
    QVariantList rows;int maxId=0;
    for(auto v:o["rows"].toArray()){if(!v.isObject())return false;auto row=v.toObject().toVariantMap();if(row["requestId"].toInt()<=0||row["title"].toString().isEmpty())return false;
        maxId=std::max(maxId,row["requestId"].toInt());row["lookup"]=row["match"].toString()=="PENDING"?"waiting":"done";rows<<refreshed(row);}
    if(rows.size()>2000)return false;
    cancel();m_rows=rows;m_draftId=o["draftId"].toString();m_draftName=o["name"].toString();m_createdAt=o["createdAt"].toString();m_detected=o["detectedArtist"].toString();
    m_format=o["format"].toInt();m_defaultArtist=o["defaultArtist"].toString();m_cover=o["cover"].toString();m_nextId=std::max(maxId,o["nextId"].toInt());m_nextBatch=o["nextBatch"].toInt();
    m_pins=o["pins"].toObject().toVariantMap();m_batches=o["batches"].toArray().toVariantList();m_saved.clear();m_targetId.clear();m_artistCache.clear();m_responses.clear();
    return true;
}
bool TextPlaylistImport::saveDraft(const QString &name){
    if(m_rows.isEmpty()||!m_saved.isEmpty()){m_status="Não há revisão em andamento para salvar.";emit changed();return false;}
    if(!name.trimmed().isEmpty())m_draftName=name.trimmed().left(100);
    if(m_draftName.isEmpty())m_draftName=QString("Rascunho de %1").arg(QDateTime::currentDateTime().toString("dd/MM HH:mm"));
    const QString path=draftFolder()+"/"+m_draftId+".json";
    if(!QFileInfo::exists(path)&&m_drafts.size()>=kMaxDrafts){m_status=QString("Limite de %1 rascunhos. Exclua um rascunho antigo antes de salvar outro.").arg(kMaxDrafts);emit changed();return false;}
    if(!writeAtomically(path,QJsonDocument(draftJson()).toJson(QJsonDocument::Compact))){
        m_status="Não foi possível salvar o rascunho (espaço em disco ou permissão). Nada foi descartado; tente novamente.";emit changed();return false;}
    // Confirm by reading back what was written before claiming success.
    QFile check(path);if(!check.open(QIODevice::ReadOnly)||QJsonDocument::fromJson(check.readAll()).object()["draftId"].toString()!=m_draftId){
        m_status="O rascunho não pôde ser conferido depois da gravação. Tente salvar de novo.";emit changed();return false;}
    QFile::remove(draftFolder()+"/"+m_draftId+".autosave.json");m_autosave.stop();
    m_dirty=false;m_status=QString("Rascunho salvo: %1").arg(m_draftName);refreshDrafts();emit changed();return true;
}
void TextPlaylistImport::autosave(){
    // Crash protection only; the explicit exit question still decides what is kept.
    if(!m_dirty||m_rows.isEmpty()||!m_saved.isEmpty())return;
    auto o=draftJson();o["autosave"]=true;
    if(!writeAtomically(draftFolder()+"/"+m_draftId+".autosave.json",QJsonDocument(o).toJson(QJsonDocument::Compact)))m_status="Aviso: a cópia automática de segurança do rascunho falhou.";
}
bool TextPlaylistImport::loadDraft(const QString &id){
    if(m_busy||m_syncing||!QRegularExpression("^[A-Za-z0-9-]{8,64}(\\.autosave)?$").match(id).hasMatch())return false;
    QFile file(draftFolder()+"/"+id+".json");
    if(!file.open(QIODevice::ReadOnly)){m_status="Rascunho não encontrado.";emit changed();return false;}
    const auto doc=QJsonDocument::fromJson(file.readAll());
    if(!doc.isObject()||!restore(doc.object())){m_status="O rascunho está corrompido ou é de uma versão incompatível; ele foi mantido sem alterações.";emit changed();return false;}
    m_dirty=id.endsWith(".autosave");   // a recovered autosave still needs an explicit save
    m_status=QString("Rascunho aberto: %1 • %2 músicas. Escolhas manuais preservadas; use Buscar pendentes para rever o que faltou.").arg(m_draftName.isEmpty()?QString("sem nome"):m_draftName).arg(m_rows.size());
    emit changed();return true;
}
bool TextPlaylistImport::deleteDraft(const QString &id){
    if(!QRegularExpression("^[A-Za-z0-9-]{8,64}(\\.autosave)?$").match(id).hasMatch())return false;
    const QString base=id.endsWith(".autosave")?id.chopped(9):id;
    bool removed=QFile::remove(draftFolder()+"/"+id+".json");if(!id.endsWith(".autosave"))QFile::remove(draftFolder()+"/"+base+".autosave.json");
    refreshDrafts();return removed;
}
bool TextPlaylistImport::renameDraft(const QString &id,const QString &name){
    if(name.trimmed().isEmpty()||!QRegularExpression("^[A-Za-z0-9-]{8,64}$").match(id).hasMatch())return false;
    const QString path=draftFolder()+"/"+id+".json";QFile file(path);if(!file.open(QIODevice::ReadOnly))return false;
    auto o=QJsonDocument::fromJson(file.readAll()).object();file.close();if(o["draftId"].toString()!=id)return false;
    o["name"]=name.trimmed().left(100);o["updatedAt"]=now();
    if(!writeAtomically(path,QJsonDocument(o).toJson(QJsonDocument::Compact)))return false;
    if(id==m_draftId)m_draftName=o["name"].toString();refreshDrafts();emit changed();return true;
}
void TextPlaylistImport::discardChanges(){
    // "Sair sem salvar": drops unsaved work only; a previously saved draft file stays as it was.
    cancel();if(!m_draftId.isEmpty())QFile::remove(draftFolder()+"/"+m_draftId+".autosave.json");m_autosave.stop();
    m_rows.clear();m_batches.clear();m_pins.clear();m_draftId.clear();m_draftName.clear();m_dirty=false;m_status.clear();refreshDrafts();emit changed();
}
void TextPlaylistImport::refreshDrafts(){
    QVariantList list;QDir dir(draftFolder());QHash<QString,QString> saved;
    for(const auto &info:dir.entryInfoList({"*.json"},QDir::Files,QDir::Time)){
        QFile file(info.absoluteFilePath());if(info.size()>16*1024*1024||!file.open(QIODevice::ReadOnly))continue;
        const auto o=QJsonDocument::fromJson(file.readAll()).object();if(o["schemaVersion"].toInt()!=kDraftSchema)continue;
        const bool recovery=info.fileName().endsWith(".autosave.json");const QString id=recovery?o["draftId"].toString()+".autosave":o["draftId"].toString();
        int selected=0,pending=0,total=0;for(auto v:o["rows"].toArray()){const auto row=v.toObject().toVariantMap();if(row["replacedBy"].toInt()>0)continue;++total;const auto s=sectionOf(row);if(s=="selected")++selected;else if(s!="ignored")++pending;}
        if(!recovery)saved.insert(o["draftId"].toString(),o["updatedAt"].toString());
        list<<QVariantMap{{"id",id},{"name",recovery?QString("Recuperação automática: %1").arg(o["name"].toString().isEmpty()?QString("sessão interrompida"):o["name"].toString()):o["name"].toString()},
            {"updatedAt",o["updatedAt"].toString()},{"total",total},{"selected",selected},{"pending",pending},{"recovery",recovery},{"draftId",o["draftId"].toString()}};
    }
    // An autosave older than its saved draft is stale: only newer crash copies are offered.
    QVariantList out;for(auto v:list){const auto m=v.toMap();if(m["recovery"].toBool()&&saved.contains(m["draftId"].toString())&&saved[m["draftId"].toString()]>=m["updatedAt"].toString())continue;if(m["recovery"].toBool()&&m["draftId"].toString()==m_draftId)continue;out<<m;}
    m_drafts=out;emit draftsChanged();
}

// ---- more songs for the same playlist ------------------------------------------------------------
QVariantMap TextPlaylistImport::previewBatch(const QString &source,int format,const QString &artist) const{
    QVariantMap out;std::vector<tp::Song> songs;
    try{songs=tp::parse(utf8(source),format,utf8(artist.isEmpty()?m_defaultArtist:artist)).songs;}catch(const std::exception &e){out["error"]=QString::fromUtf8(e.what());return out;}
    QHash<QString,int> existing;for(int i=0;i<m_rows.size();++i){const auto r=m_rows[i].toMap();if(r["replacedBy"].toInt()>0)continue;existing.insert(text(tp::referenceKey({utf8(r["artist"].toString()),utf8(r["title"].toString()),{}})),i);}
    QVariantList duplicates;int index=0;for(const auto &s:songs){const auto key=text(tp::referenceKey(s));if(existing.contains(key)){const int at=existing.value(key);
        duplicates<<QVariantMap{{"index",index},{"artist",text(s.artist)},{"title",text(s.title)},{"existingRow",at+1}};}++index;}
    const auto print=text(tp::fingerprint(songs));bool repeated=false;for(auto b:m_batches)repeated|=b.toMap()["fingerprint"].toString()==print;
    QVariantList pending;for(auto v:m_rows){const auto r=v.toMap();const auto s=sectionOf(r);if(s!="selected"&&s!="replaced")pending<<QVariantMap{{"requestId",r["requestId"]},{"artist",r["artist"]},{"title",r["title"]},{"section",s}};}
    out["count"]=int(songs.size());out["duplicates"]=duplicates;out["repeatedList"]=repeated;out["pending"]=pending;
    out["newCount"]=int(songs.size())-int(duplicates.size());return out;
}
bool TextPlaylistImport::addBatch(const QString &source,int format,const QString &artist,const QString &mode,const QVariantList &replaceIds,bool includeDuplicates){
    if(m_busy||m_syncing||!m_saved.isEmpty()||m_rows.isEmpty())return false;
    std::vector<tp::Song> songs;
    try{songs=tp::parse(utf8(source),format,utf8(artist.isEmpty()?m_defaultArtist:artist)).songs;}catch(const std::exception &e){m_status=QString::fromUtf8(e.what());emit changed();return false;}
    const auto preview=previewBatch(source,format,artist);
    if(songs.empty()){m_status="Nenhuma música reconhecida na nova lista.";emit changed();return false;}
    if(preview["repeatedList"].toBool()&&!includeDuplicates){m_status="Esta mesma lista já foi adicionada. Confirme para adicionar de novo.";emit changed();return false;}
    QSet<int> skip;if(!includeDuplicates)for(auto d:preview["duplicates"].toList())skip.insert(d.toMap()["index"].toInt());
    if(int(m_rows.size())+int(songs.size())-int(skip.size())>500){m_status="Limite de 500 músicas por playlist do Text to Tonalify.";emit changed();return false;}
    const QString batch=QString("b%1").arg(++m_nextBatch);QVariantList ids,replaced;QSet<int> created;
    // Replacement targets: chosen pending lines (or every pending line) in list order.
    QList<int> targets;if(mode=="replace"){for(auto v:replaceIds)targets<<v.toInt();if(targets.isEmpty())for(auto p:preview["pending"].toList())targets<<p.toMap()["requestId"].toInt();}
    for(int i=0;i<int(songs.size());++i){
        if(skip.contains(i))continue;const auto &s=songs[size_t(i)];
        QVariantMap row{{"requestId",++m_nextId},{"artist",text(s.artist)},{"title",text(s.title)},{"original",text(s.original)},{"batchId",batch},
            {"match",QString(tp::code(tp::Match::Pending))},{"reason",QString()},{"candidates",QVariantList{}},{"choice",-1},{"selected",false},{"manual",false},{"ignored",false},{"lookup","waiting"}};
        int position=int(m_rows.size());
        while(!targets.isEmpty()){const int target=targets.takeFirst();const int at=rowOf(target);if(at<0)continue;auto old=m_rows[at].toMap();
            if(sectionOf(old)=="selected"||old["replacedBy"].toInt()>0)continue;
            // The original stays in the list as "Substituída" for audit and undo; the new line takes its place.
            old["replacedBy"]=m_nextId;m_rows[at]=refreshed(old);row["replacementOf"]=target;position=at+1;replaced<<QVariantMap{{"old",target},{"new",m_nextId}};break;}
        m_rows.insert(position,refreshed(row));ids<<m_nextId;created.insert(m_nextId);
    }
    m_batches<<QVariantMap{{"batchId",batch},{"createdAt",now()},{"label",QString("Lote %1").arg(m_batches.size()+1)},{"fingerprint",text(tp::fingerprint(songs))},{"mode",mode},{"rows",ids},{"replaced",replaced}};
    touch();m_status=QString("%1 música(s) adicionada(s)%2%3. Buscando só as novas…").arg(ids.size()).arg(replaced.isEmpty()?QString():QString(", %1 substituindo pendentes").arg(replaced.size()))
        .arg(skip.isEmpty()?QString():QString("; %1 duplicada(s) ignorada(s)").arg(skip.size()));
    emit changed();startSearch(created);return true;
}
bool TextPlaylistImport::undoLastBatch(){
    if(m_busy||m_syncing||!m_saved.isEmpty()||m_batches.size()<2)return false;
    const auto batch=m_batches.takeLast().toMap();QSet<int> ids;for(auto v:batch["rows"].toList())ids.insert(v.toInt());
    for(int i=int(m_rows.size())-1;i>=0;--i)if(ids.contains(m_rows[i].toMap()["requestId"].toInt()))m_rows.removeAt(i);
    for(auto v:batch["replaced"].toList()){const int at=rowOf(v.toMap()["old"].toInt());if(at<0)continue;auto old=m_rows[at].toMap();old.remove("replacedBy");m_rows[at]=refreshed(old);}
    touch();m_status=QString("Última importação desfeita (%1 música(s)). As escolhas dos lotes anteriores foram mantidas.").arg(ids.size());emit changed();return true;
}
bool TextPlaylistImport::pinArtist(int row,int candidate){
    if(m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return false;
    const auto item=m_rows[row].toMap();const auto list=item["candidates"].toList();if(candidate<0||candidate>=list.size())return false;
    const auto c=list[candidate].toMap();const auto names=c["artists"].toStringList();const auto ids=c["artistIds"].toStringList();QString id,name;
    for(int i=0;i<names.size()&&i<ids.size();++i)if(tp::sameArtist(utf8(item["artist"].toString()),utf8(names[i]))){id=ids[i];name=names[i];break;}
    if(!QRegularExpression("^[A-Za-z0-9]{22}$").match(id).hasMatch())return false;
    const auto key=norm(item["artist"].toString());m_pins[key]=QVariantMap{{"id",id},{"name",name},{"album",c["album"]},{"pinnedAt",now()}};
    // Every unresolved line of that artist is decided again with the pinned profile; manual choices stay.
    QSet<int> again;for(auto v:m_rows){const auto r=v.toMap();if(norm(r["artist"].toString())==key&&r["replacedBy"].toInt()<=0&&!r["manual"].toBool()&&!r["selected"].toBool())again.insert(r["requestId"].toInt());}
    touch();m_status=QString("Cadastro de %1 fixado para esta lista (álbum %2).").arg(name,c["album"].toString());emit changed();
    if(!again.isEmpty()&&!m_busy)startSearch(again);
    return true;
}
void TextPlaylistImport::unpinArtist(const QString &artist){if(m_pins.remove(norm(artist))){touch();emit changed();}}
bool TextPlaylistImport::setCover(const QUrl &url){
    if(m_busy||!m_saved.isEmpty()||!url.isLocalFile())return false;QFileInfo file(url.toLocalFile());
    if(file.size()>16*1024*1024){m_status="Imagem maior que 16 MiB.";emit changed();return false;}
    QImageReader reader(file.absoluteFilePath());auto size=reader.size();if(!size.isValid()||qint64(size.width())*size.height()>16000000){m_status="Escolha uma imagem com até 16 megapixels.";emit changed();return false;}
    reader.setAutoTransform(true);reader.setScaledSize(size.scaled(512,512,Qt::KeepAspectRatio));auto image=reader.read();QByteArray encoded;QBuffer buffer(&encoded);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"JPEG",80);
    if(encoded.isEmpty()||encoded.toBase64().size()>256*1024){m_status="Não foi possível preparar esta capa. Escolha outra imagem.";emit changed();return false;}
    m_cover="data:image/jpeg;base64,"+QString::fromLatin1(encoded.toBase64());emit changed();return true;
}
void TextPlaylistImport::clearCover(){if(!m_busy&&m_saved.isEmpty()){m_cover.clear();emit changed();}}
bool TextPlaylistImport::setTargetPlaylist(const QString &id){
    if(m_busy||!m_saved.isEmpty())return false;
    if(!id.isEmpty()&&!QRegularExpression("^[A-Za-z0-9]{22}$").match(id).hasMatch())return false;
    m_targetId=id;return true;
}
bool TextPlaylistImport::saveLocal(const QString &name,const QString &description,bool deduplicate){
    if(m_busy||!m_saved.isEmpty())return false;QVariantList audit;const auto tracks=selectedTracks(deduplicate,&audit);m_saved=m_music->importPlaylist(name,description,m_cover,tracks,audit);
    if(!m_saved.isEmpty()&&!m_targetId.isEmpty())patch({{"spotifyId",m_targetId},{"appendMode",true},{"appendBase",QVariantList{}},{"appendBaseReady",false},{"syncedCount",0},{"cloudComplete",false},{"cloudUncertain",false},{"coverSynced",true}});
    m_status=m_saved.isEmpty()?"Não foi possível salvar. Informe um nome, selecione músicas e confira o limite da biblioteca.":"Confirmando a gravação local…";
    if(!m_saved.isEmpty()){m_busy=true;const int serial=m_generation;QPointer<TextPlaylistImport> self(this);m_music->persist([self,serial](bool ok){if(!self||self->m_generation!=serial)return;
            // The draft becomes a saved playlist only after the library write is confirmed; then its files are removed.
            if(ok&&!self->m_draftId.isEmpty()){QFile::remove(self->draftFolder()+"/"+self->m_draftId+".json");QFile::remove(self->draftFolder()+"/"+self->m_draftId+".autosave.json");self->m_autosave.stop();self->m_dirty=false;self->refreshDrafts();}
            self->finish(ok?"Playlist salva no Tonalify. Rascunho concluído. Deseja salvar também na conta Spotify?":"A gravação local falhou; o rascunho continua disponível. Exporte um backup e confira o espaço em disco antes de enviar.");});}emit changed();return !m_saved.isEmpty();
}
bool TextPlaylistImport::loadSaved(const QString &id){if(m_busy)return false;auto p=m_music->playlistDetails(id);if(p.isEmpty())return false;++m_generation;m_rows.clear();m_dirty=false;m_autosave.stop();m_draftId.clear();m_saved=id;m_cover=p["cover"].toString();m_targetId=p["appendMode"].toBool()?p["spotifyId"].toString():QString();for(auto t:m_music->playlistTracks(id))m_rows<<QVariantMap{{"requestId",++m_nextId},{"artist",t.toMap()["artist"]},{"title",t.toMap()["title"]},{"candidates",QVariantList{t}},{"choice",0},{"selected",true},{"manual",false},{"ignored",false},{"match","EXACT_MATCH"},{"status","Salva"},{"section","selected"}};m_status="Playlist local. Você pode concluir a cópia para o Spotify.";emit changed();return true;}
void TextPlaylistImport::patch(const QVariantMap &v){m_music->updatePlaylistDetails(m_saved,v);}
void TextPlaylistImport::finish(const QString &text){m_busy=false;m_syncing=false;m_status=text;emit changed();}
void TextPlaylistImport::sync(bool publicPlaylist){
    if(m_busy||m_saved.isEmpty())return;auto p=details();m_upload=m_music->playlistTracks(m_saved);if(m_upload.isEmpty())return;m_cloudId=p["spotifyId"].toString();m_cursor=std::clamp(p["syncedCount"].toInt(),0,int(m_upload.size()));
    ++m_generation;m_busy=m_syncing=true;m_status="Copiando playlist para o Spotify…";emit changed();
    if(!m_cloudId.isEmpty()){if(!QRegularExpression("^[A-Za-z0-9]{22}$").match(m_cloudId).hasMatch()){finish("Identificação da playlist Spotify inválida.");return;}m_verifiedPages=0;if(p["appendMode"].toBool()||p["cloudUncertain"].toBool())verifyRemote("/playlists/"+m_cloudId+"/items?limit=50");else uploadBatch();return;}
    if(p["cloudUncertain"].toBool()){finish("O envio anterior não confirmou a criação. Confira sua Biblioteca Spotify antes de tentar criar outra cópia.");return;}
    patch({{"cloudUncertain",true},{"spotifyPublic",publicPlaylist}});
    call("POST","/me/playlists",{},json({{"name",p["name"].toString()},{"description",p["description"].toString()},{"public",publicPlaylist}}),"application/json",[this](int s,const QJsonDocument &doc){
        if(!ok(s)||!QRegularExpression("^[A-Za-z0-9]{22}$").match(doc.object()["id"].toString()).hasMatch()){if(s>=400&&s<500)patch({{"cloudUncertain",false}});finish(failure(s));return;}
        m_cloudId=doc.object()["id"].toString();m_cursor=0;patch({{"spotifyId",m_cloudId},{"syncedCount",0},{"cloudUncertain",false}});emit spotifyChanged();uploadBatch();
    });
}
void TextPlaylistImport::acknowledgeUncertainCreate(){if(!m_busy&&details()["spotifyId"].toString().isEmpty()){patch({{"cloudUncertain",false}});m_status="Verificação confirmada. Você pode solicitar uma nova cópia.";emit changed();}}
void TextPlaylistImport::verifyRemote(const QString &path,QVariantList accumulated){
    if(++m_verifiedPages>12){finish("Não foi possível conferir a paginação da playlist. Nenhuma faixa extra foi enviada.");return;}
    call("GET",path,{}, {},{},[this,accumulated](int s,const QJsonDocument &doc)mutable{
        if(!ok(s)){finish(failure(s));return;}auto o=doc.object();for(auto v:o["items"].toArray()){auto item=v.toObject();auto t=item["item"].toObject();if(t.isEmpty())t=item["track"].toObject();accumulated<<t["uri"].toString();}
        const auto state=details();const bool append=state["appendMode"].toBool();
        if(!append&&accumulated.size()>m_upload.size()){finish("A playlist Spotify foi alterada. O envio foi interrompido para preservar seu conteúdo.");return;}
        auto next=o["next"].toString();if(!next.isEmpty()){verifyRemote(next,accumulated);return;}
        if(append){
            auto base=state["appendBase"].toList();
            if(!state["appendBaseReady"].toBool()){
                m_cursor=0;patch({{"appendBase",accumulated},{"appendBaseReady",true},{"syncedCount",0},{"cloudUncertain",false}});uploadBatch();return;
            }
            if(accumulated.size()<base.size()||accumulated.size()>base.size()+m_upload.size()){
                finish("A playlist Spotify mudou desde o início da adição. Confira seu conteúdo antes de continuar.");return;
            }
            for(int i=0;i<base.size();++i)if(accumulated[i].toString()!=base[i].toString()){
                finish("A parte original da playlist Spotify mudou. Nenhuma faixa extra foi enviada.");return;
            }
            const int received=int(accumulated.size()-base.size());
            for(int i=0;i<received;++i)if(accumulated[base.size()+i].toString()!=m_upload[i].toMap()["uri"].toString()){
                finish("As adições confirmadas no Spotify divergem desta retomada. Confira a playlist antes de continuar.");return;
            }
            m_cursor=received;patch({{"syncedCount",m_cursor},{"cloudUncertain",false}});uploadBatch();return;
        }
        for(int i=0;i<accumulated.size();++i)if(accumulated[i].toString()!=m_upload[i].toMap()["uri"].toString()){finish("A ordem no Spotify mudou. Confira a playlist antes de continuar.");return;}
        m_cursor=int(accumulated.size());patch({{"syncedCount",m_cursor},{"cloudUncertain",false}});uploadBatch();
    });
}
void TextPlaylistImport::uploadBatch(){
    if(m_cursor>=m_upload.size()){patch({{"cloudComplete",true}});emit spotifyChanged();uploadCover();return;}
    QJsonArray uris;for(int i=m_cursor;i<std::min(m_cursor+100,int(m_upload.size()));++i)uris.append(m_upload[i].toMap()["uri"].toString());const int end=m_cursor+int(uris.size());
    patch({{"cloudUncertain",true}});m_status=QString("Spotify • enviando %1–%2 de %3").arg(m_cursor+1).arg(end).arg(m_upload.size());emit changed();
    call(details()["appendMode"].toBool()||m_cursor>0?"POST":"PUT","/playlists/"+m_cloudId+"/items",{},json({{"uris",uris}}),"application/json",[this,end](int s,const QJsonDocument &){if(!ok(s)){finish(failure(s)+" Na próxima tentativa, o conteúdo será conferido antes de continuar.");return;}m_cursor=end;patch({{"syncedCount",m_cursor},{"cloudUncertain",false}});uploadBatch();});
}
void TextPlaylistImport::uploadCover(){
    if(details()["appendMode"].toBool()){finish("Músicas adicionadas à playlist Spotify existente. A cópia local das adições continua salva.");return;}
    if(m_cover.isEmpty()||details()["coverSynced"].toBool()){finish("Playlist salva no Tonalify e no Spotify.");return;}
    const auto bytes=m_cover.section(',',1).toLatin1();
    call("PUT","/playlists/"+m_cloudId+"/images",{},bytes,"image/jpeg",[this](int s,const QJsonDocument &){if(ok(s)){patch({{"coverSynced",true}});finish("Playlist e capa salvas no Tonalify e no Spotify.");}else finish("Músicas copiadas. Capa pendente: "+failure(s)+" Autorize e use Enviar novamente para concluir só a capa.");});
}
