#include "TextPlaylistImport.hpp"
#include "TextPlaylistCore.hpp"
#include "companion/MusicCompanion.h"
#include <QBuffer>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>

namespace {
QByteArray json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
bool ok(int s){return s>=200&&s<300;}
QString failure(int status){return status==401||status==403?QStringLiteral("Autorize novamente a conta Spotify nas opções do app. A capa exige a permissão adicional de imagens."):QStringLiteral("Spotify HTTP %1. Sua playlist local continua salva.").arg(status);}
QVariantMap track(const QJsonObject &t){
    QStringList names;for(auto a:t["artists"].toArray())names<<a.toObject()["name"].toString();
    auto album=t["album"].toObject();auto images=album["images"].toArray();
    return {{"provider","Spotify"},{"id",t["uri"].toString()},{"uri",t["uri"].toString()},
        {"title",t["name"].toString()},{"artist",names.join(", ")},{"album",album["name"].toString()},
        {"image",images.isEmpty()?QString():images.first().toObject()["url"].toString()},
        {"durationMs",t["duration_ms"].toInt()},{"playable",t["is_playable"].toBool(true)},
        {"popularity",t.contains("popularity")?t["popularity"].toInt():-1}};
}
}
TextPlaylistImport::TextPlaylistImport(MusicCompanion *music,Request request,QObject *parent):QObject(parent),m_music(music),m_request(std::move(request)){}
int TextPlaylistImport::selectedCount() const{int n=0;for(auto v:m_rows){auto row=v.toMap();if(row["selected"].toBool()&&row["choice"].toInt()>=0)++n;}return n;}
QVariantMap TextPlaylistImport::details() const{return m_music->playlistDetails(m_saved);}
void TextPlaylistImport::call(const QByteArray &method,const QString &path,const QVariantMap &query,const QByteArray &body,const QByteArray &mime,Reply cb){
    const int serial=m_generation;QPointer<TextPlaylistImport> self(this);
    auto send=[self,serial,method,path,query,body,mime,cb]{if(!self||self->m_generation!=serial)return;self->m_request(method,path,query,body,mime,[self,serial,cb](int s,const QJsonDocument &doc){if(self&&self->m_generation==serial)cb(s,doc);},[self,serial]{return self&&self->m_generation==serial;});};
    if(method=="GET")send();else m_music->persist([self,serial,send](bool ok){if(!self||self->m_generation!=serial)return;if(ok)send();else self->finish("Não foi possível confirmar a gravação local. O envio ao Spotify foi interrompido; confira o espaço em disco e faça backup.");});
}
void TextPlaylistImport::parse(const QString &text,int format,const QString &artist){
    if(m_syncing)return;cancel();m_rows.clear();m_saved.clear();
    try {const auto parsed=tonal::playlist::parse(text.toUtf8().toStdString(),format,artist.toUtf8().toStdString());m_detected=QString::fromStdString(parsed.detectedArtist);
        for(const auto &song:parsed.songs)m_rows<<QVariantMap{{"artist",QString::fromStdString(song.artist)},{"title",QString::fromStdString(song.title)},{"original",QString::fromStdString(song.original)},{"status","Aguardando"},{"candidates",QVariantList{}},{"choice",-1},{"selected",false}};
        m_status=m_rows.isEmpty()?"Nenhuma música reconhecida.":QString("%1 músicas. Revise o artista e busque as gravações.").arg(m_rows.size());
    }catch(const std::exception &e){m_status=QString::fromUtf8(e.what());}emit changed();
}
void TextPlaylistImport::search(){if(m_busy||!m_saved.isEmpty())return;++m_generation;m_busy=true;m_status="Buscando no Spotify…";emit changed();nextSearch(0);}
void TextPlaylistImport::cancel(){if(m_syncing)return;++m_generation;m_busy=false;emit changed();}
void TextPlaylistImport::nextSearch(int row,int attempt){
    if(row>=m_rows.size()){m_busy=false;m_status=QString("Revisão pronta • %1 de %2 selecionadas. Confira as versões e as faixas não encontradas.").arg(selectedCount()).arg(m_rows.size());emit changed();return;}
    auto item=m_rows[row].toMap();if(attempt==0&&item.value("choice").toInt()>=0){nextSearch(row+1);return;}
    tonal::playlist::Song song{item["artist"].toString().toUtf8().toStdString(),item["title"].toString().toUtf8().toStdString(),{}};
    const auto queries=tonal::playlist::queries(song);
    if(attempt>=int(queries.size())){item["status"]="Não encontrada";m_rows[row]=item;emit changed();const int serial=m_generation;QTimer::singleShot(180,this,[this,row,serial]{if(m_generation==serial&&m_busy&&!m_syncing)nextSearch(row+1);});return;}
    item["status"]="Buscando…";m_rows[row]=item;emit changed();
    call("GET","/search",{{"q",QString::fromStdString(queries[attempt])},{"type","track"},{"limit",10}}, {},{},[this,row,attempt,song](int s,const QJsonDocument &doc){
        auto item=m_rows[row].toMap();if(s!=200){item["status"]=failure(s);m_rows[row]=item;m_busy=false;m_status=failure(s);emit changed();return;}
        auto values=doc.object()["tracks"].toObject()["items"].toArray();QVariantList tracks;std::vector<tonal::playlist::Candidate> candidates;
        for(auto v:values){auto t=track(v.toObject());if(!t["uri"].toString().startsWith("spotify:track:")||!t["playable"].toBool())continue;tracks<<t;candidates.push_back({t["title"].toString().toUtf8().toStdString(),t["artist"].toString().toUtf8().toStdString(),t["popularity"].toInt()});if(tracks.size()==10)break;}
        if(tracks.isEmpty()){nextSearch(row,attempt+1);return;}
        QVariantList ordered;auto ranking=tonal::playlist::rank(song,candidates);for(const auto &r:ranking)ordered<<tracks[r.index];
        item["candidates"]=ordered;item["choice"]=0;item["selected"]=ranking.front().highConfidence;
        item["status"]=ranking.front().highConfidence?"Encontrada":"Revisar versão";m_rows[row]=item;emit changed();
        const int serial=m_generation;QTimer::singleShot(180,this,[this,row,serial]{if(m_generation==serial&&m_busy&&!m_syncing)nextSearch(row+1);});
    });
}
void TextPlaylistImport::choose(int row,int candidate,bool selected){if(m_busy||!m_saved.isEmpty()||row<0||row>=m_rows.size())return;auto item=m_rows[row].toMap();auto values=item["candidates"].toList();if(candidate<0||candidate>=values.size())return;item["choice"]=candidate;item["selected"]=selected;item["status"]=selected?"Revisada":"Ignorada";m_rows[row]=item;emit changed();}
void TextPlaylistImport::edit(int row,const QString &artist,const QString &title){if(m_busy||!m_saved.isEmpty()||row<0||row>=m_rows.size()||title.trimmed().isEmpty())return;auto item=m_rows[row].toMap();item["artist"]=artist.trimmed().left(300);item["title"]=title.trimmed().left(300);item["choice"]=-1;item["selected"]=false;item["candidates"]=QVariantList{};item["status"]="Aguardando";m_rows[row]=item;emit changed();}
void TextPlaylistImport::move(int row,int direction){if(m_busy||!m_saved.isEmpty())return;int other=row+direction;if(row<0||row>=m_rows.size()||other<0||other>=m_rows.size())return;m_rows.swapItemsAt(row,other);emit changed();}
QVariantList TextPlaylistImport::selectedTracks(bool deduplicate) const{QVariantList out;QSet<QString> seen;for(auto v:m_rows){auto row=v.toMap();auto values=row["candidates"].toList();int i=row["choice"].toInt();if(!row["selected"].toBool()||i<0||i>=values.size())continue;auto t=values[i].toMap();QString uri=t["uri"].toString();if(deduplicate&&seen.contains(uri))continue;seen.insert(uri);out<<MusicCompanion::safeTrack(t);}return out;}
bool TextPlaylistImport::setCover(const QUrl &url){
    if(m_busy||!m_saved.isEmpty()||!url.isLocalFile())return false;QFileInfo file(url.toLocalFile());
    if(file.size()>16*1024*1024){m_status="Imagem maior que 16 MiB.";emit changed();return false;}
    QImageReader reader(file.absoluteFilePath());auto size=reader.size();if(!size.isValid()||qint64(size.width())*size.height()>16000000){m_status="Escolha uma imagem com até 16 megapixels.";emit changed();return false;}
    reader.setAutoTransform(true);reader.setScaledSize(size.scaled(512,512,Qt::KeepAspectRatio));auto image=reader.read();QByteArray encoded;QBuffer buffer(&encoded);buffer.open(QIODevice::WriteOnly);image.save(&buffer,"JPEG",80);
    if(encoded.isEmpty()||encoded.toBase64().size()>256*1024){m_status="Não foi possível preparar esta capa. Escolha outra imagem.";emit changed();return false;}
    m_cover="data:image/jpeg;base64,"+QString::fromLatin1(encoded.toBase64());emit changed();return true;
}
void TextPlaylistImport::clearCover(){if(!m_busy&&m_saved.isEmpty()){m_cover.clear();emit changed();}}
bool TextPlaylistImport::saveLocal(const QString &name,const QString &description,bool deduplicate){
    if(m_busy||!m_saved.isEmpty())return false;m_saved=m_music->importPlaylist(name,description,m_cover,selectedTracks(deduplicate));
    m_status=m_saved.isEmpty()?"Não foi possível salvar. Informe um nome, selecione músicas e confira o limite da biblioteca.":"Confirmando a gravação local…";
    if(!m_saved.isEmpty()){m_busy=true;const int serial=m_generation;QPointer<TextPlaylistImport> self(this);m_music->persist([self,serial](bool ok){if(self&&self->m_generation==serial)self->finish(ok?"Playlist salva no Tonalify. Deseja salvar também na conta Spotify?":"A gravação local falhou. Exporte um backup e confira o espaço em disco antes de enviar.");});}emit changed();return !m_saved.isEmpty();
}
bool TextPlaylistImport::loadSaved(const QString &id){if(m_busy)return false;auto p=m_music->playlistDetails(id);if(p.isEmpty())return false;++m_generation;m_rows.clear();m_saved=id;m_cover=p["cover"].toString();for(auto t:m_music->playlistTracks(id))m_rows<<QVariantMap{{"artist",t.toMap()["artist"]},{"title",t.toMap()["title"]},{"candidates",QVariantList{t}},{"choice",0},{"selected",true},{"status","Salva"}};m_status="Playlist local. Você pode concluir a cópia para o Spotify.";emit changed();return true;}
void TextPlaylistImport::patch(const QVariantMap &v){m_music->updatePlaylistDetails(m_saved,v);}
void TextPlaylistImport::finish(const QString &text){m_busy=false;m_syncing=false;m_status=text;emit changed();}
void TextPlaylistImport::sync(bool publicPlaylist){
    if(m_busy||m_saved.isEmpty())return;auto p=details();m_upload=m_music->playlistTracks(m_saved);if(m_upload.isEmpty())return;m_cloudId=p["spotifyId"].toString();m_cursor=std::clamp(p["syncedCount"].toInt(),0,int(m_upload.size()));
    ++m_generation;m_busy=m_syncing=true;m_status="Copiando playlist para o Spotify…";emit changed();
    if(!m_cloudId.isEmpty()){if(!QRegularExpression("^[A-Za-z0-9]{22}$").match(m_cloudId).hasMatch()){finish("Identificação da playlist Spotify inválida.");return;}m_verifiedPages=0;if(p["cloudUncertain"].toBool())verifyRemote("/playlists/"+m_cloudId+"/items?limit=50");else uploadBatch();return;}
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
        if(accumulated.size()>m_upload.size()){finish("A playlist Spotify foi alterada. O envio foi interrompido para preservar seu conteúdo.");return;}
        auto next=o["next"].toString();if(!next.isEmpty()){verifyRemote(next,accumulated);return;}
        for(int i=0;i<accumulated.size();++i)if(accumulated[i].toString()!=m_upload[i].toMap()["uri"].toString()){finish("A ordem no Spotify mudou. Confira a playlist antes de continuar.");return;}
        m_cursor=int(accumulated.size());patch({{"syncedCount",m_cursor},{"cloudUncertain",false}});uploadBatch();
    });
}
void TextPlaylistImport::uploadBatch(){
    if(m_cursor>=m_upload.size()){patch({{"cloudComplete",true}});emit spotifyChanged();uploadCover();return;}
    QJsonArray uris;for(int i=m_cursor;i<std::min(m_cursor+100,int(m_upload.size()));++i)uris.append(m_upload[i].toMap()["uri"].toString());const int end=m_cursor+int(uris.size());
    patch({{"cloudUncertain",true}});m_status=QString("Spotify • enviando %1–%2 de %3").arg(m_cursor+1).arg(end).arg(m_upload.size());emit changed();
    call(m_cursor==0?"PUT":"POST","/playlists/"+m_cloudId+"/items",{},json({{"uris",uris}}),"application/json",[this,end](int s,const QJsonDocument &){if(!ok(s)){finish(failure(s)+" Na próxima tentativa, o conteúdo será conferido antes de continuar.");return;}m_cursor=end;patch({{"syncedCount",m_cursor},{"cloudUncertain",false}});uploadBatch();});
}
void TextPlaylistImport::uploadCover(){
    if(m_cover.isEmpty()||details()["coverSynced"].toBool()){finish("Playlist salva no Tonalify e no Spotify.");return;}
    const auto bytes=m_cover.section(',',1).toLatin1();
    call("PUT","/playlists/"+m_cloudId+"/images",{},bytes,"image/jpeg",[this](int s,const QJsonDocument &){if(ok(s)){patch({{"coverSynced",true}});finish("Playlist e capa salvas no Tonalify e no Spotify.");}else finish("Músicas copiadas. Capa pendente: "+failure(s)+" Autorize e use Enviar novamente para concluir só a capa.");});
}
