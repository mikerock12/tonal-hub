#pragma once
#include <QNetworkReply>
#include <QTimer>

// TransferTimeout bounds inactivity; this also bounds DNS, connection queues,
// and a server that keeps sending a few bytes without finishing its response.
inline void setReplyDeadline(QNetworkReply *reply, int milliseconds) {
  auto *timer = new QTimer(reply);
  timer->setSingleShot(true);
  QObject::connect(timer, &QTimer::timeout, reply, [reply] { reply->abort(); });
  QObject::connect(reply, &QNetworkReply::finished, timer, &QTimer::stop);
  timer->start(milliseconds);
}
