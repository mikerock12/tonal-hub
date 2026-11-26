#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QJsonObject>
class LibrespotProcess;

/** Public Spotify catalog reads through the native session (0.18.1): artist overview
 *  (listeners, rank, cities, top-track plays) and per-track play counts of albums.
 *  Own TonalJam process, one request at a time, bounded caches; never touches the Jam slot. */
class CatalogStats:public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY playsChanged)
public:
    explicit CatalogStats(LibrespotProcess*,QObject* parent=nullptr);
    ~CatalogStats() override;
    int revision()const{return m_revision;}
    /** Known public plays of a track URI, or -1. Never estimated. */
    Q_INVOKABLE double plays(const QString &uri)const{return m_plays.contains(uri)?double(m_plays.value(uri)):-1.0;}
    Q_INVOKABLE void artistStats(const QString &artistId);
    Q_INVOKABLE void requestAlbums(const QStringList &albumIds);
    Q_INVOKABLE bool albumKnown(const QString &albumId)const;
    Q_INVOKABLE int pendingAlbums()const{return int(m_albumQueue.size()+(m_op=="albumPlays"?m_inFlight.size():0));}
    /** 4,5M · 2,2K · 200,5K (pt-BR decimal comma); below 1000 the exact number. */
    Q_INVOKABLE static QString compact(double value);
signals:
    void playsChanged();
    void artistStatsReady(const QString &artistId,const QVariantMap &stats);
    void artistStatsFailed(const QString &artistId,int status);
private:
    void pump();
    void finish(const QJsonObject &reply);
    LibrespotProcess *m_engine;
    QProcess m_process;QTimer m_timeout,m_idle,m_retry;
    QByteArray m_buffer;
    QString m_op,m_artist;
    QStringList m_artistQueue,m_albumQueue,m_inFlight;
    QSet<QString> m_queued;
    QHash<QString,qint64> m_plays,m_albumsAt,m_artistAt;
    QStringList m_playsOrder;
    QHash<QString,QVariantMap> m_artistCache;
    qint64 m_blockedUntil=0;
    int m_revision=0;
    bool m_busy=false;
};
