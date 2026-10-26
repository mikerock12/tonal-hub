#include "LocalStories.h"
#include "stories/LyricEvidence.hpp"
#include "MusicCompanion.h"
#include "StoryReport.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QStorageInfo>
#include <QRegularExpression>
#include <limits>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
constexpr qint64 modelBytes=1280835840;
quint64 availableMemory(){
#ifdef Q_OS_WIN
 MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);return GlobalMemoryStatusEx(&memory)?memory.ullAvailPhys:0;
#else
 return std::numeric_limits<quint64>::max();
#endif
}
const auto sha=QByteArrayLiteral("aaf42c8b7c3cab2bf3d69c355048d4a0ee9973d48f16c731c0520ee914699223");
const auto modelUrl=QStringLiteral("https://huggingface.co/unsloth/Qwen3.5-2B-GGUF/resolve/f6d5376be1edb4d416d56da11e5397a961aca8ae/Qwen3.5-2B-Q4_K_M.gguf");
QString normalized(QString s){s=s.normalized(QString::NormalizationForm_KD).toCaseFolded();s.remove(QRegularExpression("[\\p{M}]"));s.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ");return s.simplified();}
QJsonObject read(const QString &path){QFile f(path);return f.size()<=65536&&f.open(QIODevice::ReadOnly)?QJsonDocument::fromJson(f.readAll()).object():QJsonObject{};}
bool write(const QString &path,const QJsonObject &o){QSaveFile f(path);auto b=QJsonDocument(o).toJson(QJsonDocument::Compact);return f.open(QIODevice::WriteOnly)&&f.write(b)==b.size()&&f.commit();}
}
LocalStoriesWorker::LocalStoriesWorker(const QString &root,QObject *parent):QObject(parent),m_root(root){
 m_net.setParent(this);m_deadline.setParent(this);m_pressure.setParent(this);m_notify.setParent(this);
 m_notify.setSingleShot(true);m_notify.setInterval(200);connect(&m_notify,&QTimer::timeout,this,[this]{emit snapshot(state());});connect(this,&LocalStoriesWorker::changed,this,[this]{if(!m_notify.isActive())m_notify.start();});
 m_deadline.setSingleShot(true);connect(&m_deadline,&QTimer::timeout,this,[this]{++m_serial;if(m_process)m_process->kill();finish("A análise excedeu o limite. Tente novamente com menos músicas.");});
 m_pressure.setInterval(1000);connect(&m_pressure,&QTimer::timeout,this,[this]{if(m_process&&availableMemory()<256ULL*1024*1024){cancel();finish("Análise interrompida para preservar memória e reprodução. Tente novamente mais tarde.");}});
}
void LocalStoriesWorker::initialize(){
 QDir().mkpath(m_root+"/lyrics");QDir().mkpath(m_root+"/summaries");
 m_ready=QFileInfo(modelPath()).size()==modelBytes&&read(m_root+"/verified.json")["sha256"].toString().toLatin1()==sha;
 if(!m_ready&&QFileInfo(m_root+"/qwen2.5-0.5b-instruct-q4_k_m.gguf").size()>0)m_status="O leitor foi atualizado. Baixe o novo modelo de 1,19 GiB; o anterior será removido após a verificação.";
 emit changed();
}
void LocalStoriesWorker::setPlaying(bool playing){
 if(m_playing==playing)return;m_playing=playing;
 if(playing&&m_downloading){cancel();m_resumeDownload=true;m_downloading=true;m_status="Download aguardando uma pausa na reprodução.";emit changed();}
 else if(playing&&m_process){++m_serial;m_deadline.stop();m_pressure.stop();auto process=m_process;m_process=nullptr;process->kill();process->deleteLater();m_busy=true;m_status="Análise suspensa: a reprodução tem prioridade.";const auto token=m_serial;emit changed();QTimer::singleShot(3000,this,[this,token]{run(token);});}
 else if(playing&&m_reply){m_reply->abort();}
 else if(!playing&&m_resumeDownload){m_resumeDownload=false;m_downloading=false;download();}
}
LocalStoriesWorker::~LocalStoriesWorker(){cancel();}
QString LocalStoriesWorker::modelPath() const{return m_root+"/Qwen3.5-2B-Q4_K_M.gguf";}
QVariantMap LocalStoriesWorker::state() const{
 qint64 cache=0;for(auto sub:{"lyrics","summaries"})for(auto f:QDir(m_root+"/"+sub).entryInfoList(QDir::Files))cache+=f.size();
 return {{"ready",m_ready},{"busy",m_busy},{"downloading",m_downloading},{"status",m_status},{"summary",m_summary},{"sources",m_sources},{"items",m_items.toVariantList()},{"partial",!m_summary.isEmpty()&&!m_complete},{"count",m_texts.size()},{"progress",double(m_downloaded)/modelBytes},{"legacyBytes",QFileInfo(m_root+"/qwen2.5-0.5b-instruct-q4_k_m.gguf").size()},{"modelBytes",QFileInfo(modelPath()).size()+qMax(qint64(0),QFileInfo(m_root+"/qwen2.5-0.5b-instruct-q4_k_m.gguf").size())},{"downloadBytes",double(modelBytes)},{"cacheBytes",double(cache)},{"engine","Qwen3.5 2B Q4_K_M • Apache 2.0 • llama.cpp MIT"}};
}
QString LocalStoriesWorker::fileFor(const QVariantMap &t) const{return m_root+"/lyrics/"+QString::fromLatin1(QCryptographicHash::hash((MusicCompanion::key(t)+"|"+t.value("title").toString()+"|"+t.value("artist").toString()+"|"+t.value("durationMs").toString()).toUtf8(),QCryptographicHash::Sha256).toHex())+".json";}
void LocalStoriesWorker::capture(const QVariantMap &track,const QString &text){if(text.trimmed().size()<20)return;QString bounded=text.left(8000);while(bounded.toUtf8().size()>16000)bounded.chop(100);write(fileFor(track),{{"text",bounded}});trimCache();}
void LocalStoriesWorker::trimCache(){for(auto pair:{qMakePair(QString("lyrics"),128),qMakePair(QString("summaries"),20)}){auto dir=QDir(m_root+"/"+pair.first);auto files=dir.entryInfoList({"*.json"},QDir::Files,QDir::Time);for(int i=pair.second;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());}}
void LocalStoriesWorker::finish(const QString &message){m_pressure.stop();m_busy=false;m_status=message;emit changed();}
void LocalStoriesWorker::cancel(){m_resumeDownload=false;m_pressure.stop();++m_serial;m_busy=false;m_downloading=false;if(m_reply){m_reply->abort();m_reply=nullptr;}m_deadline.stop();if(m_process){m_process->kill();m_process->deleteLater();m_process=nullptr;}m_file.reset();m_hash.reset();m_status="Operação cancelada. A reprodução continua normalmente.";emit changed();}
void LocalStoriesWorker::removeModel(){cancel();QFile::remove(m_root+"/qwen2.5-0.5b-instruct-q4_k_m.gguf");if(QFile::remove(modelPath())||!QFileInfo::exists(modelPath())){QFile::remove(m_root+"/verified.json");m_ready=false;m_status="Modelo removido do aparelho.";}emit changed();}
void LocalStoriesWorker::download(){
 if(m_busy||m_downloading||m_ready)return;if(m_playing){m_resumeDownload=true;m_downloading=true;m_status="Download aguardando uma pausa na reprodução.";emit changed();return;}if(QStorageInfo(m_root).bytesAvailable()<modelBytes+128*1024*1024){m_status="Libere pelo menos 1,4 GiB para baixar o modelo.";emit changed();return;}
 m_file=std::make_unique<QSaveFile>(modelPath());if(!m_file->open(QIODevice::WriteOnly)){finish("Não foi possível criar o arquivo do modelo.");return;}
 m_hash=std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256);m_downloading=true;m_downloaded=0;const auto token=++m_serial;m_status="Baixando modelo opcional • 1,19 GiB";
 QNetworkRequest req{QUrl(modelUrl)};req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);req.setTransferTimeout(30000);auto reply=m_net.get(req);m_reply=reply;reply->setReadBufferSize(262144);
 connect(reply,&QIODevice::readyRead,this,[this,reply,token]{if(token!=m_serial)return;auto b=reply->readAll();m_downloaded+=b.size();if(m_downloaded>modelBytes||m_file->write(b)!=b.size()){reply->abort();return;}m_hash->addData(b);emit changed();});
 connect(reply,&QNetworkReply::finished,this,[this,reply,token]{reply->deleteLater();if(token!=m_serial)return;m_reply=nullptr;m_downloading=false;const bool verified=reply->error()==QNetworkReply::NoError&&m_downloaded==modelBytes&&m_hash->result().toHex()==sha;const bool ok=verified&&m_file->commit();m_file.reset();m_hash.reset();m_ready=ok;if(ok){write(m_root+"/verified.json",{{"sha256",QString::fromLatin1(sha)}});QFile::remove(m_root+"/qwen2.5-0.5b-instruct-q4_k_m.gguf");}m_status=ok?"Modelo verificado. Toque em Analisar letras para criar o resumo.":"Download incompleto ou inválido. Tente novamente; nenhum modelo foi ativado.";emit changed();});emit changed();
}
void LocalStoriesWorker::analyze(const QVariantList &tracks,int days){
 if(m_downloading)return;cancel();m_summary.clear();m_sources.clear();m_texts={};m_preparedTracks={};m_items={};m_tracks=tracks.mid(0,8);m_index=0;m_days=days;
 if(!m_ready){finish("Baixe o modelo opcional de 1,19 GiB para interpretar as letras localmente.");return;}if(m_tracks.isEmpty()){finish("Não há músicas ouvidas neste período.");return;}m_busy=true;m_status="Preparando até 8 letras do período selecionado…";emit changed();next(m_serial);
}
void LocalStoriesWorker::next(quint64 token){
 if(token!=m_serial||!m_busy)return;if(m_index>=m_tracks.size()){run(token);return;}auto t=m_tracks[m_index].toMap();auto cached=read(fileFor(t))["text"].toString();
 if(cached.isEmpty()&&m_playing){m_status="Busca de letras aguardando uma pausa; o áudio tem prioridade.";emit changed();QTimer::singleShot(3000,this,[this,token]{next(token);});return;}++m_index;
 auto accept=[this,t,token](const QString &text){if(token!=m_serial)return;if(text.trimmed().size()>=20){capture(t,text);m_preparedTracks.append(QJsonObject{{"title",t.value("title").toString()},{"artist",t.value("artist").toString()}});m_texts.append(QString::fromStdString(tonal::stories::boundedEvidence(text.toUtf8().toStdString())));if(!m_sources.isEmpty())m_sources+=" • ";m_sources+=t.value("title").toString()+" — "+t.value("artist").toString();}next(token);};
 if(!cached.isEmpty()){accept(cached);return;}
 if(QDateTime::currentMSecsSinceEpoch()<m_cooldown){next(token);return;}
 QTimer::singleShot(1250,this,[this,t,token,accept]{if(token!=m_serial||!m_busy)return;if(m_playing){--m_index;next(token);return;}QUrl u("https://lrclib.net/api/get");QUrlQuery query;query.addQueryItem("track_name",t.value("title").toString());query.addQueryItem("artist_name",t.value("artist").toString());if(!t.value("album").toString().isEmpty())query.addQueryItem("album_name",t.value("album").toString());if(t.value("durationMs").toInt()>0)query.addQueryItem("duration",QString::number(qRound(t.value("durationMs").toDouble()/1000.)));u.setQuery(query);QNetworkRequest req(u);req.setTransferTimeout(6000);req.setRawHeader("User-Agent","TonalHub/0.15.1 (local lyric analysis)");auto reply=m_net.get(req);m_reply=reply;QTimer::singleShot(6000,reply,[reply]{if(reply->isRunning())reply->abort();});auto body=std::make_shared<QByteArray>();connect(reply,&QIODevice::readyRead,this,[reply,body]{body->append(reply->readAll());if(body->size()>262144)reply->abort();});connect(reply,&QNetworkReply::finished,this,[this,reply,body,t,token,accept]{reply->deleteLater();if(token!=m_serial)return;m_reply=nullptr;int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();if(status==429||status==503)m_cooldown=QDateTime::currentMSecsSinceEpoch()+60000;QString text;auto o=QJsonDocument::fromJson(*body).object();if(status==200&&body->size()<=262144&&reply->error()==QNetworkReply::NoError&&normalized(o["trackName"].toString())==normalized(t.value("title").toString())&&normalized(o["artistName"].toString())==normalized(t.value("artist").toString())&&(t.value("durationMs").toInt()<=0||std::abs(o["duration"].toDouble()*1000-t.value("durationMs").toInt())<=5000)){text=o["plainLyrics"].toString();if(text.isEmpty())for(auto line:MusicCompanion::parseLyrics(o["syncedLyrics"].toString()))text+=line.toMap()["text"].toString()+"\n";}accept(text);});});
}
void LocalStoriesWorker::run(quint64 token){
 if(token!=m_serial)return;if(m_texts.isEmpty()){finish("Nenhuma letra correspondente foi encontrada neste lote. Abra ou importe letras e tente novamente.");return;}
 m_key=QString::fromLatin1(QCryptographicHash::hash(QByteArray("tonal-stories-v4|")+sha+QByteArray::number(m_days)+QJsonDocument(m_preparedTracks).toJson(QJsonDocument::Compact)+QJsonDocument(m_texts).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());auto rawOld=read(m_root+"/summaries/"+m_key+".json");auto old=personal::storyReport(rawOld,m_preparedTracks);m_known={};for(int n=0;n<m_texts.size();++n)m_known.append("");if(!old.isEmpty()){m_summary=old["summary"].toString();m_items=old["items"].toArray();m_complete=old["complete"].toBool();if(m_complete){finish("Leitura local em cache • "+QString::number(old["qualified"].toInt())+"/"+QString::number(m_texts.size())+" leituras disponíveis.");return;}for(auto value:rawOld["items"].toArray()){auto row=value.toObject();if(row["accepted"].toBool())m_known[row["track"].toInt()]=row["meaning"];else if(!row["pending"].toBool())m_known[row["track"].toInt()]="[TONAL_READING_INCONCLUSIVE_V4]";} }

 if(m_playing){m_status="Aguardando uma pausa na música para preservar o áudio.";emit changed();QTimer::singleShot(3000,this,[this,token]{run(token);});return;}
 if(availableMemory()<quint64(modelBytes)+512ULL*1024*1024){m_status="Aguardando memória livre para preservar a reprodução. Você pode cancelar.";emit changed();QTimer::singleShot(5000,this,[this,token]{run(token);});return;}
 m_pressure.start();m_status="Interpretando "+QString::number(m_texts.size())+" letras no aparelho…";auto process=new QProcess(this);m_process=process;process->setProgram(QCoreApplication::applicationDirPath()+"/TonalStories.exe");process->setArguments({modelPath()});process->setProcessChannelMode(QProcess::SeparateChannels);
 connect(process,&QProcess::started,this,[this,process,token]{if(token!=m_serial)return;process->write(QJsonDocument(QJsonObject{{"texts",m_texts},{"known",m_known},{"deadlineMs",90000}}).toJson(QJsonDocument::Compact));process->closeWriteChannel();});
 connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,token](int code,QProcess::ExitStatus status){process->deleteLater();if(token!=m_serial||!m_busy)return;m_process=nullptr;m_deadline.stop();auto result=QJsonDocument::fromJson(process->readAllStandardOutput().left(16384)).object();auto checked=personal::storyReport(result,m_preparedTracks);if(code||status!=QProcess::NormalExit||checked.isEmpty()){finish(result["error"].toString("O modelo não produziu uma leitura específica válida. Tente novamente."));return;}m_summary=checked["summary"].toString();m_items=checked["items"].toArray();m_complete=checked["complete"].toBool();write(m_root+"/summaries/"+m_key+".json",result);trimCache();finish("Leitura experimental no aparelho • "+QString::number(checked["qualified"].toInt())+"/"+QString::number(m_texts.size())+" leituras disponíveis."+(m_complete?QString():QString(" Continue para ler o restante.")));});
 connect(process,&QProcess::errorOccurred,this,[this,process,token](auto error){if(error==QProcess::FailedToStart){process->deleteLater();if(token==m_serial&&m_busy){m_process=nullptr;m_deadline.stop();finish("O componente TonalStories não está instalado. Atualize o instalador.");}}});process->start();m_deadline.start(105000);emit changed();
}

LocalStories::LocalStories(const QString &root,QObject *parent):QObject(parent),m_worker(new LocalStoriesWorker(root)){
 m_worker->moveToThread(&m_thread);
 connect(&m_thread,&QThread::finished,m_worker,&QObject::deleteLater);
 connect(m_worker,&LocalStoriesWorker::snapshot,this,[this](const QVariantMap &state){m_state=state;emit changed();});
 m_captureTimer.setSingleShot(true);connect(&m_captureTimer,&QTimer::timeout,this,&LocalStories::flushCaptures);
 m_thread.start(QThread::LowestPriority);
 QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->initialize();},Qt::QueuedConnection);
}
LocalStories::~LocalStories(){
 m_captureTimer.stop();
 QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->cancel();},Qt::BlockingQueuedConnection);
 m_thread.quit();m_thread.wait();
}
void LocalStories::capture(const QVariantMap &track,const QString &text){
 if(text.size()<20)return;
 auto key=track.value("provider").toString()+"|"+track.value("id").toString()+"|"+track.value("title").toString()+"|"+track.value("artist").toString();
 m_pending[key]={track,text.left(8000)};
 while(m_pending.size()>128)m_pending.erase(m_pending.begin());
 if(!m_captureTimer.isActive())m_captureTimer.start(25);
}
void LocalStories::flushCaptures(){
 if(m_pending.isEmpty())return;auto pending=std::move(m_pending);m_pending.clear();
 QMetaObject::invokeMethod(m_worker,[worker=m_worker,pending=std::move(pending)]{for(const auto &pair:pending)worker->capture(pair.first,pair.second);},Qt::QueuedConnection);
}
void LocalStories::analyze(const QVariantList &tracks,int days){flushCaptures();auto selected=tracks.mid(0,8);QMetaObject::invokeMethod(m_worker,[worker=m_worker,selected,days]{worker->analyze(selected,days);},Qt::QueuedConnection);}
void LocalStories::selectPeriod(int days){QMetaObject::invokeMethod(m_worker,[worker=m_worker,days]{worker->selectPeriod(days);},Qt::QueuedConnection);}
void LocalStories::resetSummary(){QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->resetSummary();},Qt::QueuedConnection);}
void LocalStories::setPlaying(bool playing){if(m_playing==playing)return;m_playing=playing;QMetaObject::invokeMethod(m_worker,[worker=m_worker,playing]{worker->setPlaying(playing);},Qt::QueuedConnection);}
void LocalStories::download(){QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->download();},Qt::QueuedConnection);}
void LocalStories::cancel(){QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->cancel();},Qt::QueuedConnection);}
void LocalStories::removeModel(){QMetaObject::invokeMethod(m_worker,[worker=m_worker]{worker->removeModel();},Qt::QueuedConnection);}
