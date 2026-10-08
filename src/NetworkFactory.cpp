#include "NetworkFactory.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QStandardPaths>

QNetworkAccessManager *NetworkFactory::create(QObject *parent)
{
    auto *nam = new QNetworkAccessManager(parent);
    auto *cache = new QNetworkDiskCache(nam);
    cache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
                             + QStringLiteral("/images"));
    cache->setMaximumCacheSize(150LL * 1024 * 1024); // 150 MB de capas
    nam->setCache(cache);
    return nam;
}
