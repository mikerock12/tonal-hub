#pragma once
#include <QObject>
#include <QVariantList>
#include <QJsonObject>
#include <QHash>
class SpotifyApi;class MusicCompanion;
class ListeningMix:public QObject {
 Q_OBJECT
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
public:
 ListeningMix(SpotifyApi*,MusicCompanion*,QObject*parent=nullptr);
 bool busy()const{return m_busy;}QString status()const{return m_status;}
 Q_INVOKABLE QVariantList order(int count,quint32 seed)const;
 Q_INVOKABLE void play(const QVariantList&,int selected,bool random,bool discovery,bool album=false);
 Q_INVOKABLE void cancel();
signals:void changed();void ready(const QVariantList&);
private:
 QVariantMap known(const QVariantMap&)const;QString genres(const QVariantMap&)const;void search(int serial,int index);void finish(int serial);
 SpotifyApi*m_api;MusicCompanion*m_music;QJsonObject m_rules;QVariantList m_source,m_pool;QStringList m_queries;QHash<QString,int> m_totals;bool m_busy=false,m_random=false,m_discovery=false,m_album=false;int m_generation=0,m_selected=0;quint32 m_seed=0;QString m_status;
};
