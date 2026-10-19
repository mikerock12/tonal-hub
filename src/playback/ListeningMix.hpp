#pragma once
#include <QObject>
#include <QVariantList>
#include <QJsonObject>
#include <QHash>
class SpotifyApi;class MusicCompanion;class DiscoveryLyrics;
class ListeningMix:public QObject {
 Q_OBJECT
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(QVariantList previewQueue READ previewQueue NOTIFY changed)
 Q_PROPERTY(bool discoveryEnabled READ discoveryEnabled NOTIFY changed)
 Q_PROPERTY(bool randomEnabled READ randomEnabled NOTIFY changed)
 Q_PROPERTY(bool enrichmentEnabled READ enrichmentEnabled WRITE setEnrichmentEnabled NOTIFY changed)
 Q_PROPERTY(QString enrichmentStatus READ enrichmentStatus NOTIFY changed)
public:
 ListeningMix(SpotifyApi*,MusicCompanion*,QObject*parent=nullptr);
 bool busy()const{return m_busy;}QString status()const{return m_status;}
 Q_INVOKABLE QVariantList order(int count,quint32 seed)const;
 Q_INVOKABLE void play(const QVariantList&,int selected,bool random,bool discovery,bool album=false);
 Q_INVOKABLE void preview(const QVariantList&,int selected,bool random,bool discovery,bool album=false);
 Q_INVOKABLE void applyPreview(int position=0);
 QVariantList previewQueue()const{return m_previewQueue;}
 bool discoveryEnabled()const{return m_activeDiscovery;}
 bool randomEnabled()const{return m_activeRandom;}
 Q_INVOKABLE QVariantList queueFor(const QString &uri)const;
 bool enrichmentEnabled()const;
 void setEnrichmentEnabled(bool);
 QString enrichmentStatus()const;
 Q_INVOKABLE void cancel();
signals:void changed();void previewReady();void ready(const QVariantList&);void resumeReady(const QVariantList&,int);
private:
 void prepare(const QVariantList&,int,bool,bool,bool,bool);
 QVariantList m_previewQueue,m_appliedQueue;bool m_preview=false,m_activeDiscovery=false,m_activeRandom=false;
 QStringList m_retainedSource;QVariantList m_retainedExternal;
 QVariantMap known(const QVariantMap&)const;QString genres(const QVariantMap&)const;void search(int serial,int index);void finish(int serial);
 void resolveGenres(int serial, QVariantMap anchor);
 void startSearch(int serial, const QVariantMap &anchor, const QString &genre);
 QString m_failure;
 DiscoveryLyrics *m_enrichment=nullptr;
 SpotifyApi*m_api;MusicCompanion*m_music;QJsonObject m_rules;QVariantList m_source,m_pool;QStringList m_queries;QHash<QString,int> m_totals;bool m_busy=false,m_random=false,m_discovery=false,m_album=false;int m_generation=0,m_selected=0;quint32 m_seed=0;QString m_status;
};
