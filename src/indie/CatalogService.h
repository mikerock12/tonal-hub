#pragma once
#include <QCache>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <functional>
#include <memory>

// Public-source catalog. Never receives Spotify credentials or API objects.
class CatalogService : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList items READ items NOTIFY changed)
  Q_PROPERTY(QStringList messages READ messages NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool canRetry READ canRetry NOTIFY changed)
  Q_PROPERTY(bool inDetail READ inDetail NOTIFY changed)
  Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
  Q_PROPERTY(QString liveStatus READ liveStatus NOTIFY changed)
  Q_PROPERTY(QString query READ query NOTIFY changed)
  Q_PROPERTY(QString heading READ heading NOTIFY changed)
  Q_PROPERTY(
      QVariantMap configuration READ configuration NOTIFY configurationChanged)
public:
  explicit CatalogService(QObject *parent = nullptr,
                          bool loadConnections = true);
  QVariantList items() const { return m_items; }
  QStringList messages() const { return m_messages; }
  bool busy() const { return m_pending > 0; }
  bool canRetry() const { return !m_failed.isEmpty(); }
  bool inDetail() const { return !m_route.isEmpty(); }
  bool hasMore() const;
  QString liveStatus() const { return m_liveStatus; }
  QString query() const { return m_query; }
  QString heading() const { return m_heading; }
  QVariantMap configuration() const;
  Q_INVOKABLE void search(const QString &query,
                          const QString &provider = "all");
  Q_INVOKABLE void open(const QVariantMap &item);
  Q_INVOKABLE void more();
  Q_INVOKABLE void retryFailed();
  Q_INVOKABLE bool configure(const QVariantMap &values);
  Q_INVOKABLE void clearLibraryCredentials();
  Q_INVOKABLE void resolve(const QVariantMap &item);
  static bool publicUrl(const QUrl &url);
  static QString identity(const QVariantMap &item);
signals:
  void searchStarted(const QString &query, const QString &provider);
  void changed();
  void configurationChanged();
  void playable(const QUrl &url, const QString &title);

private:
  void reloadConnections();
  bool m_restoreConnections = true;
  friend class IndieCatalogTests;
  using Handler = std::function<void(const QJsonObject &)>;
  struct Request {
    QString provider;
    QUrl url;
    Handler done;
    int generation = 0, attempt = 0;
    QString notice;
  };
  struct CachedResponse {
    QByteArray bytes;
    qint64 savedAt;
  };
  void reset(const QString &heading);
  void fetch(const QString &provider, const QUrl &url, Handler done);
  void enqueue(const std::shared_ptr<Request> &request);
  void dispatch(QNetworkReply *reply, const std::shared_ptr<Request> &request);
  static qint64 retryDelay(const QByteArray &header, int attempt, qint64 now);
  static int relevance(const QVariantMap &item, const QString &query);
  bool cacheable(const QString &provider, const QUrl &url) const;
  void append(const QVariantMap &item);
  void list(int page);
  void audius(const QJsonObject &, const QString &kind);
  void musicbrainz(const QJsonObject &, const QString &kind);
  void archive(const QJsonObject &, const QString &provider = "Live Music Archive");
  void archiveFiles(const QJsonObject &, const QString &id, const QString &provider);
  void jamendo(const QJsonObject &, const QString &kind);
  void subsonic(const QJsonObject &);
  QUrl subUrl(const QString &method, const QVariantMap &args = {}) const;
  QUrl api(const QString &base, const QVariantMap &args = {}) const;
  void message(const QString &provider, const QString &text);
  QNetworkAccessManager m_net;
  std::function<QNetworkReply *(const QNetworkRequest &)> m_get;
  QList<QNetworkReply *> m_replies;
  QTimer m_mbTimer;
  QQueue<std::function<void()>> m_mbQueue;
  QList<std::shared_ptr<Request>> m_failed;
  // Public MusicBrainz metadata only. Bounded to 16 MiB, never persisted.
  QCache<QString, CachedResponse> m_mbCache{16 * 1024};
  qint64 m_mbNotBefore = 0;
  bool m_mbHasMore = false;
  bool m_lmaHasMore = false;
  QVariantList m_items;
  QStringList m_messages;
  QString m_liveStatus;
  QSet<QString> m_seen;
  QString m_query, m_heading = "Descubra outras gravações", m_provider = "all";
  QVariantMap m_route;
  int m_generation = 0, m_pending = 0, m_page = 0;
  QString m_jamendo, m_server, m_user, m_password;
};
