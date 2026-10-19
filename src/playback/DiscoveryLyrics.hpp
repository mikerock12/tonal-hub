#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QVariantList>
#include <QTimer>
#include <functional>

class QNetworkReply;
// Independent of displayed lyrics and transport. Stores derived scores only;
// requests are serialized, cancellable and never delay a prepared mix.
class DiscoveryLyrics final : public QObject {
public:
    using Reply = std::function<void(int,const QJsonObject&,int)>;
    using Fetch = std::function<void(const QVariantMap&,Reply)>;
    DiscoveryLyrics(QString path,QObject *disk,QObject *parent=nullptr,Fetch fetch={});
    ~DiscoveryLyrics() override;
    void enqueue(const QVariantList &references,const QVariantList &candidates);
    QVariantMap lookup(const QVariantMap&)const;
    bool enabled()const{return m_enabled;}
    void setEnabled(bool);
    QString status()const;
    std::function<void()> changed;
    static QString signature(const QVariantMap&);
    static bool matches(const QVariantMap&,const QJsonObject&);
private:
    void next();
    void fetch(const QVariantMap&,Reply);
    void persist();
    static QString normalize(const QString&);
    QString m_path,m_active;
    QPointer<QObject> m_disk;
    Fetch m_fetch;
    QJsonObject m_cache;
    QVariantList m_waiting;
    QTimer m_timer;
    QNetworkAccessManager m_net;
    QPointer<QNetworkReply> m_reply;
    bool m_enabled=true;
    int m_serial=0,m_consulted=0,m_analyzed=0;
};
