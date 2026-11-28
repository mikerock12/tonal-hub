#pragma once
#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
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
// shared decision, each with match/matchLabel/reason/safe/requestedArtist/group/artistIds), choice (index into
// candidates or -1), selected, match (decision code), reason, status, section, manual/manualUri/manualTrack
// (UserOverride), ignored, batchId, replacementOf/replacedBy (inactive when replaced), duplicateOf.
// 0.18.3: drafts are versioned JSON files beside the personal library (text-drafts/<id>.json, atomic writes);
// an autosave copy protects against crashes but never replaces the explicit "save draft" choice.
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
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
    Q_PROPERTY(QString draftId READ draftId NOTIFY changed)
    Q_PROPERTY(QString draftName READ draftName NOTIFY changed)
    Q_PROPERTY(QVariantList drafts READ drafts NOTIFY draftsChanged)
    Q_PROPERTY(QVariantList batches READ batches NOTIFY changed)
    Q_PROPERTY(QVariantMap pins READ pins NOTIFY changed)
public:
    using Reply=std::function<void(int,const QJsonDocument &)>;
    using Request=std::function<void(const QByteArray &,const QString &,const QVariantMap &,
        const QByteArray &,const QByteArray &,Reply,std::function<bool()>)>;
    static constexpr int kDraftSchema=1, kMaxDrafts=20, kStoredCandidates=8;
    TextPlaylistImport(MusicCompanion *,Request,QObject *parent=nullptr);
    ~TextPlaylistImport() override;
    QVariantList rows() const{return m_rows;}
    bool busy() const{return m_busy;}bool syncing() const{return m_syncing;}
    QString status() const{return m_status;}QString detectedArtist() const{return m_detected;}
    QString cover() const{return m_cover;}QString savedId() const{return m_saved;}
    int selectedCount() const;
    QVariantMap summary() const;   // exclusive sections + selected/pending/ignored/replaced/active/toSave/total
    QVariantMap details() const;
    bool dirty() const{return m_dirty;}
    QString draftId() const{return m_draftId;}QString draftName() const{return m_draftName;}
    QVariantList drafts() const{return m_drafts;}QVariantList batches() const{return m_batches;}QVariantMap pins() const{return m_pins;}
    void setRequestSpacing(int ms){m_spacing=std::max(0,ms);}
    void setDraftFolder(const QString &folder){m_draftFolder=folder;refreshDrafts();}
    QString draftFolder() const;
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
    // Drafts (V3.1 section 13).
    Q_INVOKABLE bool saveDraft(const QString &name={});
    Q_INVOKABLE bool loadDraft(const QString &id);
    Q_INVOKABLE bool deleteDraft(const QString &id);
    Q_INVOKABLE bool renameDraft(const QString &id,const QString &name);
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE void refreshDrafts();
    // More songs for the same playlist (V3.1 section 14). mode: "add" or "replace".
    Q_INVOKABLE QVariantMap previewBatch(const QString &text,int format,const QString &artist) const;
    Q_INVOKABLE bool addBatch(const QString &text,int format,const QString &artist,const QString &mode,const QVariantList &replaceIds,bool includeDuplicates);
    Q_INVOKABLE bool undoLastBatch();
    // Artist identity pinned by the user for every line of that artist in this list (V3 section 3).
    Q_INVOKABLE bool pinArtist(int row,int candidate);
    Q_INVOKABLE void unpinArtist(const QString &artist);
    static QString sectionOf(const QVariantMap &row);
signals:
    void changed();void spotifyChanged();void draftsChanged();
private:
    void call(const QByteArray &,const QString &,const QVariantMap &,const QByteArray &,const QByteArray &,Reply);
    void startSearch(const QSet<int> &only);void nextRow(int row);void step();void completeRow();void finishSearch(const QString &message={});
    void handleReply(int id,tonal::playlist::Request::Kind kind,const tonal::playlist::AlbumHit &owner,int status,const QJsonDocument &doc);
    void applyDecision(int row);int rowOf(int requestId) const;void markDuplicate(int row);
    void touch();void autosave();QJsonObject draftJson() const;bool restore(const QJsonObject &);bool writeAtomically(const QString &path,const QByteArray &);
    void uploadBatch();void uploadCover();void verifyRemote(const QString &path,QVariantList accumulated={});
    void finish(const QString &);void patch(const QVariantMap &);
    QVariantList selectedTracks(bool deduplicate,QVariantList *audit=nullptr) const;
    MusicCompanion *m_music;Request m_request;QVariantList m_rows,m_upload,m_drafts,m_batches;QVariantMap m_pins;
    QString m_status,m_detected,m_cover,m_saved,m_cloudId,m_targetId,m_stopReason,m_draftId,m_draftName,m_draftFolder,m_createdAt,m_defaultArtist;
    int m_generation=0,m_cursor=0,m_verifiedPages=0,m_nextId=0,m_nextBatch=0,m_lookupId=-1,m_spacing=350,m_format=0;
    bool m_busy=false,m_syncing=false,m_dirty=false;QSet<int> m_only;
    std::unique_ptr<tonal::playlist::Lookup> m_lookup;QVariantList m_pool;
    QHash<QString,std::vector<tonal::playlist::ArtistHit>> m_artistCache;   // per import: same artist on many lines
    QHash<QString,QPair<int,QJsonDocument>> m_responses;                     // per import: discography read once per artist
    QTimer m_autosave;
};
