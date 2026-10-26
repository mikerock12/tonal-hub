#pragma once
#include <QObject>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTimer>
#include <QSaveFile>
#include <QFile>
#include <QCryptographicHash>
#include <memory>
#include <QThread>
#include <QMap>

// The player never links llama.cpp. Only the short lived TonalStories helper does.
class LocalStoriesWorker : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
 explicit LocalStoriesWorker(const QString &root,QObject *parent=nullptr);
 ~LocalStoriesWorker() override;
 void initialize();
 void setPlaying(bool playing);
 QVariantMap state() const;
 void capture(const QVariantMap &track,const QString &text);
 void analyze(const QVariantList &tracks,int days);
 void selectPeriod(int days){if(days!=m_days){cancel();m_days=days;m_summary.clear();m_sources.clear();m_items={};m_resumable=false;emit changed();}}
 void resetSummary(){cancel();m_summary.clear();m_sources.clear();m_items={};m_resumable=false;QFile::remove(m_root+"/resume-v5.bin");QFile::remove(m_root+"/resume-v5.bin.tmp");emit changed();}
 Q_INVOKABLE void download();
 Q_INVOKABLE void cancel();
 Q_INVOKABLE void removeModel();
signals:
 void changed();
 void snapshot(QVariantMap state);
private:
 void next(quint64);
 void run(quint64);
 void finish(const QString &);
 QString fileFor(const QVariantMap &) const;
 QString modelPath() const;
 void trimCache();
 QString m_root,m_status="Resumo das letras no aparelho. O modelo é opcional.",m_summary,m_sources,m_key;
 QVariantList m_tracks;QJsonArray m_texts,m_preparedTracks,m_items,m_known;
 int m_index=0,m_days=7;bool m_busy=false,m_downloading=false,m_ready=false;
 qint64 m_downloaded=0,m_cooldown=0;quint64 m_serial=0;
 QNetworkAccessManager m_net;QPointer<QNetworkReply> m_reply;QPointer<QProcess> m_process;QTimer m_deadline,m_pressure,m_notify;
 bool m_playing=false,m_resumeDownload=false,m_complete=false,m_resumable=false;
 std::unique_ptr<QSaveFile> m_file;std::unique_ptr<QCryptographicHash> m_hash;
};

// GUI facade: only copies bounded requests and receives immutable snapshots.
// Files, hashing, network and text preparation belong to the low-priority actor.
class LocalStories : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
 explicit LocalStories(const QString &root,QObject *parent=nullptr);
 ~LocalStories() override;
 QVariantMap state()const{return m_state;}
 void capture(const QVariantMap &,const QString &);
 void analyze(const QVariantList &,int);
 void selectPeriod(int);
 void resetSummary();
 void setPlaying(bool);
 Q_INVOKABLE void download();
 Q_INVOKABLE void cancel();
 Q_INVOKABLE void removeModel();
signals:
 void changed();
private:
 void flushCaptures();
 LocalStoriesWorker *m_worker;
 QThread m_thread;QTimer m_captureTimer;
 QMap<QString,QPair<QVariantMap,QString>> m_pending;
 QVariantMap m_state{{"ready",false},{"busy",false},{"summary",QString()},{"status",QStringLiteral("Preparando leitor local…")}};
 bool m_playing=false;
};
