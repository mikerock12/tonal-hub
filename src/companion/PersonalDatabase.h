#pragma once
#include <QJsonObject>
namespace personal {
QJsonObject load(const QString &path);
QJsonObject historySnapshot(const QString &path,const QJsonObject &current,qint64 cutoff);
QJsonObject fullSnapshot(const QString &path);
bool save(const QString &path,const QJsonObject &data,QJsonObject &previous);
}
