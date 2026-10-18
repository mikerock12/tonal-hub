#pragma once
#include <QObject>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QElapsedTimer>
#include <QThread>
#include <QVariantList>
#include <QSet>
#include <QJsonDocument>
#include <functional>
#include <memory>

class MusicCompanion : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
  Q_PROPERTY(QVariantList favorites READ favorites NOTIFY changed)
  Q_PROPERTY(QVariantList playlists READ playlists NOTIFY changed)
  Q_PROPERTY(QVariantList lines READ lines NOTIFY lyricsChanged)
  Q_PROPERTY(QString lyricStatus READ lyricStatus NOTIFY lyricsChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY lyricsChanged)
  Q_PROPERTY(bool historyEnabled READ historyEnabled WRITE setHistoryEnabled NOTIFY changed)
  Q_PROPERTY(QString background READ background WRITE setBackground NOTIFY changed)
  Q_PROPERTY(QString photo READ photo WRITE setPhoto NOTIFY changed)
  Q_PROPERTY(QVariantList searches READ searches NOTIFY changed)
  Q_PROPERTY(bool syncEnabled READ syncEnabled WRITE setSyncEnabled NOTIFY lyricsChanged)
  Q_PROPERTY(int syncOffset READ syncOffset NOTIFY lyricsChanged)
  Q_PROPERTY(QVariantMap lyricStyle READ lyricStyle NOTIFY changed)
public:
  explicit MusicCompanion(QObject *parent=nullptr,const QString &root={});
  ~MusicCompanion() override;
  QVariantMap current() const {return m_current;}
  QVariantList favorites() const;
  QVariantList playlists() const;
  QVariantList lines() const{return m_lines;}
  QString lyricStatus() const{return m_lyricStatus;}
  bool busy() const{return m_busy;}
  bool historyEnabled() const{return m_data.value("history").toBool(true);}
  void setHistoryEnabled(bool);
  QString background() const{return m_data.value("background").toString("Aurora");}
  void setBackground(const QString &);
  QString photo() const{return m_data.value("photo").toString();}
  void setPhoto(const QString &);
  QVariantList searches() const;
  QVariantMap lyricStyle() const;
  Q_INVOKABLE void rememberSearch(const QString &);
  Q_INVOKABLE void forgetSearch(const QString &query={});
  Q_INVOKABLE void setLyricStyle(const QString &,const QVariant &);
  bool syncEnabled() const;int syncOffset() const;void setSyncEnabled(bool);
  Q_INVOKABLE void alignLyrics(int row,int position);
  Q_INVOKABLE int lineAt(int position) const;
  Q_INVOKABLE bool exportSyncFeedback(const QUrl &);
  Q_INVOKABLE QVariantMap summary(const QString &kind,const QString &value) const;
  Q_INVOKABLE void mergeMetadata(const QString &key,const QVariantMap &);
  Q_INVOKABLE void refreshMetadata();
  Q_INVOKABLE void selectTrack(const QVariantMap &);
  Q_INVOKABLE void pulse(bool playing,int positionMs);
  Q_INVOKABLE bool liked(const QVariantMap &) const;
  Q_INVOKABLE void favorite(const QVariantMap &);
  void setFavorite(const QVariantMap &,bool);
  Q_INVOKABLE QString createPlaylist(const QString &);
  Q_INVOKABLE void renamePlaylist(const QString &,const QString &);
  Q_INVOKABLE void deletePlaylist(const QString &);
  Q_INVOKABLE void addToPlaylist(const QString &,const QVariantMap &);
  Q_INVOKABLE void removeFromPlaylist(const QString &,int);
  Q_INVOKABLE QVariantList playlistTracks(const QString &) const;
  Q_INVOKABLE void play(const QVariantList &tracks,int index=0){emit queueRequested(tracks,index);}
  Q_INVOKABLE QVariantMap statistics(int days,const QString &dimension) const;
  Q_INVOKABLE QVariantMap insights(int days) const;
  Q_INVOKABLE void clearHistory();
  Q_INVOKABLE void setGenre(const QString &);
  Q_INVOKABLE void editMetadata(const QString &,const QString &);
  Q_INVOKABLE void fetchLyrics();
  Q_INVOKABLE void translate(const QString &language);
  Q_INVOKABLE void importLyrics(const QString &,bool translation=false);
  Q_INVOKABLE bool useDisplayedLyrics(const QString &trackUri,const QVariantList &lines);
  Q_INVOKABLE void seek(int ms){emit seekRequested(ms);}
  Q_INVOKABLE bool exportBackup(const QUrl &);
  Q_INVOKABLE bool restoreBackup(const QUrl &);
  static QString key(const QVariantMap &);
  static QVariantMap safeTrack(const QVariantMap &);
  static QVariantList parseLyrics(const QString &);
  static QVariantMap tone(const QString &,const QString &genre);
signals:
  void metadataRequested(const QVariantMap &track);
  void changed();void currentChanged();void lyricsChanged();
  void message(const QString &);
  void queueRequested(const QVariantList &,int);
  void seekRequested(int);
  void spotifyFavoriteRequested(const QVariantMap &,bool);
private:
  friend class CompanionTests;
  QString syncKey() const;void metadataNext();
  void save();void flush();void checkpoint();
  void get(const QUrl &,std::function<void(int,const QJsonDocument &)>);
  void acceptLyrics(const QJsonObject &);
  bool retryLyrics(int status,int generation);
  void nextTranslation(int);
  void updateTone();
  QJsonObject m_data;
  QVariantMap m_current;
  QVariantList m_lines;
  QString m_path,m_lyricStatus="Abra uma música e busque sua letra.",m_session;
  int m_generation=0,m_position=0,m_translationIndex=0;
  int m_lyricRetryCount=0;QString m_lyricRetryKey;
  bool m_playing=false,m_busy=false;
  qint64 m_pendingMs=0,m_sessionMs=0;
  bool m_counted=false;
  QTimer m_saveTimer;
  QElapsedTimer m_clock;
  QNetworkAccessManager m_net;
  QHash<QString,QVariantList> m_lyricCache;
  QHash<QString,QString> m_translationCache;
  QString m_language;int m_translationRetries=0,m_translationFailures=0;
  QStringList m_metadataQueue;QSet<QString> m_metadataPending;bool m_metadataBusy=false;
  mutable quint64 m_revision=0,m_summaryRevision=~quint64(0);mutable QHash<QString,QVariantMap> m_summary;
  QObject *m_disk=nullptr;QThread m_diskThread;
  std::shared_ptr<QJsonObject> m_written=std::make_shared<QJsonObject>();
};
