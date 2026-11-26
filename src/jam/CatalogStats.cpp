#include "CatalogStats.h"
#include "LibrespotProcess.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <cmath>

namespace {
constexpr qint64 kTtlMs=6LL*3600*1000;
constexpr int kBatch=8,kMaxTracks=6000,kMaxArtists=48;
bool spotifyId(const QString &id){static const QRegularExpression re("^[A-Za-z0-9]{22}$");return re.match(id).hasMatch();}
}

CatalogStats::CatalogStats(LibrespotProcess *engine,QObject *parent):QObject(parent),m_engine(engine){
    m_process.setProcessChannelMode(QProcess::SeparateChannels);
    m_timeout.setSingleShot(true);m_timeout.setInterval(45000);
    m_idle.setSingleShot(true);m_idle.setInterval(60000);
    m_retry.setSingleShot(true);
    connect(&m_idle,&QTimer::timeout,this,[this]{if(!m_busy)m_process.kill();});
    connect(&m_retry,&QTimer::timeout,this,&CatalogStats::pump);
    connect(&m_timeout,&QTimer::timeout,this,[this]{m_process.kill();finish({{"status",408}});});
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_process.readAllStandardError();});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{
        m_buffer+=m_process.readAllStandardOutput();
        if(m_buffer.size()>1200*1024){m_buffer.clear();m_process.kill();return;}
        int end;while((end=m_buffer.indexOf('\n'))>=0){const auto bytes=m_buffer.left(end);m_buffer.remove(0,end+1);
            const auto value=QJsonDocument::fromJson(bytes);if(value.isObject())finish(value.object());}
    });
    connect(&m_process,&QProcess::finished,this,[this](int,QProcess::ExitStatus){m_buffer.clear();if(m_busy)finish({{"status",503}});});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart&&m_busy)finish({{"status",503}});});
}
CatalogStats::~CatalogStats(){m_process.kill();m_process.waitForFinished(1000);}

bool CatalogStats::albumKnown(const QString &id)const{const auto at=m_albumsAt.value(id,0);return at>0&&QDateTime::currentMSecsSinceEpoch()-at<kTtlMs;}

void CatalogStats::artistStats(const QString &id){
    if(!spotifyId(id))return;
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(m_artistCache.contains(id)&&now-m_artistAt.value(id)<kTtlMs){const auto cached=m_artistCache.value(id);QTimer::singleShot(0,this,[this,id,cached]{emit artistStatsReady(id,cached);});return;}
    if(!m_artistQueue.contains(id)&&!(m_op=="artistStats"&&m_artist==id))m_artistQueue.prepend(id);
    pump();
}

void CatalogStats::requestAlbums(const QStringList &ids){
    bool added=false;
    for(const auto &id:ids){if(!spotifyId(id)||albumKnown(id)||m_queued.contains(id))continue;m_queued.insert(id);m_albumQueue<<id;added=true;}
    if(added)pump();
}

void CatalogStats::pump(){
    if(m_busy)return;
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(now<m_blockedUntil){m_retry.start(int(m_blockedUntil-now));return;}
    if(m_artistQueue.isEmpty()&&m_albumQueue.isEmpty()){m_idle.start();return;}
    if(!m_engine||!m_engine->nativeLoginAvailable()){
        // Without the native login no public statistic exists: unknown, not zero.
        for(const auto &a:m_artistQueue)emit artistStatsFailed(a,401);
        m_artistQueue.clear();for(const auto &a:m_albumQueue)m_queued.remove(a);m_albumQueue.clear();emit playsChanged();return;
    }
    QJsonObject command;
    if(!m_artistQueue.isEmpty()){m_artist=m_artistQueue.takeFirst();m_op="artistStats";command={{"op","artistStats"},{"uri","spotify:artist:"+m_artist}};}
    else{m_inFlight.clear();QJsonArray albums;while(m_inFlight.size()<kBatch&&!m_albumQueue.isEmpty()){const auto id=m_albumQueue.takeFirst();m_inFlight<<id;albums.append("spotify:album:"+id);}
        m_op="albumPlays";command={{"op","albumPlays"},{"albums",albums}};}
    m_busy=true;m_idle.stop();m_timeout.start();
    const auto line=QJsonDocument(command).toJson(QJsonDocument::Compact)+'\n';
    if(m_process.state()==QProcess::Running)m_process.write(line);
    else{m_buffer.clear();m_process.setProgram(QCoreApplication::applicationDirPath()+"/TonalJam.exe");m_process.setArguments({m_engine->sessionCachePath()});
        connect(&m_process,&QProcess::started,this,[this,line]{m_process.write(line);},Qt::SingleShotConnection);m_process.start();}
}

void CatalogStats::finish(const QJsonObject &reply){
    if(!m_busy)return;
    m_busy=false;m_timeout.stop();
    const int code=reply.value("status").toInt();const auto result=reply.value("result").toObject();const auto now=QDateTime::currentMSecsSinceEpoch();
    if(code==429||code==503)m_blockedUntil=now+60000;
    const auto remember=[this](const QString &uri,qint64 plays){if(!m_plays.contains(uri))m_playsOrder<<uri;m_plays.insert(uri,plays);
        while(m_playsOrder.size()>kMaxTracks)m_plays.remove(m_playsOrder.takeFirst());};
    if(m_op=="artistStats"){
        const auto id=m_artist;m_op.clear();
        if(code>=200&&code<300){
            const auto map=result.toVariantMap();
            if(m_artistCache.size()>=kMaxArtists){const auto first=m_artistCache.keys().constFirst();m_artistCache.remove(first);m_artistAt.remove(first);}
            m_artistCache.insert(id,map);m_artistAt.insert(id,now);
            const auto counts=result.value("playCounts").toObject();for(auto it=counts.begin();it!=counts.end();++it)remember(it.key(),it.value().toInteger());
            if(!counts.isEmpty()){++m_revision;emit playsChanged();}
            emit artistStatsReady(id,map);
        }else emit artistStatsFailed(id,code);
    }else{
        m_op.clear();QSet<QString> done;
        if(code>=200&&code<300){
            const auto plays=result.value("plays").toObject();for(auto it=plays.begin();it!=plays.end();++it)remember(it.key(),it.value().toInteger());
            for(const auto &v:result.value("albums").toArray()){const auto uri=v.toString();if(uri.startsWith("spotify:album:"))done.insert(uri.mid(14));}
        }
        // Failed albums stay unknown; only a later screen request queues them again.
        for(const auto &album:m_inFlight){m_queued.remove(album);if(done.contains(album))m_albumsAt.insert(album,now);}
        m_inFlight.clear();++m_revision;emit playsChanged();
    }
    pump();
}

QString CatalogStats::compact(double value){
    if(value<0)return {};
    if(value<1000)return QString::number(qint64(value));
    static const char *units[]={"K","M","B","T"};
    double v=value;int unit=-1;while(v>=1000&&unit<3){v/=1000;++unit;}
    double rounded=std::round(v*10)/10.0;
    if(rounded>=1000&&unit<3){rounded=std::round(rounded/100)/10.0;++unit;}
    QString text=rounded==std::floor(rounded)?QString::number(qint64(rounded)):QString::number(rounded,'f',1).replace('.',',');
    return text+QString::fromLatin1(units[unit]);
}
