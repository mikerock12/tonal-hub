#pragma once
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonDocument>
#include <QUrl>
#include <functional>
class MusicCompanion;
class TextPlaylistImport : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList rows READ rows NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString detectedArtist READ detectedArtist NOTIFY changed)
    Q_PROPERTY(QString cover READ cover NOTIFY changed)
    Q_PROPERTY(QString savedId READ savedId NOTIFY changed)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY changed)
    Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
public:
    using Reply=std::function<void(int,const QJsonDocument &)>;
    using Request=std::function<void(const QByteArray &,const QString &,const QVariantMap &,
        const QByteArray &,const QByteArray &,Reply,std::function<bool()>)>;
    TextPlaylistImport(MusicCompanion *,Request,QObject *parent=nullptr);
    QVariantList rows() const{return m_rows;}
    bool busy() const{return m_busy;}bool syncing() const{return m_syncing;}
    QString status() const{return m_status;}QString detectedArtist() const{return m_detected;}
    QString cover() const{return m_cover;}QString savedId() const{return m_saved;}
    int selectedCount() const;
    QVariantMap details() const;
    Q_INVOKABLE void parse(const QString &,int format=0,const QString &artist={});
    Q_INVOKABLE void search();Q_INVOKABLE void cancel();
    Q_INVOKABLE void choose(int row,int candidate,bool selected=true);
    Q_INVOKABLE void edit(int row,const QString &artist,const QString &title);
    Q_INVOKABLE void move(int row,int direction);
    Q_INVOKABLE bool setCover(const QUrl &);
    Q_INVOKABLE void clearCover();
    Q_INVOKABLE bool saveLocal(const QString &name,const QString &description,bool deduplicate=false);
    Q_INVOKABLE bool loadSaved(const QString &id);
    Q_INVOKABLE void sync(bool publicPlaylist=false);
    Q_INVOKABLE void acknowledgeUncertainCreate();
signals:
    void changed();void spotifyChanged();
private:
    void call(const QByteArray &,const QString &,const QVariantMap &,const QByteArray &,const QByteArray &,Reply);
    void nextSearch(int row,int attempt=0);
    void uploadBatch();void uploadCover();void verifyRemote(const QString &path,QVariantList accumulated={});
    void finish(const QString &);void patch(const QVariantMap &);
    QVariantList selectedTracks(bool deduplicate=false) const;
    MusicCompanion *m_music;Request m_request;QVariantList m_rows,m_upload;
    QString m_status,m_detected,m_cover,m_saved,m_cloudId;
    int m_generation=0,m_cursor=0,m_verifiedPages=0;bool m_busy=false,m_syncing=false;
};
