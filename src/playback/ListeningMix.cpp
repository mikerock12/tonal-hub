#include "ListeningMix.hpp"
#include "ListeningMixCore.hpp"
#include "playlist/TextPlaylistCore.hpp"
#include "SpotifyApi.h"
#include "companion/MusicCompanion.h"
#include <QJsonArray>
#include <QFile>
#include <QRandomGenerator>
#include <QTimer>
#include <QSet>
#include <algorithm>
ListeningMix::ListeningMix(SpotifyApi*a,MusicCompanion*m,QObject*p):QObject(p),m_api(a),m_music(m){QFile f(":/companion/resources/music-rules.json");if(f.open(QIODevice::ReadOnly))m_rules=QJsonDocument::fromJson(f.readAll()).object();}
QVariantList ListeningMix::order(int count,quint32 seed)const{QVariantList out;for(int i:tonal::mix::order(count,seed))out<<i;return out;}
void ListeningMix::cancel(){++m_generation;m_busy=false;emit changed();}
QVariantMap ListeningMix::known(const QVariantMap&t)const{return m_music->m_data["tracks"].toObject()[MusicCompanion::key(t)].toObject().toVariantMap();}
QString ListeningMix::genres(const QVariantMap&t)const{auto g=known(t)["genre"].toString();if(g.isEmpty())g=t["genre"].toString();if(!g.isEmpty())return g;auto name=tonal::playlist::normalize(t["artist"].toString().toStdString());for(auto v:m_rules["recommendations"].toArray()){auto r=v.toObject();for(auto a:r["artists"].toArray())if(name==tonal::playlist::normalize(a.toString().toStdString())){QStringList tags;for(auto tag:r["genres"].toArray())tags<<tag.toString();return tags.join(',');}}return {};}
void ListeningMix::play(const QVariantList&t,int n,bool r,bool d,bool a){prepare(t,n,r,d,a,false);}
void ListeningMix::preview(const QVariantList&t,int n,bool r,bool d,bool a){prepare(t,n,r,d,a,true);}
void ListeningMix::applyPreview(int position){if(!m_busy&&!m_previewQueue.isEmpty())emit resumeReady(m_previewQueue,std::max(0,position));}
void ListeningMix::prepare(const QVariantList &tracks, int selected, bool random,
                           bool discovery, bool album, bool preview) {
 cancel();
 m_preview=preview;m_previewQueue.clear();m_failure.clear();
 if(selected<0||selected>=tracks.size())return;
 const int start=selected>=500?selected:0;
 m_source=tracks.mid(start,500);selected-=start;
 m_pool.clear();m_totals.clear();m_selected=selected;m_random=random;
 m_discovery=discovery;m_album=album;m_seed=QRandomGenerator::global()->generate();
 m_busy=true;const int serial=m_generation;
 m_status=discovery?"Preparando descoberta 2+1… a música atual continua tocando.":"Preparando ordem da reprodução…";
 emit changed();
 if(!discovery){finish(serial);return;}
 if((random?m_source.size():m_source.size()-selected)<2){
   m_failure="A Descoberta 2+1 precisa de pelo menos duas músicas na fila. Abra um álbum ou uma playlist e reproduza a seleção com a Descoberta ativada.";
   finish(serial);return;
 }
 // This work is asynchronous and never blocks the audio thread. Allow catalog
 // latency without abandoning all results after the first slow request.
 QTimer::singleShot(15000,this,[this,serial]{
   if(serial!=m_generation||!m_busy)return;
   m_failure="A consulta de sugestões demorou demais. Tente novamente.";
   finish(serial);
 });
 resolveGenres(serial,m_source[selected].toMap());
}
void ListeningMix::resolveGenres(int serial, QVariantMap anchor) {
 if(serial!=m_generation||!m_busy)return;
 const auto genre=genres(anchor);
 if(!genre.isEmpty()){startSearch(serial,anchor,genre);return;}
 const auto artistId=anchor["artistId"].toString();
 if(artistId.size()==22){
   m_api->raw("GET","/artists/"+artistId,{}, {}, {},
     [this,serial,anchor](int status,const QJsonDocument &doc){
       if(serial!=m_generation||!m_busy)return;
       if(status==401||status==403||status==429){m_failure=QString("Não foi possível consultar sugestões (Spotify HTTP %1). Tente novamente mais tarde.").arg(status);finish(serial);return;}
       QStringList tags;
       if(status==200)for(const auto &v:doc.object()["genres"].toArray())tags<<v.toString();
       startSearch(serial,anchor,tags.join(','));
     },[this,serial]{return serial==m_generation&&m_busy;});
   return;
 }
 const auto uri=anchor["uri"].toString();
 if(uri.startsWith("spotify:track:")&&uri.section(':',-1).size()==22){
   m_api->raw("GET","/tracks/"+uri.section(':',-1),{}, {}, {},
     [this,serial,anchor](int status,const QJsonDocument &doc)mutable{
       if(serial!=m_generation||!m_busy)return;
       const auto artists=doc.object()["artists"].toArray();
       if(status==200&&!artists.isEmpty()){
         anchor["artistId"]=artists.first().toObject()["id"].toString();
         if(anchor["artistId"].toString().size()==22){resolveGenres(serial,anchor);return;}
       }
       startSearch(serial,anchor,{});
     },[this,serial]{return serial==m_generation&&m_busy;});
   return;
 }
 startSearch(serial,anchor,{});
}
void ListeningMix::startSearch(int serial,const QVariantMap &anchor,const QString &genre){
 if(serial!=m_generation||!m_busy)return;
 for(int i=0;i<m_source.size();++i){auto t=m_source[i].toMap();
   if(t["artist"]==anchor["artist"]&&genres(t).isEmpty()&&!genre.isEmpty()){
     t["genre"]=genre;m_source[i]=t;
   }
 }
 // Spotify may omit artist genres. Use verified styles from the selection as
 // additional anchors; never label a song with an invented genre.
 QStringList tags;
 if(!genre.isEmpty())tags<<genre;
 for(const auto &row:m_source){auto g=genres(row.toMap());if(!g.isEmpty()&&!tags.contains(g))tags<<g;if(tags.size()>=3)break;}
 m_queries.clear();
 for(const auto &q:tonal::mix::queries(tags.join(',').toStdString(),anchor["year"].toInt()))
   if(!m_queries.contains(QString::fromStdString(q)))m_queries<<QString::fromStdString(q);
 if(m_queries.isEmpty()){
   m_failure="Ainda não há estilos identificados nesta seleção para buscar músicas parecidas. Abra as estatísticas para atualizar os metadados ou escolha outra seleção.";
   finish(serial);return;
 }
 search(serial,0);
}
void ListeningMix::search(int serial,int index){if(serial!=m_generation||!m_busy)return;if(index>=m_queries.size()*2||m_pool.size()>=120){finish(serial);return;}if(index%2 && m_totals.value(m_queries[index/2],0)<=10){search(serial,index+1);return;}const QString q=m_queries[index/2],genre=q.section('"',1,1);m_api->raw("GET","/search",{{"q",q},{"type","track"},{"limit",10},{"offset",index%2?int(QRandomGenerator::global()->bounded(std::max(1,(std::min(1000,m_totals.value(q,100))-10)/10+1))*10):0}},{},{},[this,serial,index,genre,q](int status,const QJsonDocument&doc){if(serial!=m_generation||!m_busy)return;if(status==429||status==401||status==403){m_failure=QString("Sugestões indisponíveis: Spotify HTTP %1. Tente novamente mais tarde.").arg(status);finish(serial);return;}if(status!=200)m_failure=QString("Falha na consulta de sugestões (Spotify HTTP %1). Tente novamente.").arg(status);if(status==200)m_totals[q]=doc.object()["tracks"].toObject()["total"].toInt(100);if(status==200)for(auto v:doc.object()["tracks"].toObject()["items"].toArray()){auto t=v.toObject(),a=t["album"].toObject();if(!t["uri"].toString().startsWith("spotify:track:")||t["is_local"].toBool()||(t.contains("is_playable")&&!t["is_playable"].toBool()))continue;QStringList names;auto artists=t["artists"].toArray();for(auto av:artists)names<<av.toObject()["name"].toString();auto label=QString::fromStdString(tonal::playlist::normalize((names.join(" ")+" "+t["name"].toString()).toStdString()));if(label.contains("karaoke")||label.contains("tribute"))continue;auto images=a["images"].toArray();m_pool<<QVariantMap{{"provider","Spotify"},{"id",t["uri"].toString()},{"uri",t["uri"].toString()},{"title",t["name"].toString()},{"artist",names.join(", ")},{"artistId",artists.isEmpty()?QString():artists[0].toObject()["id"].toString()},{"album",a["name"].toString()},{"albumId",a["id"].toString()},{"genre",genre},{"year",a["release_date"].toString().left(4).toInt()},{"image",images.isEmpty()?QString():images[0].toObject()["url"].toString()},{"durationMs",t["duration_ms"].toInt()}};}QTimer::singleShot(160,this,[this,serial,index]{search(serial,index+1);});},[this,serial]{return m_busy&&serial==m_generation;});}
void ListeningMix::finish(int serial){if(serial!=m_generation||!m_busy)return;QSet<QString> heard,artists;auto all=m_music->m_data["tracks"].toObject();for(auto v:m_music->m_data["events"].toArray()){auto e=v.toObject();if(e["ms"].toDouble()<=0)continue;auto t=all[e["key"].toString()].toObject();heard.insert(t["uri"].toString());artists.insert(QString::fromStdString(tonal::playlist::normalize(t["artist"].toString().toStdString())));}
 auto fields=[&](const QVariantList&list){std::vector<tonal::mix::Track> out;for(auto v:list){auto t=v.toMap(),k=known(t);unsigned bits=0;int n=0;if(k["lyricsAnalyzed"].toBool())for(auto name:{"amor","saudade","perda","rebeldia","esperanca","celebracao","tensao","introspeccao"}){if(k["topics"].toMap()[name].toDouble()>=.35)bits|=1u<<n;++n;}auto artist=t["artist"].toString();out.push_back({t["uri"].toString().toStdString(),artist.toStdString(),t["albumId"].toString().toStdString(),genres(t).toStdString(),t["year"].toInt(),heard.contains(t["uri"].toString()),artists.contains(QString::fromStdString(tonal::playlist::normalize(artist.toStdString()))),bits});}return out;};QVariantList result;int outside=0;for(int i:tonal::mix::plan(fields(m_source),fields(m_pool),m_selected,m_random,m_discovery,m_seed,m_album)){auto track=(i>=0?m_source[i]:m_pool[-i-1]).toMap();if(i<0)track["mixExternal"]=true;result<<track;if(i<0)outside++;}m_busy=false;m_status=m_discovery?(outside?QString("Descoberta 2+1 • %1 músicas externas. Artistas inéditos priorizados no histórico deste aparelho.").arg(outside):(m_failure.isEmpty()?QString("Não encontrei músicas externas compatíveis nesta consulta. Tente sortear novamente."):m_failure)):m_random?"Reprodução aleatória":"Ordem da lista";m_pool.clear();m_source.clear();m_previewQueue=result;emit changed();if(!m_preview)emit ready(result);}
