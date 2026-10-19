#include "DiscoveryLyrics.hpp"
#include "insights/TinyMusicModel.h"
#include "NetworkDeadline.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace { constexpr int MaxEntries=2000,MaxPending=144; }
QString DiscoveryLyrics::normalize(const QString &s){
    auto n=s.normalized(QString::NormalizationForm_D).toLower();
    n.remove(QRegularExpression("[\\x{0300}-\\x{036f}]"));
    return n.replace(QRegularExpression("[^\\p{L}\\p{N}]+")," ").simplified();
}
QString DiscoveryLyrics::signature(const QVariantMap &t){
    const auto values=QJsonArray{t.value("uri").toString(),t.value("title").toString(),
        t.value("artist").toString(),t.value("album").toString(),t.value("durationMs").toInt()};
    return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(values).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
}
bool DiscoveryLyrics::matches(const QVariantMap &t,const QJsonObject &o){
    return normalize(t.value("title").toString())==normalize(o["trackName"].toString()) &&
        normalize(t.value("artist").toString())==normalize(o["artistName"].toString()) &&
        o["duration"].isDouble() && std::isfinite(o["duration"].toDouble()) &&
        std::abs(t.value("durationMs").toDouble()/1000.-o["duration"].toDouble())<=5.;
}
DiscoveryLyrics::DiscoveryLyrics(QString path,QObject *disk,QObject *parent,Fetch fetcher)
    :QObject(parent),m_path(std::move(path)),m_disk(disk),m_fetch(std::move(fetcher)){
    QFile f(m_path);
    if(f.open(QIODevice::ReadOnly)&&f.size()<=2*1024*1024){
        auto root=QJsonDocument::fromJson(f.readAll()).object();
        if(root["version"].toInt()==1){m_enabled=root["enabled"].toBool(true);auto cache=root["entries"].toObject();
            for(auto i=cache.begin();i!=cache.end()&&m_cache.size()<MaxEntries;++i)
                if(i.key().size()==64&&i.value().isObject())m_cache[i.key()]=i.value();
        }
    }
    m_timer.setSingleShot(true);connect(&m_timer,&QTimer::timeout,this,[this]{next();});
}
DiscoveryLyrics::~DiscoveryLyrics(){++m_serial;if(m_reply)m_reply->abort();}
QVariantMap DiscoveryLyrics::lookup(const QVariantMap &t)const{
    const auto entry=m_cache[signature(t)].toObject();
    if(entry["until"].toDouble()<=QDateTime::currentMSecsSinceEpoch()||!entry["analyzed"].toBool())return {};
    QVariantMap topics;auto scores=entry["topics"].toObject();
    for(auto label:tonal::muse::labels()){double v=scores[label].toDouble(-1);if(std::isfinite(v)&&v>=0&&v<=1)topics[label]=v;}
    return {{"lyricsAnalyzed",true},{"topics",topics}};
}
void DiscoveryLyrics::setEnabled(bool enabled){
    if(m_enabled==enabled)return;m_enabled=enabled;
    if(!enabled){++m_serial;m_timer.stop();m_waiting.clear();m_active.clear();if(m_reply)m_reply->abort();}
    persist();if(changed)changed();
}
QString DiscoveryLyrics::status()const{
    if(!m_enabled)return "Análise automática de letras desativada";
    return QString("Letras da descoberta • %1 consultadas • %2 analisadas • %3 pendentes")
        .arg(m_consulted).arg(m_analyzed).arg(m_waiting.size()+(m_active.isEmpty()?0:1));
}
void DiscoveryLyrics::enqueue(const QVariantList &references,const QVariantList &candidates){
    if(!m_enabled)return;
    // Latest selection has priority; abandoning a search does not leave an
    // unbounded backlog. A currently active request may finish and populate cache.
    m_waiting.clear();QSet<QString> seen;const auto all=references.mid(0,12)+candidates.mid(0,120);
    static const QRegularExpression uri("^spotify:track:[A-Za-z0-9]{22}$");
    for(const auto &v:all){auto t=v.toMap();auto k=signature(t);
        if(m_waiting.size()>=MaxPending)break;
        if(!uri.match(t["uri"].toString()).hasMatch()||t["title"].toString().trimmed().isEmpty()||
           t["artist"].toString().trimmed().isEmpty()||t["durationMs"].toInt()<=0||
           t["durationMs"].toInt()>3600000||k==m_active||seen.contains(k))continue;
        seen.insert(k);if(m_cache[k].toObject()["until"].toDouble()>QDateTime::currentMSecsSinceEpoch())continue;
        m_waiting<<t;
    }
    if(changed)changed();if(m_active.isEmpty()&&!m_timer.isActive())m_timer.start(1250);
}
void DiscoveryLyrics::next(){
    if(!m_enabled||!m_active.isEmpty()||m_waiting.isEmpty())return;
    auto t=m_waiting.takeFirst().toMap();const auto key=signature(t);m_active=key;const auto serial=m_serial;
    QPointer<DiscoveryLyrics> self(this);
    Reply done=[this,self,key,t,serial](int code,const QJsonObject &o,int retryAfter){
        if(!self||serial!=m_serial)return;m_active.clear();++m_consulted;
        const bool found=code==200&&matches(t,o);
        QString text=found?o["plainLyrics"].toString():QString();
        if(found&&text.isEmpty())text=o["syncedLyrics"].toString().remove(QRegularExpression("\\[[^\\]]*\\]"));
        QJsonObject entry{{"analyzed",found&&!text.trimmed().isEmpty()},
            {"until",double(QDateTime::currentMSecsSinceEpoch()+
              (found?30LL*86400000:(code==404?86400000LL:900000LL)))}};
        if(found&&!text.trimmed().isEmpty()){
            auto scores=tonal::muse::analyze(normalize(text.left(12000)).toStdString());QJsonObject topics;
            for(int i=0;i<tonal::muse::labelsCount;++i)topics[tonal::muse::labels()[i]]=double(scores[i]);
            entry["topics"]=topics;++m_analyzed;
        }
        m_cache[key]=entry;
        while(m_cache.size()>MaxEntries){auto oldest=m_cache.begin();for(auto i=m_cache.begin();i!=m_cache.end();++i)
            if(i.value().toObject()["until"].toDouble()<oldest.value().toObject()["until"].toDouble())oldest=i;
            m_cache.erase(oldest);}
        persist();if(changed)changed();
        // No immediate retry loop. Cache failures briefly and honor provider cooldown.
        m_timer.start(code==429||code==503?std::clamp(std::clamp(retryAfter,0,60)*1000,10000,60000):1250);
    };
    if(m_fetch)m_fetch(t,std::move(done));else fetch(t,std::move(done));
}
void DiscoveryLyrics::fetch(const QVariantMap &t,Reply done){
    QUrl url("https://lrclib.net/api/get");QUrlQuery q;
    q.addQueryItem("track_name",t["title"].toString().left(300));q.addQueryItem("artist_name",t["artist"].toString().left(300));
    q.addQueryItem("album_name",t["album"].toString().left(300));q.addQueryItem("duration",QString::number(qRound(t["durationMs"].toDouble()/1000.)));url.setQuery(q);
    QNetworkRequest request(url);request.setTransferTimeout(5000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::SameOriginRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,"TonalHub/0.12.0 (https://github.com/mikerock12/tonal-hub)");
    auto *reply=m_net.get(request);m_reply=reply;setReplyDeadline(reply,6000);
    connect(reply,&QNetworkReply::readyRead,reply,[reply]{if(reply->bytesAvailable()>256*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[reply,done]{
        const auto bytes=reply->readAll();const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const int retry=std::clamp(reply->rawHeader("Retry-After").toInt(),0,60);reply->deleteLater();
        done(bytes.size()>256*1024?0:code,QJsonDocument::fromJson(bytes).object(),retry);
    });
}
void DiscoveryLyrics::persist(){
    const auto path=m_path;const QJsonObject snapshot{{"version",1},{"enabled",m_enabled},{"entries",m_cache}};
    auto write=[path,snapshot]{QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);if(f.open(QIODevice::WriteOnly)){f.write(QJsonDocument(snapshot).toJson(QJsonDocument::Compact));f.commit();}};
    if(m_disk)QMetaObject::invokeMethod(m_disk,std::move(write),Qt::QueuedConnection);else write();
}
