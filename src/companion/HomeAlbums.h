#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <algorithm>

namespace personal {
inline QVariantList homeAlbums(const QJsonObject &data) {
    const auto tracks=data.value("tracks").toObject();QHash<QString,QVariantMap> albums;
    static const QRegularExpression valid("^[A-Za-z0-9]{22}$");
    const auto now=QDateTime::currentMSecsSinceEpoch();
    for(const auto &value:data.value("events").toArray()) {
        const auto event=value.toObject(),track=tracks.value(event.value("key").toString()).toObject();
        const auto id=track.value("albumId").toString(),name=track.value("album").toString();
        if(track.value("provider").toString()!="Spotify"||!valid.match(id).hasMatch()||name.isEmpty()||event.value("ms").toDouble()<=0||event.value("at").toDouble()>now)continue;
        auto &album=albums[id];album["kind"]="album";album["id"]=id;album["uri"]="spotify:album:"+id;
        album["title"]=name;album["subtitle"]=track.value("artist").toString();
        if(!track.value("image").toString().isEmpty())album["imageUrl"]=track.value("image").toString();
        album["lastAt"]=std::max(album.value("lastAt").toLongLong(),qint64(event.value("lastAt").toDouble(event.value("at").toDouble())));
        album["listenedMs"]=album.value("listenedMs").toLongLong()+qint64(event.value("ms").toDouble());
    }
    QList<QVariantMap> recent=albums.values(),ranked=recent;
    auto byRecent=[](const auto &a,const auto &b){const auto at=a.value("lastAt").toLongLong(),bt=b.value("lastAt").toLongLong();return at==bt?a.value("id").toString()<b.value("id").toString():at>bt;};
    std::sort(recent.begin(),recent.end(),byRecent);
    std::sort(ranked.begin(),ranked.end(),[&](const auto&a,const auto&b){const auto am=a.value("listenedMs").toLongLong(),bm=b.value("listenedMs").toLongLong();return am==bm?byRecent(a,b):am>bm;});
    QVariantList result;QSet<QString> ids;
    for(const auto &list:{recent.mid(0,60),ranked.mid(0,20)})for(const auto &a:list)if(!ids.contains(a.value("id").toString())){ids.insert(a.value("id").toString());result<<a;}
    return result;
}
}
