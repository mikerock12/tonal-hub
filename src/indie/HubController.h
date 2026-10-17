#pragma once
#include <QObject>
#include <QUrl>
class HubController : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  Q_INVOKABLE void indie(const QString &query = {});
  Q_INVOKABLE void spotify();
  Q_INVOKABLE void hub();
  Q_INVOKABLE void external(const QUrl &url);
signals:
  void error(const QString &message);
};
