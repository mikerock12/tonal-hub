#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QVariantMap>
#include <optional>

namespace tonal::jam {
inline QString text(const QJsonObject &o,const char *first,const char *second=nullptr){
    auto value=o.value(first);if(!value.isString()&&second)value=o.value(second);
    return value.isString()?value.toString():QString();
}
inline bool atom(const QString &s,int min,int max){static const QRegularExpression valid("^[A-Za-z0-9_-]+$");return s.size()>=min&&s.size()<=max&&valid.match(s).hasMatch();}
inline std::optional<QVariantMap> parse(const QJsonObject &value,const QString &viewer={}){
    const auto s=value.value("session").isObject()?value.value("session").toObject():value;
    auto id=text(s,"session_id","sessionId");if(id.isEmpty())id=text(s,"id");if(!atom(id,6,128))return {};
    auto rows=s.value("session_members");if(!rows.isArray())rows=s.value("sessionMembers");
    if(!rows.isArray())rows=s.value("members");
    if(!rows.isArray()||rows.toArray().size()>128)return {};
    QVariantList members;QSet<QString> ids;
    for(const auto &v:rows.toArray()){
        if(!v.isObject())return {};const auto m=v.toObject();auto member=text(m,"id","username");
        if(member.isEmpty())member=text(m,"user_id");
        auto name=text(m,"display_name","displayName");if(name.isEmpty())name=text(m,"username");
        if(name.isEmpty())name=text(m,"name");
        if(member.isEmpty()||member.size()>160||name.size()>160||ids.contains(member))return {};
        ids.insert(member);members<<QVariantMap{{"id",member},{"name",name.isEmpty()?QString("Participante"):name}};
    }
    const auto token=text(s,"join_session_token","joinSessionToken");
    auto owner=text(s,"session_owner_id","sessionOwnerId");if(owner.isEmpty())owner=text(s,"owner_id");
    return QVariantMap{{"id",id},{"active",true},{"members",members},
        {"host",s.value("is_session_owner").toBool(s.value("isSessionOwner").toBool(!viewer.isEmpty()&&owner==viewer))},
        {"link",atom(token,6,512)?"https://open.spotify.com/socialsession/"+token:QString()}};
}
inline QStringList joined(const QVariantMap &old,const QVariantMap &now){
    if(old.value("id")!=now.value("id"))return {};
    QSet<QString> before;for(const auto &v:old.value("members").toList())before.insert(v.toMap().value("id").toString());
    QStringList names;for(const auto &v:now.value("members").toList()){const auto m=v.toMap();if(!before.contains(m.value("id").toString()))names<<m.value("name").toString();}return names;
}
}
