#pragma once
#include <QJsonObject>
namespace personal {
QJsonObject load(const QString &path);
bool save(const QString &path,const QJsonObject &data,QJsonObject &previous);
}
