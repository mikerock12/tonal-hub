#pragma once

#include <QQmlNetworkAccessManagerFactory>

// Dá ao QML um QNetworkAccessManager com cache em disco, para as capas
// não serem baixadas de novo a cada vez que aparecem na tela.
class NetworkFactory : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
};
