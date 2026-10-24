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
#include <QCryptographicHash>
#include <memory>

// The player never links llama.cpp. Only the short lived TonalStories helper does.
class LocalStories : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap state READ state NOTIFY changed)
public:
 explicit LocalStories(const QString &root,QObject *parent=nullptr);
 ~LocalStories() override;
 QVariantMap state() const;
 void capture(const QVariantMap &track,const QString &text);
 void analyze(const QVariantList &tracks,int days);
 void selectPeriod(int days){if(days!=m_days){cancel();m_days=days;m_summary.clear();m_sources.clear();emit changed();}}
 void resetSummary(){cancel();m_summary.clear();m_sources.clear();emit changed();}
 Q_INVOKABLE void download();
 Q_INVOKABLE void cancel();
 Q_INVOKABLE void removeModel();
signals:
 void changed();
private:
 void next(quint64);
 void run(quint64);
 void finish(const QString &);
 QString fileFor(const QVariantMap &) const;
 QString modelPath() const;
 void trimCache();
 QString m_root,m_status="Resumo das letras no aparelho. O modelo é opcional.",m_summary,m_sources,m_key;
 QVariantList m_tracks;QJsonArray m_texts;
 int m_index=0,m_days=7;bool m_busy=false,m_downloading=false,m_ready=false;
 qint64 m_downloaded=0,m_cooldown=0;quint64 m_serial=0;
 QNetworkAccessManager m_net;QPointer<QNetworkReply> m_reply;QPointer<QProcess> m_process;QTimer m_deadline;
 std::unique_ptr<QSaveFile> m_file;std::unique_ptr<QCryptographicHash> m_hash;
};
