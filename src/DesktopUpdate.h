#pragma once

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QTemporaryFile>
#include <QUrl>

class DesktopUpdate : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
  Q_PROPERTY(bool ready READ ready NOTIFY changed)
  Q_PROPERTY(bool installed READ installed CONSTANT)
public:
  struct Release {
    QString version;
    QString fileName;
    QByteArray sha256;
    qint64 size = 0;
    QUrl url;
  };
  explicit DesktopUpdate(QObject *parent = nullptr);
  QString status() const { return m_status; }
  bool busy() const { return m_busy; }
  QString availableVersion() const { return m_release.version; }
  bool ready() const { return m_ready; }
  bool installed() const { return !m_installPath.isEmpty(); }
  static Release parseRelease(const QByteArray &data);
  static bool newer(const QString &candidate, const QString &installed);
  static bool allowedAssetRedirect(const QUrl &url);

  Q_INVOKABLE void check();
  Q_INVOKABLE void download();
  Q_INVOKABLE void install();
signals:
  void changed();
private:
  void fetchAsset(const QUrl &url, int redirects = 0);
  void fail(const QString &status);
  void status(const QString &text);
  QNetworkAccessManager m_network;
  QTemporaryFile m_file;
  QCryptographicHash m_hash{QCryptographicHash::Sha256};
  QString m_installPath, m_status;
  Release m_release;
  qint64 m_received = 0;
  bool m_busy = false, m_ready = false;
};
