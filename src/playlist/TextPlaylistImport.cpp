#include "TextPlaylistImport.hpp"
#include "companion/MusicCompanion.h"
#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QRandomGenerator>
#include <QSet>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

namespace tp=tonal::playlist;
namespace {
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
bool ok(int s){return s>=200&&s<300;}
QString failure(int status){return status==401||status==403?QStringLiteral("Autorize novamente a conta Spotify nas opções do app. A capa exige a permissão adicional de imagens."):QStringLiteral("Spotify HTTP %1. Sua playlist local continua salva.").arg(status);}
std::string utf8(const QString &s){return s.toUtf8().toStdString();}
QString text(const std::string &s){return QString::fromUtf8(s.data(),qsizetype(s.size()));}
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
    Parsed p;auto &c=p.candidate;QStringList names;
    for(auto value:t["artists"].toArray()){const auto a=value.toObject();names<<a["name"].toString();c.artists.push_back(utf8(a["name"].toString()));c.artistIds.push_back(utf8(a["id"].toString()));}
    const auto album=t["album"].toObject();const auto images=album["images"].toArray();
    c.id=utf8(t["id"].toString());c.uri=utf8(t["uri"].toString());c.title=utf8(t["name"].toString());
    c.album=utf8(album.isEmpty()?text(owner.name):album["name"].toString());c.albumType=utf8(album.isEmpty()?text(owner.type):album["album_type"].toString());
    c.releaseDate=utf8(album.isEmpty()?text(owner.releaseDate):album["release_date"].toString());
    c.durationMs=t["duration_ms"].toInt();c.popularity=t.contains("popularity")?t["popularity"].toInt():-1;
    c.playable=t.contains("is_playable")?(t["is_playable"].toBool()?1:0):-1;
    c.restriction=utf8(t["restrictions"].toObject()["reason"].toString());c.linkedFrom=utf8(t["linked_from"].toObject()["uri"].toString());
    const QString image=images.isEmpty()?text(owner.image):images.first().toObject()["url"].toString();
    p.map={{"provider","Spotify"},{"id",t["uri"].toString()},{"uri",t["uri"].toString()},{"title",t["name"].toString()},{"artist",names.join(", ")},
        {"album",text(c.album)},{"image",image},{"durationMs",c.durationMs},{"playable",c.playable!=0},{"popularity",c.popularity},
        {"linkedFrom",text(c.linkedFrom)},{"restriction",text(c.restriction)}};
    return p;
}
QString statusText(const QVariantMap &row){
    if(row["manual"].toBool())return row["selected"].toBool()?QStringLiteral("Escolha manual"):QStringLiteral("Ignorada");
    const auto m=tp::matchFromCode(utf8(row["match"].toString()));
    if(m==tp::Match::Pending)return row["lookup"].toString()=="searching"?QStringLiteral("Buscando…"):QStringLiteral("Aguardando");
    auto value=text(tp::label(m));
    if(m==tp::Match::Equivalent)value+=row["selected"].toBool()?QStringLiteral(" · selecionada"):QStringLiteral(" · confirme");
    return value;
}
QVariantMap refreshed(QVariantMap row){row["status"]=statusText(row);row["section"]=TextPlaylistImport::sectionOf(row);return row;}
}
TextPlaylistImport::TextPlaylistImport(MusicCompanion *music,Request request,QObject *parent):QObject(parent),m_music(music),m_request(std::move(request)){}
TextPlaylistImport::~TextPlaylistImport()=default;
QString TextPlaylistImport::sectionOf(const QVariantMap &row){
    if(row["ignored"].toBool())return "ignored";
    const auto candidates=row["candidates"].toList();const int choice=row["choice"].toInt();
    if(row["selected"].toBool()&&choice>=0&&choice<candidates.size())return "selected";
    switch(tp::matchFromCode(utf8(row["match"].toString()))){
    case tp::Match::Equivalent:case tp::Match::Ambiguous:case tp::Match::Exact:return "equivalent";
    case tp::Match::Alternate:return "alternate";
    case tp::Match::Unavailable:return "unavailable";
    case tp::Match::LookupError:case tp::Match::Incomplete:return "error";
    case tp::Match::NotFound:case tp::Match::WrongArtist:case tp::Match::WrongSong:case tp::Match::Cover:
        for(auto v:candidates)if(!v.toMap()["requestedArtist"].toBool())return "wrongArtist";
        return "notFound";
    default:return "waiting";
    }
}
int TextPlaylistImport::selectedCount() const{int n=0;for(auto v:m_rows){auto row=v.toMap();const int c=row["choice"].toInt();if(row["selected"].toBool()&&c>=0&&c<row["candidates"].toList().size())++n;}return n;}
QVariantMap TextPlaylistImport::summary() const{
    QVariantMap out;int pending=0,ignored=0,selected=0;
    for(auto v:m_rows){const auto row=v.toMap();const auto section=row["section"].toString().isEmpty()?sectionOf(row):row["section"].toString();
        out[section]=out.value(section).toInt()+1;if(section=="selected")++selected;else if(section=="ignored")++ignored;else ++pending;}
    out["selectedTotal"]=selected;out["pending"]=pending;out["ignoredTotal"]=ignored;out["total"]=int(m_rows.size());return out;
}
QVariantMap TextPlaylistImport::details() const{return m_music->playlistDetails(m_saved);}
int TextPlaylistImport::rowOf(int requestId) const{for(int i=0;i<m_rows.size();++i)if(m_rows[i].toMap()["requestId"].toInt()==requestId)return i;return -1;}
void TextPlaylistImport::call(const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &body,const QByteArray &mime,Reply cb){
    const int serial=m_generation;QPointer<TextPlaylistImport> self(this);
    auto send=[self,serial,method,path,query,body,mime,cb]{if(!self||self->m_generation!=serial)return;self->m_request(method,path,query,body,mime,[self,serial,cb](int s,const QJsonDocument &doc){if(self&&self->m_generation==serial)cb(s,doc);},[self,serial]{return self&&self->m_generation==serial;});};
    if(method=="GET")send();else m_music->persist([self,serial,send](bool ok){if(!self||self->m_generation!=serial)return;if(ok)send();else self->finish("Não foi possível confirmar a gravação local. O envio ao Spotify foi interrompido; confira o espaço em disco e faça backup.");});
}
void TextPlaylistImport::parse(const QString &source,int format,const QString &artist){
    if(m_syncing)return;cancel();m_rows.clear();m_saved.clear();m_targetId.clear();m_artistCache.clear();
    try {const auto parsed=tp::parse(utf8(source),format,utf8(artist));m_detected=text(parsed.detectedArtist);
        for(const auto &song:parsed.songs)m_rows<<refreshed({{"requestId",++m_nextId},{"artist",text(song.artist)},{"title",text(song.title)},{"original",text(song.original)},
            {"match",QString(tp::code(tp::Match::Pending))},{"reason",QString()},{"candidates",QVariantList{}},{"choice",-1},{"selected",false},{"manual",false},{"ignored",false},{"lookup","waiting"}});
        m_status=m_rows.isEmpty()?"Nenhuma música reconhecida.":QString("%1 músicas. Revise o artista e busque as gravações.").arg(m_rows.size());
    }catch(const std::exception &e){m_status=QString::fromUtf8(e.what());}emit changed();
}
void TextPlaylistImport::search(){startSearch(-1);}
void TextPlaylistImport::retry(int row){
    if(m_busy||m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();item["manual"]=false;item["ignored"]=false;item.remove("manualUri");item.remove("manualTrack");item["selected"]=false;m_rows[row]=refreshed(item);
    startSearch(item["requestId"].toInt());
}
void TextPlaylistImport::startSearch(int only){
    if(m_busy||m_syncing||!m_saved.isEmpty())return;
    ++m_generation;m_busy=true;m_only=only;m_stopReason.clear();m_status="Buscando no Spotify…";emit changed();
    nextRow(only>=0?std::max(0,rowOf(only)):0);
}
void TextPlaylistImport::nextRow(int row){
    for(;row<m_rows.size();++row){
        const auto item=m_rows[row].toMap();
        if(m_only>=0&&item["requestId"].toInt()!=m_only)continue;
        // "Buscar pendentes" re-queries unresolved references only; manual choices and selections are kept.
        if(m_only<0&&(item["manual"].toBool()||item["selected"].toBool()))continue;
        break;
    }
    if(row>=m_rows.size()){finishSearch();return;}
    auto item=m_rows[row].toMap();
    m_lookup=std::make_unique<tp::Lookup>(tp::Song{utf8(item["artist"].toString()),utf8(item["title"].toString()),utf8(item["original"].toString())});
    m_lookupId=item["requestId"].toInt();m_pool.clear();
    const auto key=text(tp::normalize(utf8(item["artist"].toString())));
    if(!key.isEmpty()&&m_artistCache.contains(key))m_lookup->seedArtists(m_artistCache.value(key));
    item["lookup"]="searching";m_rows[row]=refreshed(item);
    m_status=QString("Buscando %1 de %2 • %3 — %4").arg(row+1).arg(m_rows.size()).arg(item["artist"].toString(),item["title"].toString());emit changed();
    step();
}
void TextPlaylistImport::step(){
    if(!m_lookup)return;
    const auto r=m_lookup->next();
    if(r.kind==tp::Request::None){completeRow();return;}
    QVariantMap query;for(const auto &[k,v]:r.query)query[text(k)]=text(v);
    const int id=m_lookupId;const auto kind=r.kind;const auto owner=r.album;const int serial=m_generation;
    call("GET",text(r.path),query,{},{},[this,id,kind,owner,serial](int s,const QJsonDocument &doc){
        if(!m_lookup||m_lookupId!=id)return;   // late reply of a cancelled/replaced lookup
        if(s!=200){
            m_lookup->fail(s,utf8(lookupFailure(s,doc)));
            if(m_lookup->finished()&&(s==401||s==403||s==429||s<=0))m_stopReason=lookupFailure(s,doc);
        } else if(kind==tp::Request::Artists){
            std::vector<tp::ArtistHit> hits;for(auto v:doc.object()["artists"].toObject()["items"].toArray()){const auto a=v.toObject();hits.push_back({utf8(a["id"].toString()),utf8(a["name"].toString())});}
            m_lookup->acceptArtists(hits);
        } else if(kind==tp::Request::Albums){
            std::vector<tp::AlbumHit> hits;for(auto v:doc.object()["items"].toArray()){const auto a=v.toObject();const auto images=a["images"].toArray();
                hits.push_back({utf8(a["id"].toString()),utf8(a["name"].toString()),utf8(a["album_type"].toString()),utf8(a["release_date"].toString()),utf8(images.isEmpty()?QString():images.first().toObject()["url"].toString())});}
            m_lookup->acceptAlbums(hits);
        } else {
            const auto items=kind==tp::Request::AlbumTracks?doc.object()["items"].toArray():doc.object()["tracks"].toObject()["items"].toArray();
            std::vector<tp::Candidate> candidates;QVariantList maps;
            for(auto v:items){const auto t=v.toObject();if(!t["uri"].toString().startsWith("spotify:track:"))continue;auto p=parseTrack(t,owner);candidates.push_back(std::move(p.candidate));maps<<p.map;}
            const auto indices=m_lookup->acceptTracks(candidates);
            for(size_t i=0;i<indices.size();++i)if(indices[i]==int(m_pool.size()))m_pool<<maps[qsizetype(i)];
        }
        if(m_lookup->finished()){completeRow();return;}
        // Transient failure: wait the lookup's backoff plus jitter before repeating the same request.
        const int backoff=m_lookup->backoffMs();const int delay=backoff>0&&m_spacing>0?backoff+int(QRandomGenerator::global()->bounded(250)):m_spacing;
        QTimer::singleShot(delay,this,[this,id,serial]{if(m_generation==serial&&m_lookup&&m_lookupId==id)step();});
    });
}
void TextPlaylistImport::applyDecision(int row){
    if(!m_lookup||row<0||row>=m_rows.size())return;
    const auto d=m_lookup->decision();auto item=m_rows[row].toMap();QVariantList ordered;
    for(const auto &e:d.ordered){if(e.index<0||e.index>=m_pool.size())continue;auto m=m_pool[e.index].toMap();
        m["match"]=QString(tp::code(e.match));m["matchLabel"]=text(tp::label(e.match));m["reason"]=text(e.reason);m["safe"]=e.safe;m["requestedArtist"]=tp::requestedArtistResult(e.match);ordered<<m;}
    item["match"]=QString(tp::code(d.match));item["reason"]=text(d.reason);item["requests"]=m_lookup->requests();
    item["lookup"]=d.match==tp::Match::LookupError?"error":"done";
    if(item["manual"].toBool()){
        // UserOverride: a late automatic result never replaces the user's decision.
        const auto uri=item["manualUri"].toString();int index=-1;
        for(int i=0;i<ordered.size()&&!uri.isEmpty();++i)if(ordered[i].toMap()["uri"].toString()==uri){index=i;break;}
        if(index<0&&item.contains("manualTrack")){ordered<<item["manualTrack"];index=int(ordered.size())-1;}
        item["choice"]=index;
    } else {item["choice"]=d.choice;item["selected"]=d.autoSelect;}
    item["candidates"]=ordered;m_rows[row]=refreshed(item);
}
void TextPlaylistImport::completeRow(){
    if(!m_lookup)return;
    const int row=rowOf(m_lookupId);applyDecision(row);
    const auto artist=text(tp::normalize(m_lookup->song().artist));
    if(!artist.isEmpty()&&m_lookup->artistsResolved())m_artistCache.insert(artist,m_lookup->artists());
    m_lookup.reset();m_lookupId=-1;m_pool.clear();
    if(!m_stopReason.isEmpty()){finishSearch("Busca pausada: "+m_stopReason+" As referências restantes continuam pendentes.");return;}
    if(m_only>=0){finishSearch();return;}
    emit changed();
    const int serial=m_generation;QTimer::singleShot(m_spacing,this,[this,row,serial]{if(m_generation==serial&&m_busy&&!m_syncing)nextRow(row+1);});
}
void TextPlaylistImport::finishSearch(const QString &message){
    m_busy=false;m_only=-1;const auto s=summary();
    m_status=!message.isEmpty()?message:QString("Revisão pronta • %1 selecionadas, %2 pendentes, %3 ignoradas de %4. Homônimos de outros artistas ficam separados.")
        .arg(s["selectedTotal"].toInt()).arg(s["pending"].toInt()).arg(s["ignoredTotal"].toInt()).arg(m_rows.size());
    emit changed();
}
void TextPlaylistImport::cancel(){
    if(m_syncing)return;++m_generation;
    if(m_lookup){const int row=rowOf(m_lookupId);if(row>=0){auto item=m_rows[row].toMap();item["lookup"]="waiting";m_rows[row]=refreshed(item);}m_lookup.reset();m_lookupId=-1;m_pool.clear();}
    m_busy=false;m_only=-1;emit changed();
}
void TextPlaylistImport::choose(int row,int candidate,bool selected){
    // Allowed during a search: the choice becomes a UserOverride kept until an explicit retry/edit.
    if(m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();const auto values=item["candidates"].toList();if(candidate<0||candidate>=values.size())return;
    item["choice"]=candidate;item["selected"]=selected;item["manual"]=true;item["ignored"]=!selected;
    item["manualUri"]=values[candidate].toMap()["uri"];item["manualTrack"]=values[candidate];
    m_rows[row]=refreshed(item);emit changed();
}
void TextPlaylistImport::ignore(int row){
    if(m_syncing||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;
    auto item=m_rows[row].toMap();item["selected"]=false;item["manual"]=true;item["ignored"]=true;m_rows[row]=refreshed(item);emit changed();
}
void TextPlaylistImport::edit(int row,const QString &artist,const QString &title){
    if(m_busy||!m_saved.isEmpty()||row<0||row>=m_rows.size()||title.trimmed().isEmpty())return;
    auto item=m_rows[row].toMap();item["artist"]=artist.trimmed().left(300);item["title"]=title.trimmed().left(300);item["choice"]=-1;item["selected"]=false;
    item["manual"]=false;item["ignored"]=false;item.remove("manualUri");item.remove("manualTrack");item["candidates"]=QVariantList{};
    item["match"]=QString(tp::code(tp::Match::Pending));item["reason"]=QString();item["lookup"]="waiting";m_rows[row]=refreshed(item);emit changed();
}
void TextPlaylistImport::move(int row,int direction){if(m_busy||!m_saved.isEmpty())return;int other=row+direction;if(row<0||row>=m_rows.size()||other<0||other>=m_rows.size())return;m_rows.swapItemsAt(row,other);emit changed();}
QVariantList TextPlaylistImport::selectedTracks(bool deduplicate,QVariantList *audit) const{
    QVariantList out;QSet<QString> seen;
    for(auto v:m_rows){auto row=v.toMap();auto values=row["candidates"].toList();int i=row["choice"].toInt();if(!row["selected"].toBool()||i<0||i>=values.size())continue;
        auto t=values[i].toMap();QString uri=t["uri"].toString();if(deduplicate&&seen.contains(uri))continue;seen.insert(uri);out<<MusicCompanion::safeTrack(t);
        if(audit)*audit<<QVariantMap{{"requested",(row["artist"].toString()+" - "+row["title"].toString()).left(300)},{"uri",uri},
            {"match",t["match"].toString()},{"reason",row["manual"].toBool()?QStringLiteral("Escolha manual: ")+t["reason"].toString():t["reason"].toString()},{"manual",row["manual"].toBool()}};}
    return out;
}
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
    if(!m_saved.isEmpty()){m_busy=true;const int serial=m_generation;QPointer<TextPlaylistImport> self(this);m_music->persist([self,serial](bool ok){if(self&&self->m_generation==serial)self->finish(ok?"Playlist salva no Tonalify. Deseja salvar também na conta Spotify?":"A gravação local falhou. Exporte um backup e confira o espaço em disco antes de enviar.");});}emit changed();return !m_saved.isEmpty();
}
bool TextPlaylistImport::loadSaved(const QString &id){if(m_busy)return false;auto p=m_music->playlistDetails(id);if(p.isEmpty())return false;++m_generation;m_rows.clear();m_saved=id;m_cover=p["cover"].toString();m_targetId=p["appendMode"].toBool()?p["spotifyId"].toString():QString();for(auto t:m_music->playlistTracks(id))m_rows<<QVariantMap{{"requestId",++m_nextId},{"artist",t.toMap()["artist"]},{"title",t.toMap()["title"]},{"candidates",QVariantList{t}},{"choice",0},{"selected",true},{"manual",false},{"ignored",false},{"match","EXACT_MATCH"},{"status","Salva"},{"section","selected"}};m_status="Playlist local. Você pode concluir a cópia para o Spotify.";emit changed();return true;}
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
