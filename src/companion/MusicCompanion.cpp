#include "MusicCompanion.h"
#include "PersonalDatabase.h"
#include "insights/TinyMusicModel.h"
#include <QCoreApplication>
#include <QPointer>
#include "NetworkDeadline.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QUuid>
#include <QTimeZone>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
QString norm(QString s){s=s.normalized(QString::NormalizationForm_D).remove(QRegularExpression("[\\x{0300}-\\x{036f}]")).toLower();return s.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ").simplified();}
QJsonObject rules(){static auto r=[] {QFile f(":/companion/resources/music-rules.json");f.open(QIODevice::ReadOnly);return QJsonDocument::fromJson(f.readAll()).object();}();return r;}
QString shortText(QString s,int size=300){return s.trimmed().left(size);}
QUrl url(const QString &base,const QVariantMap &q){QUrl u(base);QUrlQuery query;for(auto it=q.begin();it!=q.end();++it)query.addQueryItem(it.key(),it.value().toString());u.setQuery(query);return u;}
}
MusicCompanion::MusicCompanion(QObject *parent,const QString &root):QObject(parent){
  QString folder=root.isEmpty()?QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/personal":root;QDir().mkpath(folder);m_path=folder+"/music-library.sqlite";
  m_data=personal::load(m_path);*m_written=m_data;
  if(m_data.isEmpty()){QFile f(folder+"/library-v1.json");if(f.open(QIODevice::ReadOnly)&&f.size()<=8*1024*1024)m_data=QJsonDocument::fromJson(f.readAll()).object();}
  m_disk=new QObject;m_disk->moveToThread(&m_diskThread);connect(&m_diskThread,&QThread::finished,m_disk,&QObject::deleteLater);m_diskThread.start();
  m_saveTimer.setSingleShot(true);m_saveTimer.setInterval(750);connect(&m_saveTimer,&QTimer::timeout,this,&MusicCompanion::flush);m_clock.start();
}
MusicCompanion::~MusicCompanion(){checkpoint();flush();QMetaObject::invokeMethod(m_disk,[]{},Qt::BlockingQueuedConnection);m_diskThread.quit();m_diskThread.wait();}
void MusicCompanion::save(){++m_revision;m_saveTimer.start();emit changed();}
void MusicCompanion::flush(){const auto data=m_data;const auto path=m_path;auto written=m_written;QMetaObject::invokeMethod(m_disk,[this,data,path,written]{if(!personal::save(path,data,*written))QMetaObject::invokeMethod(this,[this]{emit message("Não foi possível salvar a biblioteca SQLite. Confira o espaço em disco e faça um backup.");},Qt::QueuedConnection);},Qt::QueuedConnection);}
void MusicCompanion::persist(std::function<void(bool)> done){const auto data=m_data;const auto path=m_path;auto written=m_written;QPointer<MusicCompanion> self(this);QMetaObject::invokeMethod(m_disk,[self,data,path,written,done]{const bool ok=personal::save(path,data,*written);QMetaObject::invokeMethod(QCoreApplication::instance(),[self,done,ok]{if(self)done(ok);},Qt::QueuedConnection);},Qt::QueuedConnection);}

void MusicCompanion::rememberTracks(const QVariantList& values){auto all=m_data["tracks"].toObject();for(auto v:values){auto t=safeTrack(v.toMap());auto id=key(t);if(!all.contains(id)&&all.size()>=3000)continue;auto stored=all[id].toObject();for(auto it=t.cbegin();it!=t.cend();++it)if(!it.value().toString().isEmpty()||it.value().typeId()!=QMetaType::QString)stored[it.key()]=QJsonValue::fromVariant(it.value());all[id]=stored;}m_data["tracks"]=all;save();}
QString MusicCompanion::key(const QVariantMap &t){return t.value("provider",t.value("source")).toString()+"|"+t.value("server").toString()+"|"+t.value("account").toString()+"|"+t.value("id",t.value("uri")).toString();}
QVariantMap MusicCompanion::safeTrack(const QVariantMap &t){
  QVariantMap r;for(auto name:{"provider","source","id","uri","title","artist","album","genre","image","artistId","albumId","genreSource","metadataSource","year","durationMs","playable","server","account","localFile"})if(t.contains(name))r[name]=t[name];
  if(!r.contains("provider"))r["provider"]=r.value("source");
  if(!r.contains("title"))r["title"]=t.value("name");
  QUrl stream(t.value("stream").toString());if(stream.scheme()=="https"&&stream.userInfo().isEmpty()&&stream.query().isEmpty())r["stream"]=stream.toString();
  for(auto name:{"provider","id","uri","title","artist","album","genre"})if(r.contains(name))r[name]=shortText(r[name].toString(),name==QString("id")?1024:300);
  return r;
}
void MusicCompanion::selectTrack(const QVariantMap &input){
  auto t=safeTrack(input);if(t.value("title").toString().isEmpty())return;QString id=key(t);auto tracks=m_data["tracks"].toObject();auto old=tracks[id].toObject().toVariantMap();
  for(auto field:{"album","image","artistId","albumId","genre","genreSource","metadataSource"})if(t.value(field).toString().isEmpty()&&!old.value(field).toString().isEmpty())t[field]=old[field];
  for(auto name:{"tone","lyricsAnalyzed","topics","topicVersion"})if(old.contains(name))t[name]=old[name];
  bool different=id!=key(m_current);if(different){checkpoint();m_session=QUuid::createUuid().toString(QUuid::WithoutBraces);m_sessionMs=0;m_pendingMs=0;m_counted=false;m_position=0;m_playing=false;m_clock.restart();m_generation++;m_busy=false;m_lines.clear();m_lyricStatus="Busque a letra desta faixa • LRCLIB";emit lyricsChanged();}
  tracks[id]=QJsonObject::fromVariantMap(t);if(tracks.size()>3000){QSet<QString> keep;for(auto v:m_data["favorites"].toArray())keep.insert(v.toString());for(auto v:m_data["playlists"].toArray())for(auto k:v.toObject()["tracks"].toArray())keep.insert(k.toString());for(auto it=tracks.begin();it!=tracks.end()&&tracks.size()>3000;){if(it.key()!=id&&!keep.contains(it.key()))it=tracks.erase(it);else ++it;}}
  m_data["tracks"]=tracks;m_current=t;emit currentChanged();save();
}
void MusicCompanion::pulse(bool playing,int position){
  qint64 wall=m_clock.restart();if(historyEnabled()&&!m_current.isEmpty()&&playing&&m_playing){qint64 delta=position-m_position;if(delta>0&&delta<5000){qint64 ms=std::min(std::clamp(wall,qint64(0),qint64(2000)),delta);m_pendingMs+=ms;m_sessionMs+=ms;if(m_pendingMs>=5000)checkpoint();}}
  int duration=m_current.value("durationMs").toInt();if(duration>1000&&m_position>=duration-1500&&position<1000&&m_counted){checkpoint();m_session=QUuid::createUuid().toString();m_sessionMs=0;m_counted=false;}
  m_position=position;m_playing=playing;if(!playing)checkpoint();
}
void MusicCompanion::checkpoint(){if(m_pendingMs<=0||m_current.isEmpty())return;auto events=m_data["events"].toArray();QJsonObject e;int index=-1;for(int i=events.size()-1;i>=0&&i>=events.size()-8;--i)if(events[i].toObject()["session"]==m_session){index=i;e=events[i].toObject();break;}
  int duration=m_current.value("durationMs").toInt();qint64 threshold=duration>0?std::min(30000,std::max(1000,duration/2)):30000;
  e["session"]=m_session;e["key"]=key(m_current);e["at"]=e.contains("at")?e["at"]:QJsonValue(QDateTime::currentMSecsSinceEpoch());e["ms"]=e["ms"].toDouble()+m_pendingMs;e["play"]=m_sessionMs>=threshold;m_counted=m_counted||e["play"].toBool();if(index<0)events.append(e);else events[index]=e;
  qint64 cutoff=QDateTime::currentMSecsSinceEpoch()-180LL*86400000;while(!events.isEmpty()&&(events.size()>10000||events.first().toObject()["at"].toDouble()<cutoff))events.removeFirst();m_data["events"]=events;m_pendingMs=0;save();
}
bool MusicCompanion::liked(const QVariantMap &t) const{return m_data["favorites"].toArray().contains(key(t));}
QVariantList MusicCompanion::favorites() const{QVariantList r;auto tracks=m_data["tracks"].toObject();for(auto k:m_data["favorites"].toArray())if(tracks.contains(k.toString()))r<<tracks[k.toString()].toObject().toVariantMap();return r;}
QVariantList MusicCompanion::playlists() const{return m_data["playlists"].toArray().toVariantList();}
void MusicCompanion::favorite(const QVariantMap &t){bool on=!liked(t);if(t.value("provider").toString()=="Spotify"){emit spotifyFavoriteRequested(t,on);return;}setFavorite(t,on);}
void MusicCompanion::setFavorite(const QVariantMap &t,bool on){auto id=key(t);auto tracks=m_data["tracks"].toObject();auto saved=tracks[id].toObject();auto clean=QJsonObject::fromVariantMap(safeTrack(t));for(auto it=clean.begin();it!=clean.end();++it)if(!it.value().isString()||!it.value().toString().isEmpty())saved[it.key()]=it.value();tracks[id]=saved;m_data["tracks"]=tracks;auto list=m_data["favorites"].toArray();for(int i=list.size()-1;i>=0;--i)if(list[i]==id)list.removeAt(i);if(on)list.prepend(id);m_data["favorites"]=list;save();}
QString MusicCompanion::createPlaylist(const QString &name){if(name.trimmed().isEmpty()||playlists().size()>=100)return {};auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);auto list=m_data["playlists"].toArray();list.append(QJsonObject{{"id",id},{"name",shortText(name,100)},{"tracks",QJsonArray{}}});m_data["playlists"]=list;save();return id;}
QString MusicCompanion::importPlaylist(const QString &name,const QString &description,const QString &cover,const QVariantList &input){
  if(name.trimmed().isEmpty()||playlists().size()>=100||input.isEmpty()||input.size()>500||cover.size()>262200)return {};
  auto next=m_data;auto tracks=next["tracks"].toObject();QJsonArray entries;
  for(auto value:input){auto t=safeTrack(value.toMap());if(!t.value("uri").toString().startsWith("spotify:track:"))return {};auto k=key(t);tracks[k]=QJsonObject::fromVariantMap(t);entries.append(k);}
  if(tracks.size()>3000)return {};
  const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);auto lists=next["playlists"].toArray();
  lists.append(QJsonObject{{"id",id},{"name",name.trimmed().left(100)},{"description",description.left(300)},{"cover",cover},{"tracks",entries},{"importSource","Text to Tonalify"}});
  next["tracks"]=tracks;next["playlists"]=lists;if(QJsonDocument(next).toJson(QJsonDocument::Compact).size()>7*1024*1024)return {};
  m_data=next;save();flush();return id;
}
QVariantMap MusicCompanion::playlistDetails(const QString &id) const{for(auto v:m_data["playlists"].toArray())if(v.toObject()["id"]==id)return v.toObject().toVariantMap();return {};}
void MusicCompanion::updatePlaylistDetails(const QString &id,const QVariantMap &patch){auto lists=m_data["playlists"].toArray();for(int i=0;i<lists.size();++i){auto p=lists[i].toObject();if(p["id"]!=id)continue;for(auto key:{"spotifyId","syncedCount","cloudUncertain","spotifyPublic","cloudComplete","coverSynced"})if(patch.contains(key))p[key]=QJsonValue::fromVariant(patch[key]);lists[i]=p;}m_data["playlists"]=lists;save();flush();}
void MusicCompanion::renamePlaylist(const QString &id,const QString &name){if(name.trimmed().isEmpty())return;auto list=m_data["playlists"].toArray();for(int i=0;i<list.size();i++){auto p=list[i].toObject();if(p["id"]==id){p["name"]=shortText(name,100);list[i]=p;}}m_data["playlists"]=list;save();}
void MusicCompanion::deletePlaylist(const QString &id){auto list=m_data["playlists"].toArray();for(int i=list.size()-1;i>=0;i--)if(list[i].toObject()["id"]==id)list.removeAt(i);m_data["playlists"]=list;save();}
void MusicCompanion::addToPlaylist(const QString &id,const QVariantMap &t){if(t.isEmpty())return;auto tracks=m_data["tracks"].toObject();auto stored=tracks[key(t)].toObject();auto clean=QJsonObject::fromVariantMap(safeTrack(t));for(auto it=clean.begin();it!=clean.end();++it)stored[it.key()]=it.value();tracks[key(t)]=stored;m_data["tracks"]=tracks;auto list=m_data["playlists"].toArray();for(int i=0;i<list.size();++i){auto p=list[i].toObject();if(p["id"]!=id)continue;auto entries=p["tracks"].toArray();if(!entries.contains(key(t))&&entries.size()<1000)entries.append(key(t));p["tracks"]=entries;list[i]=p;}m_data["playlists"]=list;save();}
void MusicCompanion::removeFromPlaylist(const QString &id,int row){auto list=m_data["playlists"].toArray();for(int i=0;i<list.size();++i){auto p=list[i].toObject();if(p["id"]!=id)continue;auto entries=p["tracks"].toArray();if(row>=0&&row<entries.size())entries.removeAt(row);p["tracks"]=entries;list[i]=p;}m_data["playlists"]=list;save();}
QVariantList MusicCompanion::playlistTracks(const QString &id) const{auto tracks=m_data["tracks"].toObject();QVariantList r;for(auto value:m_data["playlists"].toArray()){auto p=value.toObject();if(p["id"]==id)for(auto k:p["tracks"].toArray())if(tracks.contains(k.toString()))r<<tracks[k.toString()].toObject().toVariantMap();}return r;}
void MusicCompanion::setHistoryEnabled(bool on){checkpoint();m_data["history"]=on;m_clock.restart();save();}
void MusicCompanion::clearHistory(){m_pendingMs=0;m_sessionMs=0;m_counted=false;m_data["events"]=QJsonArray();save();}
void MusicCompanion::setBackground(const QString &s){if(QStringList{"Aurora","Ondas","Espectro","Capa","Foto","Sem animação","Estrelas","Chuva","Orbitas","Pulso","Grade"}.contains(s)){m_data["background"]=s;save();}}
void MusicCompanion::setPhoto(const QString &s){QUrl u(s);if(u.isLocalFile()||u.scheme()=="https"){m_data["photo"]=s;save();}}
void MusicCompanion::setGenre(const QString &g){auto t=m_current;t["genre"]=shortText(g,80);selectTrack(t);updateTone();}
void MusicCompanion::editMetadata(const QString &title,const QString &artist){auto t=m_current;t["title"]=shortText(title);t["artist"]=shortText(artist);selectTrack(t);m_generation++;m_lines.clear();m_busy=false;emit lyricsChanged();}
QVariantMap MusicCompanion::tone(const QString &text,const QString &genre){QVariantMap out;auto defs=rules()["tones"].toObject();QString g=norm(genre);auto words=norm(text).split(' ',Qt::SkipEmptyParts);for(auto it=defs.begin();it!=defs.end();++it){auto d=it.value().toObject();QSet<QString> lex;for(auto v:d["words"].toArray())lex.insert(norm(v.toString()));int count=0;for(auto w:words)if(lex.contains(w))count++;double genrePoints=0;for(auto v:d["genres"].toArray())if(g.contains(norm(v.toString())))genrePoints=1;out[it.key()]=std::min(10.,double(count))+genrePoints;}return out;}
void MusicCompanion::updateTone(){if(m_current.isEmpty())return;QString text;for(auto v:m_lines)text+=v.toMap()["text"].toString()+"\n";auto t=m_current;t["tone"]=tone(text,t["genre"].toString());t["lyricsAnalyzed"]=!text.trimmed().isEmpty();if(!text.trimmed().isEmpty()){auto probabilities=tonal::muse::analyze(norm(text).toStdString());QVariantMap topics;for(int i=0;i<tonal::muse::labelsCount;++i)topics[tonal::muse::labels()[i]]=double(probabilities[i]);t["topics"]=topics;t["topicVersion"]=1;}else{t.remove("topics");t.remove("topicVersion");}auto tracks=m_data["tracks"].toObject();tracks[key(t)]=QJsonObject::fromVariantMap(t);m_data["tracks"]=tracks;m_current=t;emit currentChanged();save();}
QVariantList MusicCompanion::parseLyrics(const QString &text){QVariantList rows;QRegularExpression stamp("\\[(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?\\]");int offset=0;auto om=QRegularExpression("\\[offset:([+-]?\\d+)\\]",QRegularExpression::CaseInsensitiveOption).match(text);if(om.hasMatch())offset=om.captured(1).toInt();for(auto raw:text.left(100000).split('\n')){auto matches=stamp.globalMatch(raw);QList<int> times;int end=0;while(matches.hasNext()){auto m=matches.next();QString fraction=m.captured(3).leftJustified(3,'0');times<<std::max(0,m.captured(1).toInt()*60000+m.captured(2).toInt()*1000+fraction.toInt()+offset);end=m.capturedEnd();}QString line=raw.mid(end).trimmed();if(times.isEmpty()){if(!line.startsWith('['))rows<<QVariantMap{{"time",-1},{"text",line}};}else for(int time:times)rows<<QVariantMap{{"time",time},{"text",line}};if(rows.size()>=1000)break;}bool synced=false;for(auto r:rows)if(r.toMap()["time"].toInt()>=0)synced=true;if(synced){rows.erase(std::remove_if(rows.begin(),rows.end(),[](auto r){return r.toMap()["time"].toInt()<0;}),rows.end());std::stable_sort(rows.begin(),rows.end(),[](auto a,auto b){return a.toMap()["time"].toInt()<b.toMap()["time"].toInt();});}return rows;}
void MusicCompanion::get(const QUrl &u,std::function<void(int,const QJsonDocument &)> done){QNetworkRequest req(u);if(u.host()=="translate.googleapis.com")req.setAttribute(QNetworkRequest::Http2AllowedAttribute,false);req.setHeader(QNetworkRequest::UserAgentHeader,"TonalHub/0.8.0 (https://github.com/mikerock12/tonal-hub)");req.setTransferTimeout(12000);req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);auto *r=m_net.get(req);setReplyDeadline(r,20000);connect(r,&QNetworkReply::readyRead,r,[r]{if(r->bytesAvailable()>1024*1024)r->abort();});connect(r,&QNetworkReply::finished,this,[r,done]{auto bytes=r->readAll();int status=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();r->deleteLater();done(bytes.size()>1024*1024?0:status,QJsonDocument::fromJson(bytes));});}
void MusicCompanion::fetchLyrics(){if(m_current.isEmpty())return;QString k=key(m_current);if(k!=m_lyricRetryKey){m_lyricRetryKey=k;m_lyricRetryCount=0;}if(m_lyricCache.contains(k)){m_lines=m_lyricCache[k];m_lyricStatus="LRCLIB • letra em cache nesta sessão";updateTone();emit lyricsChanged();return;}int generation=++m_generation;m_busy=true;m_lines.clear();m_lyricStatus="Buscando letra no LRCLIB…";emit lyricsChanged();QVariantMap params{{"track_name",m_current["title"]},{"artist_name",m_current["artist"]}};if(!m_current["album"].toString().isEmpty())params["album_name"]=m_current["album"];int duration=m_current["durationMs"].toInt();if(duration>0&&duration<=3600000)params["duration"]=qRound(duration/1000.);get(url("https://lrclib.net/api/get",params),[this,generation,params,duration](int status,const QJsonDocument &d){if(generation!=m_generation)return;if(status==200){acceptLyrics(d.object());return;}if(retryLyrics(status,generation))return;if(status!=404&&!(duration<=0&&status==400)){m_busy=false;m_lyricStatus="Letra temporariamente indisponível. Tente novamente ou cole uma letra/LRC.";emit lyricsChanged();return;}get(url("https://lrclib.net/api/search",{{"track_name",params["track_name"]},{"artist_name",params["artist_name"]}}),[this,generation,params,duration](int code,const QJsonDocument &doc){if(generation!=m_generation)return;if(retryLyrics(code,generation))return;QJsonObject best;double score=1e9;for(auto value:doc.array()){auto o=value.toObject();if(norm(o["trackName"].toString())!=norm(params["track_name"].toString())||norm(o["artistName"].toString())!=norm(params["artist_name"].toString()))continue;double diff=duration>0?std::abs(o["duration"].toDouble()-duration/1000.):0;if(diff>5)continue;double s=diff+(o["syncedLyrics"].toString().isEmpty()?100:0);if(s<score){score=s;best=o;}}if(code==200&&!best.isEmpty())acceptLyrics(best);else {m_busy=false;m_lyricStatus=code==200?"Letra não encontrada para esta versão. Cole TXT/LRC para usá-la.":"Falha ao consultar letras. Tente novamente.";emit lyricsChanged();}});});}
bool MusicCompanion::retryLyrics(int status,int generation){
  if(!QList<int>{429,502,503,504}.contains(status)||m_lyricRetryCount>=2)return false;
  m_busy=false;m_lyricStatus="Serviço de letras ocupado • tentando novamente…";emit lyricsChanged();
  QTimer::singleShot(1500*(1<<m_lyricRetryCount++),this,[this,generation]{if(generation==m_generation)fetchLyrics();});return true;
}
void MusicCompanion::acceptLyrics(const QJsonObject &o){m_busy=false;QString synced=o["syncedLyrics"].toString(),plain=o["plainLyrics"].toString();m_lines=parseLyrics(synced.isEmpty()?plain:synced);m_lyricStatus=o["instrumental"].toBool()?"Faixa instrumental • LRCLIB":m_lines.isEmpty()?"Letra não disponível • LRCLIB":synced.isEmpty()?"LRCLIB • sem tempos; rolagem manual":"LRCLIB • letra sincronizada";if(m_lyricCache.size()>=30)m_lyricCache.erase(m_lyricCache.begin());m_lyricCache[key(m_current)]=m_lines;updateTone();emit lyricsChanged();}
void MusicCompanion::importLyrics(const QString &s,bool translation){if(translation){auto translated=s.left(100000).split('\n');if(translated.size()!=m_lines.size()){emit message("A tradução deve ter o mesmo número de linhas da letra.");return;}for(int i=0;i<m_lines.size();i++){auto line=m_lines[i].toMap();line["translation"]=translated[i];m_lines[i]=line;}}else {++m_generation;m_busy=false;m_lines=parseLyrics(s);m_lyricStatus="Letra fornecida por você";updateTone();}emit lyricsChanged();}
bool MusicCompanion::useDisplayedLyrics(const QString &trackUri,const QVariantList &lines){
  // Translate exactly the version already on screen, preserving empty verses and LRC times.
  if(trackUri.isEmpty()||trackUri!=m_current.value("uri").toString()||lines.isEmpty()||lines.size()>3000)return false;
  QVariantList clean;qsizetype size=0;bool same=lines.size()==m_lines.size();
  for(qsizetype i=0;i<lines.size();++i){
    const auto row=lines[i].toMap();const auto text=row.value("text").toString();size+=text.size();if(size>100000)return false;
    const auto time=row.value("time",-1).toInt();clean.append(QVariantMap{{"text",text},{"time",time}});
    const auto old=m_lines.value(i).toMap();same=same&&old.value("text").toString()==text&&old.value("time",-1).toInt()==time;
  }
  if(same)return true; // Keep completed translations and any request already translating these lines.
  ++m_generation;m_busy=false;m_lines=clean;m_lyricStatus="Letra exibida • LRCLIB";
  updateTone();emit lyricsChanged();return true;
}
void MusicCompanion::translate(const QString &lang){if(m_lines.isEmpty()||m_busy)return;if(lang=="pt"){m_lyricStatus="A letra já está em português.";emit lyricsChanged();return;}if(!QStringList{"auto","en","es","fr","de","it","ja","ko"}.contains(lang))return;m_language=lang;m_translationIndex=0;m_translationRetries=0;m_translationFailures=0;m_busy=true;m_lyricStatus="Traduzindo para pt-BR • Google Tradutor";int g=++m_generation;emit lyricsChanged();nextTranslation(g);}
void MusicCompanion::nextTranslation(int generation){
  if(generation!=m_generation)return;
  while(m_translationIndex<m_lines.size()){
    auto line=m_lines[m_translationIndex].toMap();QString original=line["text"].toString(),cache=m_language+"|"+original;
    if(original.isEmpty()||!line["translation"].toString().isEmpty()){m_translationIndex++;continue;}
    if(m_translationCache.contains(cache)){line["translation"]=m_translationCache[cache];m_lines[m_translationIndex++]=line;continue;}
    auto day=QDate::currentDate().toString(Qt::ISODate);if(m_data["translationDay"]!=day){m_data["translationDay"]=day;m_data["translationBytes"]=0;}
    int used=m_data["translationBytes"].toInt();if(used+original.toUtf8().size()>512*1024){m_busy=false;m_lyricStatus="Tradução parcial • proteção local diária de 512 KiB. Retome amanhã ou importe tradução.";emit lyricsChanged();return;}
    m_data["translationBytes"]=used+original.toUtf8().size();save();int row=m_translationIndex;
    m_lyricStatus=QString("Traduzindo pt-BR • linha %1/%2").arg(row+1).arg(m_lines.size());emit lyricsChanged();
    // The HTTPS client has a bounded URL/response. Long verses are translated as one row,
    // preserving its identity and avoiding partial text being mistaken for a complete verse.
    get(url("https://translate.googleapis.com/translate_a/single",{{"client","gtx"},{"sl",m_language},{"tl","pt-BR"},{"dt","t"},{"q",original}}),[this,generation,row,cache](int status,const QJsonDocument &doc){
      if(generation!=m_generation)return;QString text;const auto response=doc.array();const auto segments=response.isEmpty()?QJsonArray():response.at(0).toArray();for(auto part:segments){const auto segment=part.toArray();if(!segment.isEmpty())text+=segment.at(0).toString();}
      if(status!=200||text.isEmpty()){
        bool transient=status==0||status==408||status==429||status>=500;
        if(transient&&m_translationRetries<2){int delay=1500*(++m_translationRetries);QTimer::singleShot(delay,this,[this,generation]{nextTranslation(generation);});return;}
        m_translationFailures++;m_translationRetries=0;m_translationIndex=row+1;
      }else{if(m_translationCache.size()>2000)m_translationCache.clear();m_translationCache[cache]=text;auto line=m_lines[row].toMap();line["translation"]=text;m_lines[row]=line;m_translationIndex=row+1;m_translationRetries=0;}
      emit lyricsChanged();QTimer::singleShot(350,this,[this,generation]{nextTranslation(generation);});
    });return;
  }
  m_busy=false;int translated=0,eligible=0;for(auto v:m_lines)if(!v.toMap()["text"].toString().isEmpty()){eligible++;if(!v.toMap()["translation"].toString().isEmpty())translated++;}
  m_lyricStatus=QString("Tradução pt-BR • %1/%2 linhas • Google Tradutor%3").arg(translated).arg(eligible).arg(m_translationFailures?" • clique em traduzir para retomar pendências":" • pode conter erros");emit lyricsChanged();
}

namespace {
QString module(){return QCoreApplication::applicationName()=="IndieTonal"?"indie":"tonalify";}
}
bool MusicCompanion::exportBackup(const QUrl &target){
  checkpoint();if(!target.isLocalFile()||target.toLocalFile()==m_path)return false;
  QJsonObject doc{{"format","tonal-personal-backup"},{"version",1},{"module",module()},{"createdAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"data",m_data}};
  QSaveFile f(target.toLocalFile());auto bytes=QJsonDocument(doc).toJson();bool ok=f.open(QIODevice::WriteOnly)&&f.write(bytes)==bytes.size()&&f.commit();emit message(ok?"Backup salvo. Guarde este arquivo fora da pasta do aplicativo.":"Não foi possível salvar o backup.");return ok;
}
bool MusicCompanion::restoreBackup(const QUrl &source){
  QFile f(source.toLocalFile());if(!source.isLocalFile()||!f.open(QIODevice::ReadOnly)||f.size()>8*1024*1024){emit message("Backup inválido ou maior que 8 MiB.");return false;}
  auto doc=QJsonDocument::fromJson(f.readAll()).object();auto data=doc["data"].toObject();
  if(doc["format"]!="tonal-personal-backup"||doc["version"]!=1||doc["module"]!=module()||!data["tracks"].isObject()||data["tracks"].toObject().size()>3000||data["events"].toArray().size()>10000||data["playlists"].toArray().size()>100){emit message("Backup incompatível. Use um backup deste player (Tonalify ou IndieTonal).");return false;}
  QJsonObject clean,tracks;auto imported=data["tracks"].toObject();
  for(auto it=imported.begin();it!=imported.end();++it){auto raw=it.value().toObject();auto t=safeTrack(raw.toVariantMap());if(key(t)!=it.key()){emit message("Backup com identificação de faixa inválida.");return false;}auto row=QJsonObject::fromVariantMap(t);row["lyricsAnalyzed"]=raw["lyricsAnalyzed"].toBool();QJsonObject tone;auto scores=raw["tone"].toObject();for(auto i=scores.begin();i!=scores.end();++i)if(QStringList{"intenso","contemplativo","afetivo","festivo"}.contains(i.key()))tone[i.key()]=std::clamp(i.value().toDouble(),0.,11.);row["tone"]=tone;QJsonObject topics;for(auto label:tonal::muse::labels())if(raw["topics"].toObject().contains(label))topics[label]=std::clamp(raw["topics"].toObject()[label].toDouble(),0.,1.);row["topics"]=topics;row["topicVersion"]=1;tracks[it.key()]=row;}
  clean["tracks"]=tracks;QJsonArray favorites,playlists,events;
  for(auto k:data["favorites"].toArray())if(k.isString()&&tracks.contains(k.toString())&&favorites.size()<3000&&!favorites.contains(k))favorites.append(k);clean["favorites"]=favorites;
  for(auto value:data["playlists"].toArray()){auto raw=value.toObject();if(raw["id"].toString().isEmpty())return false;QJsonObject p{{"id",raw["id"].toString().left(100)},{"name",raw["name"].toString().left(100)}};QJsonArray entries;for(auto k:raw["tracks"].toArray())if(k.isString()&&tracks.contains(k.toString())&&entries.size()<1000)entries.append(k);p["tracks"]=entries;p["description"]=raw["description"].toString().left(300);auto cover=raw["cover"].toString();if(cover.startsWith("data:image/jpeg;base64,")&&cover.size()<=262200)p["cover"]=cover;for(auto k:{"spotifyId","syncedCount","cloudUncertain","spotifyPublic","cloudComplete","coverSynced","importSource"})if(raw.contains(k))p[k]=raw[k];playlists.append(p);}clean["playlists"]=playlists;
  for(auto value:data["events"].toArray()){auto raw=value.toObject();if(!tracks.contains(raw["key"].toString()))continue;events.append(QJsonObject{{"key",raw["key"].toString()},{"session",raw["session"].toString(QUuid::createUuid().toString())},{"at",raw["at"].toDouble()},{"ms",std::clamp(raw["ms"].toDouble(),0.,86400000.)},{"play",raw["play"].toBool()}});}clean["events"]=events;
  for(auto k:{"history","background","photo","translationDay","translationBytes","searches","lyricStyle","lyricSync"})if(data.contains(k))clean[k]=data[k];
  checkpoint();m_data=clean;m_pendingMs=m_sessionMs=0;m_session=QUuid::createUuid().toString();m_playing=false;m_clock.restart();m_generation++;m_busy=false;m_lines.clear();m_lyricCache.clear();m_translationCache.clear();save();emit lyricsChanged();emit message("Backup restaurado. Arquivos locais precisam existir neste aparelho; reconecte suas contas se necessário.");return true;
}
