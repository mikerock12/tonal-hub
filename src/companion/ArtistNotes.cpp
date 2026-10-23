#include "MusicCompanion.h"
#include <QDateTime>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QFile>
#include <algorithm>

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

QVariantMap MusicCompanion::artistFacts(const QJsonObject &artist){
    QVariantList facts;const auto country=artist["country"].toString(),place=artist["begin-area"].toObject()["name"].toString();
    if(!place.isEmpty())facts<<"Origem: "+place+(country.isEmpty()?QString():" · "+country);else if(!country.isEmpty())facts<<"País: "+country;
    const auto life=artist["life-span"].toObject();
    if(!life["begin"].toString().isEmpty())facts<<(artist["type"]=="Group"?"Formação: ":"Nascimento: ")+life["begin"].toString();
    if(!life["end"].toString().isEmpty())facts<<"Fim do período registrado: "+life["end"].toString();
    QList<QJsonObject> tags;for(auto field:{"genres","tags"})for(auto v:artist[field].toArray())if(v.toObject()["count"].toInt()>0)tags<<v.toObject();
    std::stable_sort(tags.begin(),tags.end(),[](auto a,auto b){return a["count"].toInt()>b["count"].toInt();});
    QStringList genres,members;for(auto tag:tags)if(genres.size()<4&&!genres.contains(tag["name"].toString()))genres<<tag["name"].toString();
    if(!genres.isEmpty())facts<<"Estilos: "+genres.join(", ");
    if(!artist["disambiguation"].toString().isEmpty())facts<<artist["disambiguation"].toString();
    for(auto value:artist["relations"].toArray()){auto rel=value.toObject();auto name=rel["artist"].toObject()["name"].toString();if(rel["type"]=="member of band"&&!name.isEmpty()&&!members.contains(name)&&members.size()<6)members<<name;}
    if(!members.isEmpty())facts<<"Conexões / integrantes registrados: "+members.join(", ");
    QString source;auto id=artist["id"].toString();if(QRegularExpression("^[a-fA-F0-9-]{36}$").match(id).hasMatch())source="https://musicbrainz.org/artist/"+id;
    return {{"facts",facts},{"genre",genres.join(", ")},{"url",source}};
}

QString MusicCompanion::artistFromRelease(const QJsonObject &response,const QString &name,const QString &album){
    QSet<QString> ids;
    for(auto value:response["release-groups"].toArray()){
        auto release=value.toObject();if(normalized(release["title"].toString())!=normalized(album))continue;
        for(auto credit:release["artist-credit"].toArray()){auto artist=credit.toObject()["artist"].toObject();if(normalized(artist["name"].toString())==normalized(name)&&!artist["id"].toString().isEmpty())ids.insert(artist["id"].toString());}
    }return ids.size()==1?*ids.begin():QString();
}

void MusicCompanion::loadArtistNotes(){
    const auto name=m_current.value("artist").toString().trimmed();if(name.isEmpty())return;
    auto album=m_current.value("album").toString();album.remove(QRegularExpression("\\s*\\(.*?\\)|\\s*\\[.*?\\]"));album=album.trimmed();
    const auto identity=name+"|"+album;
    if(m_artistName==identity)return;m_artistName=identity;const int generation=++m_artistGeneration;
    auto publish=[this,name,generation](QVariantMap result){if(generation!=m_artistGeneration)return;
        auto genres=result.value("genre").toString();result["related"]=related(name,genres.isEmpty()?m_current.value("genre").toString():genres);m_artistNotes=result;emit artistNotesChanged();};
    const auto cached=m_artistCache.value(identity);auto now=QDateTime::currentMSecsSinceEpoch();
    if(!cached.isEmpty()&&now-cached.value("at").toLongLong()<cached.value("ttl").toLongLong()){publish(cached);return;}
    publish({{"message","Consultando MusicBrainz…"},{"busy",true}});
    auto finish=[this,publish,identity](QVariantMap result){result["at"]=QDateTime::currentMSecsSinceEpoch();result["ttl"]=result.value("url").toString().isEmpty()?3600000LL:7LL*86400000;
        if(m_artistCache.size()>=32)m_artistCache.erase(m_artistCache.begin());m_artistCache[identity]=result;publish(result);};
    auto detail=[this,generation,finish](const QString &id){
        if(id.isEmpty()){finish({{"message","A fonte não identificou um único artista com segurança. Nenhum fato foi atribuído a homônimos."}});return;}
        getArtistNote(QUrl("https://musicbrainz.org/ws/2/artist/"+id+"?fmt=json&inc=genres+tags+artist-rels"),[generation,this,finish](int status,const QJsonDocument &details){if(generation!=m_artistGeneration)return;if(status==200)finish(artistFacts(details.object()));else finish({{"message","Informações do artista temporariamente indisponíveis."}});});
    };
    // Debounce rapid next/previous gestures before sending a new artist query.
    QTimer::singleShot(250,this,[this,name,album,generation,finish,detail]{if(generation!=m_artistGeneration)return;
        QUrl url("https://musicbrainz.org/ws/2/artist/");QUrlQuery query;auto literal=name;literal.remove('"');query.addQueryItem("fmt","json");query.addQueryItem("limit","8");query.addQueryItem("query","artist:\""+literal+"\"");url.setQuery(query);
        getArtistNote(url,[this,name,album,generation,finish,detail](int status,const QJsonDocument &doc){if(generation!=m_artistGeneration)return;
            if(status!=200){finish({{"message","Informações do artista temporariamente indisponíveis. Sua música continua tocando."}});return;}
            QJsonObject artist;bool ambiguous=false;for(auto value:doc.object()["artists"].toArray()){auto a=value.toObject();if(normalized(a["name"].toString())!=normalized(name)||a["score"].toInt()<98)continue;if(!artist.isEmpty())ambiguous=true;artist=a;}
            if(!ambiguous&&!artist.isEmpty()){detail(artist["id"].toString());return;}
            if(album.isEmpty()){detail({});return;}
            QUrl url("https://musicbrainz.org/ws/2/release-group/");QUrlQuery q;q.addQueryItem("fmt","json");q.addQueryItem("limit","10");auto a=name,b=album;a.remove('"');b.remove('"');q.addQueryItem("query","artist:\""+a+"\" AND releasegroup:\""+b+"\"");url.setQuery(q);
            getArtistNote(url,[name,album,detail](int status,const QJsonDocument &doc){detail(status==200?artistFromRelease(doc.object(),name,album):QString());});
        });
    });
}

void MusicCompanion::getArtistNote(const QUrl &url,std::function<void(int,const QJsonDocument &)> done){int generation=m_artistGeneration;get(url,std::move(done),[this,generation]{return generation==m_artistGeneration;});}
