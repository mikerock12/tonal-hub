#include "MusicCompanion.h"
#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QFile>
#include <algorithm>
#include <memory>
#include <QDate>

namespace {
QString normalized(QString text){return text.normalized(QString::NormalizationForm_D).remove(QRegularExpression("[\\x{0300}-\\x{036f}]")).toCaseFolded().simplified();}
QVariantList related(const QString &name,const QString &genres){
    QFile f(":/companion/resources/music-rules.json");f.open(QIODevice::ReadOnly);auto rules=QJsonDocument::fromJson(f.readAll()).object();QVariantList out;
    for(auto value:rules["recommendations"].toArray()){auto rule=value.toObject();bool match=false;
        for(auto g:rule["genres"].toArray())if(genres.contains(g.toString(),Qt::CaseInsensitive))match=true;
        for(auto a:rule["artists"].toArray())if(normalized(a.toString())==normalized(name))match=true;
        if(match)for(auto a:rule["artists"].toArray())if(normalized(a.toString())!=normalized(name)&&!out.contains(a.toString())&&out.size()<8)out<<a.toString();
    }return out;
}
}

QString MusicCompanion::memberAge(const QString &born,const QString &died,const QDate &today){
    bool ok=false;const int by=born.left(4).toInt(&ok);if(!ok||born.size()<4)return {};
    int ey=today.year(),em=today.month(),ed=today.day();
    if(died.size()>=4){ey=died.left(4).toInt();em=died.size()>=7?died.mid(5,2).toInt():12;ed=died.size()>=10?died.mid(8,2).toInt():31;}
    const bool exact=born.size()>=10;int years=ey-by;
    if(exact){const int bm=born.mid(5,2).toInt(),bd=born.mid(8,2).toInt();if(em<bm||(em==bm&&ed<bd))--years;}
    if(years<0||years>130)return {};
    const QString text=(exact?QString():QStringLiteral("cerca de "))+QString::number(years)+" anos";
    return died.size()>=4?" (aos "+text+")":" ("+text+")";
}

namespace {
QString role(const QString &value){
    static const QHash<QString,QString> names{{"lead vocals","vocal"},{"background vocals","vocal de apoio"},{"other vocals","vocal de apoio"},{"guitar","guitarra"},
        {"bass guitar","baixo"},{"bass","baixo"},{"drums (drum set)","bateria"},{"drums","bateria"},{"keyboard","teclado"},{"keyboards","teclado"},{"piano","piano"},
        {"rhythm guitar","guitarra base"},{"lead guitar","guitarra solo"},{"electric guitar","guitarra"},{"acoustic guitar","violão"},{"percussion","percussão"},
        {"synthesizer","sintetizador"},{"violin","violino"},{"saxophone","saxofone"},{"trumpet","trompete"}};
    return names.value(value,value);
}
QString nonNull(const QJsonValue &v){return v.isString()?v.toString():QString();}
}

QString MusicCompanion::memberLine(const QVariantMap &m,const QDate &today){
    QString line=m.value("name").toString();const auto roles=m.value("roles").toStringList();
    if(!roles.isEmpty())line+=" — "+roles.join(", ");
    const auto from=m.value("from").toString().left(4),to=m.value("to").toString().left(4);
    if(m.value("current").toBool())line+=from.isEmpty()?QStringLiteral(" · atual"):" · desde "+from;
    else if(!from.isEmpty()||!to.isEmpty())line+=" · "+(from.isEmpty()?QStringLiteral("?"):from)+"–"+(to.isEmpty()?QStringLiteral("?"):to);
    const auto born=m.value("born").toString(),died=m.value("died").toString();
    if(!born.isEmpty()){
        if(!died.isEmpty())line+=" · "+born.left(4)+"–"+died.left(4)+memberAge(born,died,today);
        else if(!m.value("deceased").toBool()){auto age=memberAge(born,{},today);age.remove(" (").remove(")");if(!age.isEmpty())line+=" · "+age;}
    }
    return line;
}

QVariantMap MusicCompanion::artistFacts(const QJsonObject &artist){
    QVariantList facts;const bool group=artist["type"]=="Group";
    const auto country=artist["country"].toString(),place=artist["begin-area"].toObject()["name"].toString(),area=artist["area"].toObject()["name"].toString();
    if(!place.isEmpty())facts<<"Origem: "+place+(country.isEmpty()?QString():", "+country);else if(!area.isEmpty())facts<<"Origem: "+area;else if(!country.isEmpty())facts<<"País: "+country;
    const auto life=artist["life-span"].toObject();const auto begin=nonNull(life["begin"]),end=nonNull(life["end"]);
    if(group){if(!begin.isEmpty())facts<<"Início: "+begin.left(4)+(end.isEmpty()?(life["ended"].toBool()?QString():QStringLiteral(" · em atividade")):" · encerrada em "+end.left(4));}
    else if(!begin.isEmpty())facts<<"Nascimento: "+begin+memberAge(begin,end,QDate::currentDate());
    QList<QJsonObject> tags;for(auto field:{"genres","tags"})for(auto v:artist[field].toArray())if(v.toObject()["count"].toInt()>0)tags<<v.toObject();
    std::stable_sort(tags.begin(),tags.end(),[](auto a,auto b){return a["count"].toInt()>b["count"].toInt();});
    QStringList genres;for(auto tag:tags)if(genres.size()<4&&!genres.contains(tag["name"].toString()))genres<<tag["name"].toString();
    if(!genres.isEmpty())facts<<"Estilos: "+genres.join(", ");
    if(!artist["disambiguation"].toString().isEmpty())facts<<artist["disambiguation"].toString();
    // Group members point to the band with direction "backward"; merge their relations by MBID.
    QStringList order;QHash<QString,QVariantMap> members;
    for(auto value:artist["relations"].toArray()){
        const auto rel=value.toObject();if(rel["type"]!="member of band")continue;
        const auto person=rel["artist"].toObject();const auto name=person["name"].toString(),id=person["id"].toString();if(name.isEmpty())continue;
        if(group&&rel["direction"].toString()!="backward")continue;
        const auto key=id.isEmpty()?name:id;
        if(!members.contains(key)){if(order.size()>=24)continue;order<<key;members[key]={{"id",id},{"name",name},{"roles",QStringList{}},{"current",false},{"original",false},{"from",QString()},{"to",QString()}};}
        auto &m=members[key];auto roles=m["roles"].toStringList();
        for(auto a:rel["attributes"].toArray()){const auto r=a.toString();if(r=="original"){m["original"]=true;continue;}const auto pt=role(r);if(!roles.contains(pt)&&roles.size()<4)roles<<pt;}
        m["roles"]=roles;if(!rel["ended"].toBool())m["current"]=true;
        const auto from=nonNull(rel["begin"]),to=nonNull(rel["end"]);
        if(!from.isEmpty()&&(m["from"].toString().isEmpty()||from<m["from"].toString()))m["from"]=from;
        if(!to.isEmpty()&&to>m["to"].toString())m["to"]=to;
    }
    QList<QVariantMap> sorted;for(const auto &k:order)sorted<<members[k];
    std::stable_sort(sorted.begin(),sorted.end(),[](const QVariantMap &a,const QVariantMap &b){
        if(a["current"].toBool()!=b["current"].toBool())return a["current"].toBool();
        if(a["original"].toBool()!=b["original"].toBool())return a["original"].toBool();
        const auto af=a["from"].toString().isEmpty()?QStringLiteral("9999"):a["from"].toString(),bf=b["from"].toString().isEmpty()?QStringLiteral("9999"):b["from"].toString();return af<bf;});
    QVariantList list;for(const auto &m:sorted)if(list.size()<8)list<<m;
    QString source;auto id=artist["id"].toString();if(QRegularExpression("^[a-fA-F0-9-]{36}$").match(id).hasMatch())source="https://musicbrainz.org/artist/"+id;
    return {{"facts",facts},{"genre",genres.join(", ")},{"url",source},{"members",list},{"memberTotal",int(sorted.size())},{"group",group}};
}

QString MusicCompanion::linkedArtist(const QJsonObject &url){
    QSet<QString> ids;for(auto v:url["relations"].toArray()){const auto rel=v.toObject();if(rel["target-type"]!="artist")continue;const auto id=rel["artist"].toObject()["id"].toString();if(QRegularExpression("^[a-fA-F0-9-]{36}$").match(id).hasMatch())ids.insert(id);}
    return ids.size()==1?*ids.begin():QString();
}

QString MusicCompanion::artistFromRelease(const QJsonObject &response,const QString &name,const QString &album){
    QSet<QString> ids;
    for(auto value:response["release-groups"].toArray()){
        auto release=value.toObject();if(normalized(release["title"].toString())!=normalized(album))continue;
        for(auto credit:release["artist-credit"].toArray()){auto artist=credit.toObject()["artist"].toObject();if(normalized(artist["name"].toString())==normalized(name)&&!artist["id"].toString().isEmpty())ids.insert(artist["id"].toString());}
    }return ids.size()==1?*ids.begin():QString();
}

void MusicCompanion::retryArtistNotes(){cancelArtistNotes();m_artistCache.clear();loadArtistNotes();}

void MusicCompanion::loadArtistNotes(){
    const auto name=m_current.value("artist").toString().trimmed();if(name.isEmpty())return;
    auto album=m_current.value("album").toString();album.remove(QRegularExpression("\\s*\\(.*?\\)|\\s*\\[.*?\\]"));album=album.trimmed();
    const auto spotifyArtist=m_current.value("artistId").toString();const bool spotify=QRegularExpression("^[A-Za-z0-9]{22}$").match(spotifyArtist).hasMatch();
    const auto identity=spotify?"spotify:"+spotifyArtist:name+"|"+album;
    if(m_artistName==identity)return;m_artistName=identity;const int generation=++m_artistGeneration;
    auto publish=[this,name,generation](QVariantMap result){if(generation!=m_artistGeneration)return;
        auto genres=result.value("genre").toString();result["related"]=related(name,genres.isEmpty()?m_current.value("genre").toString():genres);
        auto list=result["members"].toList();for(auto &v:list){auto m=v.toMap();m["line"]=memberLine(m,QDate::currentDate());v=m;}result["members"]=list;
        m_artistNotes=result;emit artistNotesChanged();};
    const auto cached=m_artistCache.value(identity);auto now=QDateTime::currentMSecsSinceEpoch();
    if(!cached.isEmpty()&&now-cached.value("at").toLongLong()<cached.value("ttl").toLongLong()){publish(cached);return;}
    publish({{"message","Consultando MusicBrainz…"},{"busy",true}});
    auto finish=[this,publish,identity](QVariantMap result){result["at"]=QDateTime::currentMSecsSinceEpoch();result["ttl"]=result.value("url").toString().isEmpty()?3600000LL:7LL*86400000;
        if(m_artistCache.size()>=32)m_artistCache.erase(m_artistCache.begin());m_artistCache[identity]=result;publish(result);};
    auto unavailable=[finish](int status){finish({{"message",status==503||status==429?"O MusicBrainz está sobrecarregado agora. Tente de novo em instantes; sua música continua tocando.":"Informações do artista temporariamente indisponíveis. Sua música continua tocando."},{"retry",true}});};
    // Member birth/death dates: up to six sequential lookups through the shared 1,25 s throttle.
    auto members=std::make_shared<std::function<void(QVariantMap,int,int)>>();
    *members=[this,generation,finish,publish,members](QVariantMap result,int index,int lookups){
        if(generation!=m_artistGeneration)return;
        auto list=result["members"].toList();
        while(index<list.size()&&!QRegularExpression("^[a-fA-F0-9-]{36}$").match(list[index].toMap()["id"].toString()).hasMatch())++index;
        if(index>=list.size()||lookups>=6){finish(result);return;}
        getArtistNote(QUrl("https://musicbrainz.org/ws/2/artist/"+list[index].toMap()["id"].toString()+"?fmt=json"),[this,generation,finish,members,result,index,lookups](int status,const QJsonDocument &doc)mutable{
            if(generation!=m_artistGeneration)return;
            if(status!=200){finish(result);return;}
            auto list=result["members"].toList();auto m=list[index].toMap();const auto life=doc.object()["life-span"].toObject();
            m["born"]=nonNull(life["begin"]);m["died"]=nonNull(life["end"]);m["deceased"]=life["ended"].toBool();list[index]=m;result["members"]=list;
            (*members)(result,index+1,lookups+1);
        });
    };
    auto detail=[this,generation,finish,publish,members,unavailable](const QString &id){
        if(id.isEmpty()){finish({{"message","A fonte não identificou um único artista com segurança. Nenhum fato foi atribuído a homônimos."}});return;}
        getArtistNote(QUrl("https://musicbrainz.org/ws/2/artist/"+id+"?fmt=json&inc=genres+tags+artist-rels"),[generation,this,publish,members,unavailable](int status,const QJsonDocument &details){if(generation!=m_artistGeneration)return;
            if(status!=200){unavailable(status);return;}
            auto facts=artistFacts(details.object());publish(facts);(*members)(facts,0,0);});
    };
    auto byName=[this,name,album,generation,detail,unavailable]{
        QUrl url("https://musicbrainz.org/ws/2/artist/");QUrlQuery query;auto literal=name;literal.remove('"');query.addQueryItem("fmt","json");query.addQueryItem("limit","8");query.addQueryItem("query","artist:\""+literal+"\"");url.setQuery(query);
        getArtistNote(url,[this,name,album,generation,detail,unavailable](int status,const QJsonDocument &doc){if(generation!=m_artistGeneration)return;
            if(status!=200){unavailable(status);return;}
            QJsonObject artist;bool ambiguous=false;for(auto value:doc.object()["artists"].toArray()){auto a=value.toObject();if(normalized(a["name"].toString())!=normalized(name)||a["score"].toInt()<98)continue;if(!artist.isEmpty())ambiguous=true;artist=a;}
            if(!ambiguous&&!artist.isEmpty()){detail(artist["id"].toString());return;}
            if(album.isEmpty()){detail({});return;}
            QUrl url("https://musicbrainz.org/ws/2/release-group/");QUrlQuery q;q.addQueryItem("fmt","json");q.addQueryItem("limit","10");auto a=name,b=album;a.remove('"');b.remove('"');q.addQueryItem("query","artist:\""+a+"\" AND releasegroup:\""+b+"\"");url.setQuery(q);
            getArtistNote(url,[name,album,detail](int status,const QJsonDocument &doc){detail(status==200?artistFromRelease(doc.object(),name,album):QString());});
        });
    };
    // Debounce rapid next/previous gestures before sending a new artist query.
    QTimer::singleShot(250,this,[this,generation,spotify,spotifyArtist,detail,byName,unavailable]{if(generation!=m_artistGeneration)return;
        if(!spotify){byName();return;}
        // Exact identity: the Spotify artist link registered in MusicBrainz (lookup, not search).
        QUrl url("https://musicbrainz.org/ws/2/url");QUrlQuery q;q.addQueryItem("fmt","json");q.addQueryItem("inc","artist-rels");q.addQueryItem("resource","https://open.spotify.com/artist/"+spotifyArtist);url.setQuery(q);
        getArtistNote(url,[this,generation,detail,byName,unavailable](int status,const QJsonDocument &doc){if(generation!=m_artistGeneration)return;
            const auto id=status==200?linkedArtist(doc.object()):QString();
            if(!id.isEmpty())detail(id);else if(status==200||status==404)byName();else unavailable(status);});
    });
}

void MusicCompanion::getArtistNote(const QUrl &url,std::function<void(int,const QJsonDocument &)> done){int generation=m_artistGeneration;get(url,std::move(done),[this,generation]{return generation==m_artistGeneration;});}
