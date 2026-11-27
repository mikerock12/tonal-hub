#pragma once
#include <QObject>
#include <QHash>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonDocument>
#include <QUrl>
#include <functional>
#include <memory>
#include <vector>
#include "playlist/TextPlaylistCore.hpp"
class MusicCompanion;
// Rows are QVariantMaps keyed by a stable requestId. Fields: artist, title, original, candidates (ordered by the
// shared decision, each with match/matchLabel/reason/safe/requestedArtist), choice (index into candidates or -1),
// selected, match (decision code), reason, status, section, manual/manualUri/manualTrack (UserOverride), ignored.
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
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
public:
    using Reply=std::function<void(int,const QJsonDocument &)>;
    using Request=std::function<void(const QByteArray &,const QString &,const QVariantMap &,
        const QByteArray &,const QByteArray &,Reply,std::function<bool()>)>;
    TextPlaylistImport(MusicCompanion *,Request,QObject *parent=nullptr);
    ~TextPlaylistImport() override;
    QVariantList rows() const{return m_rows;}
    bool busy() const{return m_busy;}bool syncing() const{return m_syncing;}
    QString status() const{return m_status;}QString detectedArtist() const{return m_detected;}
    QString cover() const{return m_cover;}QString savedId() const{return m_saved;}
    int selectedCount() const;
    QVariantMap summary() const;   // selected, pending, ignored, total + one count per section
    QVariantMap details() const;
    void setRequestSpacing(int ms){m_spacing=std::max(0,ms);}
    Q_INVOKABLE void parse(const QString &,int format=0,const QString &artist={});
    Q_INVOKABLE void search();Q_INVOKABLE void cancel();
    Q_INVOKABLE void retry(int row);
    Q_INVOKABLE void choose(int row,int candidate,bool selected=true);
    Q_INVOKABLE void ignore(int row);
    Q_INVOKABLE void edit(int row,const QString &artist,const QString &title);
    Q_INVOKABLE void move(int row,int direction);
    Q_INVOKABLE bool setCover(const QUrl &);
    Q_INVOKABLE void clearCover();
    Q_INVOKABLE bool saveLocal(const QString &name,const QString &description,bool deduplicate=false);
    Q_INVOKABLE bool setTargetPlaylist(const QString &id);
    Q_INVOKABLE bool loadSaved(const QString &id);
    Q_INVOKABLE void sync(bool publicPlaylist=false);
    Q_INVOKABLE void acknowledgeUncertainCreate();
    static QString sectionOf(const QVariantMap &row);
signals:
    void changed();void spotifyChanged();
private:
    void call(const QByteArray &,const QString &,const QVariantMap &,const QByteArray &,const QByteArray &,Reply);
    void startSearch(int only);void nextRow(int row);void step();void completeRow();void finishSearch(const QString &message={});
    void applyDecision(int row);int rowOf(int requestId) const;
    void uploadBatch();void uploadCover();void verifyRemote(const QString &path,QVariantList accumulated={});
    void finish(const QString &);void patch(const QVariantMap &);
    QVariantList selectedTracks(bool deduplicate,QVariantList *audit=nullptr) const;
    MusicCompanion *m_music;Request m_request;QVariantList m_rows,m_upload;
    QString m_status,m_detected,m_cover,m_saved,m_cloudId,m_targetId,m_stopReason;
    int m_generation=0,m_cursor=0,m_verifiedPages=0,m_nextId=0,m_only=-1,m_lookupId=-1,m_spacing=120;bool m_busy=false,m_syncing=false;
    std::unique_ptr<tonal::playlist::Lookup> m_lookup;QVariantList m_pool;
    QHash<QString,std::vector<tonal::playlist::ArtistHit>> m_artistCache;   // per import: same artist on many lines
};
