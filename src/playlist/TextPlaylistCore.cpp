#include "TextPlaylistCore.hpp"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace tonal::playlist {
namespace {
std::string trim(std::string s) {
    auto blank=[](unsigned char c){return std::isspace(c)!=0;};
    while(!s.empty()&&blank(s.front()))s.erase(s.begin());
    while(!s.empty()&&blank(s.back()))s.pop_back();
    return s;
}
std::string clean(std::string s) {
    s=trim(std::regex_replace(s,std::regex("[\\t\\r\\n ]+")," "));
    for(const std::string quote : {"\"","'","“","”","«","»"}) {
        if(s.starts_with(quote))s.erase(0,quote.size());
        if(s.ends_with(quote))s.erase(s.size()-quote.size());
    }
    return trim(s);
}
std::pair<std::string,bool> strip(std::string s) {
    static const std::regex number(R"(^\s*(?:\d+[.)-]|\(\d+\)|\[\d+\]|\d+:(?!\d))\s*)");
    static const std::regex leading(R"(^\s*(?:\[|\()?\s*\d{1,2}:\d{2}(?::\d{2})?\s*(?:\]|\))?\s*[-:|. ]*\s*)");
    static const std::regex trailing(R"(\s*[-:|. ]*\s*(?:\[|\()?\d{1,2}:\d{2}(?::\d{2})?\s*(?:\]|\))?\s*$)");
    s=std::regex_replace(trim(s),number,"");
    const bool timestamp=std::regex_search(s,leading)||std::regex_search(s,trailing);
    s=std::regex_replace(s,leading,"");
    s=std::regex_replace(s,number,"");
    s=std::regex_replace(s,trailing,"");
    s=std::regex_replace(s,std::regex(R"(^[-*|~ ]+|[-*|~ ]+$)"),"");
    return {trim(s),timestamp};
}
bool ignored(const std::string &line) {
    const auto s=normalize(line);
    static const std::regex header(R"(^(set list|setlist|tracklist|track list|playlist|faixas|musicas|songs|tracks)( .*)?$)");
    return s.empty()||std::regex_match(s,header)||line.starts_with("https://")||line.starts_with("http://")||
           std::regex_search(s,std::regex(R"(^[0-9., ]+ (views|visualizacoes|subscribers|inscritos)\b)"));
}
bool showTitle(const std::string &s) {
    const auto n=normalize(s);
    return std::regex_search(n,std::regex(R"(\b(full (concert|show|set|album)|live (at|in|from|hd|session)|ao vivo|show completo|unplugged|from the basement|setlist)\b)"));
}
std::vector<std::string> words(const std::string &normalized) {
    std::vector<std::string> out;std::istringstream in(normalized);std::string w;while(in>>w)out.push_back(w);return out;
}
double similarity(const std::string &a,const std::string &b) {
    const auto x=normalize(a),y=normalize(b);
    if(x.empty()||y.empty())return 0;
    if(x==y)return 1;
    std::set<std::string> aa,bb;for(auto &w:words(x))aa.insert(w);for(auto &w:words(y))bb.insert(w);
    int intersection=0;for(const auto &w:aa)intersection+=bb.count(w)!=0;
    return double(intersection)/double(aa.size()+bb.size()-intersection);
}
std::string compact(std::string s){s.erase(std::remove(s.begin(),s.end(),' '),s.end());return s;}
int distance(const std::string &a,const std::string &b) {
    if(a.size()>80||b.size()>80)return 99;
    std::vector<int> row(b.size()+1);for(size_t j=0;j<=b.size();++j)row[j]=int(j);
    for(size_t i=1;i<=a.size();++i){int diag=row[0];row[0]=int(i);for(size_t j=1;j<=b.size();++j){int up=row[j];row[j]=std::min({row[j]+1,row[j-1]+1,diag+(a[i-1]==b[j-1]?0:1)});diag=up;}}
    return row[b.size()];
}
std::string artistName(const std::string &s) {
    static const std::regex article(R"(^(?:the|a|an|o|os|as) )");
    return std::regex_replace(normalize(s),article,"");
}
std::string escaped(std::string v){v.erase(std::remove(v.begin(),v.end(),'"'),v.end());return trim(v);}
bool validId(const std::string &id){return id.size()==22&&std::all_of(id.begin(),id.end(),[](unsigned char c){return std::isalnum(c)!=0;});}

// ---- qualifiers --------------------------------------------------------------------------
enum class Kind { Unknown, Essential, Annotation, Edition, Version };
struct Qualifier { Kind kind=Kind::Unknown; Version version=Version::Studio; bool cover=false; };
bool has(const std::string &n,const std::regex &pattern){return std::regex_search(n,pattern);}
Qualifier classify(const std::string &text) {
    // Compiled once: classify runs for every candidate of every decision.
    static const std::regex karaoke(R"(\b(karaoke|in the style of|originally performed|made famous|backing track|tribute)\b)"),cover(R"(\bcover\b)"),
        essential(R"(^(part|pt|parte|vol|volume|chapter|capitulo|act|movement|no) ?\w+)"),essentialWord(R"(\b(reprise|intro|outro|interlude|prelude|overture|medley|suite)\b)"),
        numeral(R"(\d{1,3}|[ivx]+)"),rerecord(R"(\b(re ?record(ed|ing)?|taylors version|reimagined|re imagined)\b)"),
        live(R"(\b(live|ao vivo|en vivo|en directo|in concert|concert|sessions?)\b)"),acoustic(R"(\b(acoustic|acustic[oa]|unplugged|stripped)\b)"),
        demo(R"(\b(demo|early version|rough mix|work tape|home recording|4 track)\b)"),instrumental(R"(\binstrumental\b)"),
        plainMix(R"(\b(original|album|mono|stereo|single) mix\b)"),remix(R"(\b(remix|rmx|rework|dub|slowed|sped up|speed up|reverb|nightcore|8d|bootleg|mashup|mix)\b)"),
        edit(R"(\b(radio edit|single edit|edit|radio version|radio mix|short version)\b)"),
        edition(R"(\b(remaster(ed)?|remasterizad[oa]|deluxe|expanded|anniversary|edition|bonus( track)?|mono|stereo|explicit|clean|single version|album version|original version|original mix|album mix|mono mix|stereo mix|single mix)\b)"),
        year(R"((19|20)\d\d( .*)?)"),credit(R"(^(feat|ft|featuring|with|com)\b)"),from(R"(^(from|theme from|tema de|do filme|da novela|trilha)\b)"),
        alternate(R"(\b(alternate|alt|take \d+|outtake|rehearsal|version|versao|orchestral|symphonic|piano|strings|lullaby)\b)");
    const auto n=normalize(text);Qualifier q;
    if(n.empty()){q.kind=Kind::Annotation;return q;}
    auto version=[&](Version v,bool coverMarker=false){q.kind=Kind::Version;q.version=v;q.cover=coverMarker;return q;};
    if(has(n,karaoke))return version(Version::Karaoke,true);
    if(has(n,cover))return version(Version::Cover,true);
    if(has(n,essential)||has(n,essentialWord)||std::regex_match(n,numeral)){q.kind=Kind::Essential;return q;}
    if(has(n,rerecord))return version(Version::ReRecording);
    if(has(n,live))return version(Version::Live);
    if(has(n,acoustic))return version(Version::Acoustic);
    if(has(n,demo))return version(Version::Demo);
    if(has(n,instrumental))return version(Version::Instrumental);
    if(!has(n,plainMix)&&has(n,remix))return version(Version::Remix);
    if(has(n,edit))return version(Version::Edit);
    if(has(n,edition)||std::regex_match(n,year)){q.kind=Kind::Edition;return q;}
    if(has(n,credit)||has(n,from)){q.kind=Kind::Annotation;return q;}
    if(has(n,alternate))return version(Version::Alternate);
    return q;   // unknown credit/subtitle, e.g. "(New Order)": equivalent annotation
}
int strength(Version v){
    switch(v){case Version::Karaoke:return 10;case Version::Cover:return 9;case Version::Live:return 8;case Version::Acoustic:return 7;case Version::Demo:return 6;
        case Version::Remix:return 5;case Version::Instrumental:return 4;case Version::ReRecording:return 3;case Version::Edit:return 2;case Version::Alternate:return 1;default:return 0;}
}
void absorb(Title &t,const Qualifier &q,const std::string &text) {
    t.qualifiers.push_back(trim(text));
    if(q.kind==Kind::Version){if(strength(q.version)>strength(t.version))t.version=q.version;t.coverMarker|=q.cover;}
    else t.annotated=true;
}
std::string versionName(Version v){
    switch(v){case Version::Live:return "ao vivo";case Version::Acoustic:return "acústica";case Version::Demo:return "demo";case Version::Remix:return "remix";
        case Version::Instrumental:return "instrumental";case Version::Karaoke:return "karaokê/tributo";case Version::Cover:return "cover";case Version::ReRecording:return "regravação";
        case Version::Edit:return "edição para rádio";case Version::Alternate:return "versão alternativa";default:return "estúdio";}
}

// ---- comparison --------------------------------------------------------------------------
enum class TitleFit { Same, Plural, Typo, Prefix, Partial, Different };
TitleFit titleFit(const Title &req,const Title &cand) {
    if(req.core.empty()||cand.core.empty())return TitleFit::Different;
    if(req.core==cand.core||compact(req.core)==compact(cand.core))return TitleFit::Same;
    const auto a=words(req.core),b=words(cand.core);
    if(a.size()==b.size()){int diff=0;bool plural=true;
        for(size_t i=0;i<a.size();++i)if(a[i]!=b[i]){++diff;const auto &x=a[i].size()<b[i].size()?a[i]:b[i],&y=a[i].size()<b[i].size()?b[i]:a[i];
            plural&=x.size()>=3&&(y==x+"s"||y==x+"es");}
        if(diff==1&&plural)return TitleFit::Plural;}
    const auto ca=compact(req.core),cb=compact(cand.core);const size_t len=std::min(ca.size(),cb.size());
    const int d=distance(ca,cb);if((len>=12&&d<=2)||(len>=5&&d<=1))return TitleFit::Typo;
    if(cand.core.size()>req.core.size()&&cand.core.starts_with(req.core+" ")&&(a.size()>=2||req.core.size()>=6))return TitleFit::Prefix;
    return similarity(req.core,cand.core)>=.5?TitleFit::Partial:TitleFit::Different;
}
enum class ArtistFit { Unknown, ById, Primary, Featured, Homonym, Fuzzy, Different };
std::vector<std::string> names(const Candidate &c) {
    if(!c.artists.empty())return c.artists;
    std::vector<std::string> out;std::string s=c.artist(),part;std::istringstream in(s);while(std::getline(in,part,','))if(!trim(part).empty())out.push_back(trim(part));return out;
}
struct ArtistMatch { ArtistFit fit=ArtistFit::Different; std::string id, credited; };
ArtistMatch artistFit(const std::string &requested,const Candidate &c,const std::vector<ArtistHit> &hits,bool resolved) {
    ArtistMatch m;if(trim(requested).empty()){m.fit=ArtistFit::Unknown;return m;}
    const auto list=names(c);m.credited=c.artist();if(list.empty())return m;m.credited=list.front();
    int idx=-1;for(size_t k=0;k<list.size()&&idx<0;++k)if(sameArtist(requested,list[k]))idx=int(k);
    if(idx<0){
        // "A feat. B" / "A, B" / "A x B" requested: the first named performer must be credited.
        static const std::regex split(R"(\s*(?:,|;|\s+feat\.?\s+|\s+ft\.?\s+|\s+featuring\s+|\s+with\s+|\s+x\s+)\s*)",std::regex::icase);
        std::sregex_token_iterator part(requested.begin(),requested.end(),split,-1),end;
        if(part!=end){const std::string primary=part->str();if(!trim(primary).empty()&&normalize(primary)!=normalize(requested))
            for(size_t k=0;k<list.size()&&idx<0;++k)if(sameArtist(primary,list[k]))idx=int(k);}
    }
    if(idx>=0){
        m.credited=list[size_t(idx)];m.id=size_t(idx)<c.artistIds.size()?c.artistIds[size_t(idx)]:std::string();
        const bool byId=!m.id.empty()&&std::any_of(hits.begin(),hits.end(),[&](const ArtistHit &h){return h.id==m.id;});
        if(resolved&&!hits.empty()&&!m.id.empty()&&!byId){m.fit=ArtistFit::Homonym;return m;}
        m.fit=idx>0?ArtistFit::Featured:byId?ArtistFit::ById:ArtistFit::Primary;return m;
    }
    double best=0;for(const auto &n:list){const auto a=compact(artistName(requested)),b=compact(artistName(n));const size_t len=std::min(a.size(),b.size());
        const int d=distance(a,b);if((len>=8&&d<=2)||(len>=4&&d<=1))best=1;best=std::max(best,similarity(artistName(requested),artistName(n)));}
    m.fit=best>=.75?ArtistFit::Fuzzy:ArtistFit::Different;return m;
}
bool coverArtist(const Candidate &c){
    static const std::regex marker(R"(\b(karaoke|tribute|made famous|cover band|backing track|in the style)\b)");
    for(const auto &n:names(c))if(has(normalize(n),marker))return true;return false;
}
int priority(const Evaluation &e){
    switch(e.match){case Match::Exact:return 0;case Match::Equivalent:return e.safe?1:2;case Match::Ambiguous:return 3;case Match::Alternate:return 4;
        case Match::Unavailable:return 5;case Match::ArtistAmbiguous:return 6;case Match::Cover:return 7;case Match::WrongSong:return 8;default:return 9;}
}
int year(const std::string &date){if(date.size()<4)return 0;int y=0;for(int i=0;i<4;++i){if(!std::isdigit(static_cast<unsigned char>(date[size_t(i)])))return 0;y=y*10+(date[size_t(i)]-'0');}return y>=1900&&y<=2100?y:0;}
// "Part II", "Pt. 2", "pt 2" and "Part 2" share one key; roman numerals only after a part/volume word.
std::string canonicalNumbers(const std::string &core) {
    static const std::vector<std::string> romans={"i","ii","iii","iv","v","vi","vii","viii","ix","x","xi","xii","xiii","xiv","xv","xvi","xvii","xviii","xix","xx"};
    auto w=words(core);std::string out;
    for(size_t i=0;i<w.size();++i){
        auto &x=w[i];
        if(x=="pt")x="part";
        if(i>0&&(w[i-1]=="part"||w[i-1]=="vol"||w[i-1]=="volume"||w[i-1]=="chapter"||w[i-1]=="no")){
            const auto r=std::find(romans.begin(),romans.end(),x);if(r!=romans.end())x=std::to_string(r-romans.begin()+1);}
        out+=(out.empty()?"":" ")+x;
    }
    return out;
}
struct Scored { Evaluation e; std::string artistId, album; };
Scored evaluate(const Song &song,const Title &req,const Candidate &c,int index,const std::vector<ArtistHit> &hits,bool resolved,const Context &ctx) {
    Scored out;auto &e=out.e;e.index=index;out.album=c.album;const Title cand=title(c.title);auto artist=artistFit(song.artist,c,hits,resolved);out.artistId=artist.id;
    // Identity evidence beyond one search: profiles proven to be the same act, or the profile pinned for this list.
    const bool known=!artist.id.empty()&&(ctx.sameArtistIds.count(artist.id)||artist.id==ctx.preferredId);
    if(known&&(artist.fit==ArtistFit::Homonym||artist.fit==ArtistFit::Primary))artist.fit=ArtistFit::ById;
    const bool otherProfile=!ctx.preferredId.empty()&&!artist.id.empty()&&!known&&(artist.fit==ArtistFit::ById||artist.fit==ArtistFit::Primary||artist.fit==ArtistFit::Homonym);
    const auto fit=titleFit(req,cand);
    double artistScore=0;switch(artist.fit){case ArtistFit::ById:artistScore=1;break;case ArtistFit::Primary:artistScore=.95;break;case ArtistFit::Featured:artistScore=.6;break;
        case ArtistFit::Unknown:case ArtistFit::Fuzzy:artistScore=.5;break;case ArtistFit::Homonym:artistScore=.2;break;default:artistScore=.1*similarity(song.artist,c.artist());}
    double titleScore=0;switch(fit){case TitleFit::Same:titleScore=1;break;case TitleFit::Plural:titleScore=.96;break;case TitleFit::Typo:titleScore=.9;break;case TitleFit::Prefix:titleScore=.8;break;
        default:titleScore=.5*similarity(req.core,cand.core);}
    const int y=year(c.releaseDate);
    e.score=artistScore+titleScore+(cand.version==req.version?.1:0)+(cand.version==Version::Studio?.02:0)+(c.playable==1?.01:0)+
        (c.albumType=="album"?.03:c.albumType=="single"?.025:0)+std::clamp(c.popularity,0,100)*.0002+
        (!artist.id.empty()&&artist.id==ctx.preferredId?.05:0)+(y?(2100-y)*.00004:0);   // pinned/main profile, then the original (earliest) release
    const bool titleOk=fit==TitleFit::Same||fit==TitleFit::Plural||fit==TitleFit::Typo||fit==TitleFit::Prefix;
    const std::string who=artist.credited.empty()?c.artist():artist.credited;
    auto finish=[&](Match m,bool safe,std::string reason){e.match=m;e.safe=safe;e.reason=std::move(reason);return out;};
    if(otherProfile&&titleOk)return finish(Match::ArtistAmbiguous,false,"Outro cadastro chamado "+who+" no Spotify (álbum "+c.album+"); "+(ctx.preferredReason.empty()?"o cadastro fixado é outro":ctx.preferredReason));
    switch(artist.fit){
    case ArtistFit::Unknown:
        return finish(titleOk?Match::Ambiguous:Match::WrongSong,false,"Artista não informado na lista; confirme o intérprete");
    case ArtistFit::Different:case ArtistFit::Homonym:case ArtistFit::Fuzzy:
        if(cand.coverMarker||coverArtist(c))return finish(Match::Cover,false,"Regravação/karaokê por outro intérprete ("+who+")");
        if(artist.fit==ArtistFit::Fuzzy&&(fit==TitleFit::Same||fit==TitleFit::Plural))return finish(Match::Ambiguous,false,"Nome de artista parecido ("+who+"); confirme");
        if(artist.fit==ArtistFit::Homonym)return finish(titleOk?Match::ArtistAmbiguous:Match::WrongArtist,false,"Cadastro do Spotify com o mesmo nome ("+who+", álbum "+c.album+"), não confirmado pela busca do artista; confirme o intérprete");
        return finish(Match::WrongArtist,false,std::string(fit==TitleFit::Same?"Mesmo título, ":titleOk?"Título parecido, ":"")+"artista não corresponde ("+who+")");
    case ArtistFit::Featured:
        return finish(titleOk?Match::Ambiguous:Match::WrongSong,false,"O artista pedido aparece só como participação (principal: "+c.artist()+")");
    default:break;
    }
    std::string confirmed=artist.fit==ArtistFit::ById?"Artista confirmado (ID Spotify)":"Artista confirmado pelo nome";
    if(!artist.id.empty()&&artist.id==ctx.preferredId&&!ctx.preferredReason.empty())confirmed+=" — "+ctx.preferredReason;
    if(!artist.id.empty()&&ctx.sameArtistIds.count(artist.id)&&!ctx.sameArtistNote.empty())confirmed+="; "+ctx.sameArtistNote;
    if(!titleOk)return finish(Match::WrongSong,false,"Mesmo artista, outra música ("+c.title+")");
    if(cand.coverMarker&&!req.coverMarker)return finish(Match::Cover,false,"Karaokê/cover no título; não é a gravação pedida");
    Match m;bool safe;std::string reason;
    if(cand.version!=req.version){
        m=Match::Alternate;safe=false;reason=confirmed+"; versão "+versionName(cand.version)+" (pedido: "+versionName(req.version)+")";
    } else if(fit==TitleFit::Same){
        const bool exact=req.normalized==cand.normalized||(req.qualifiers.empty()&&cand.qualifiers.empty()&&canonicalNumbers(req.normalized)==canonicalNumbers(cand.normalized));
        m=exact?Match::Exact:Match::Equivalent;safe=true;
        const std::string note=!cand.qualifiers.empty()?cand.qualifiers.front():!req.qualifiers.empty()?req.qualifiers.front():c.title;
        reason=confirmed+(exact?"; título idêntico":"; título equivalente ("+note+")");
    } else if(fit==TitleFit::Plural){m=Match::Equivalent;safe=true;reason=confirmed+"; título equivalente (singular/plural)";}
    else if(fit==TitleFit::Typo){m=Match::Equivalent;safe=false;reason=confirmed+"; grafia parecida ("+c.title+"); confirme";}
    else {m=Match::Equivalent;safe=false;reason=confirmed+"; título começa com o pedido ("+c.title+"); confirme a composição";}
    if(c.playable==0){m=Match::Unavailable;safe=false;reason+="; indisponível na região da conta"+(c.restriction.empty()?std::string():" ("+c.restriction+")");}
    else if(!c.linkedFrom.empty())reason+="; faixa relinkada pelo Spotify";
    return finish(m,safe,reason);
}
}

std::string Candidate::artist() const {
    std::string out;for(const auto &a:artists){if(!out.empty())out+=", ";out+=a;}return out;
}
std::string normalize(const std::string &s) {
    std::string result;bool space=true;
    for(size_t i=0;i<s.size();) {
        const size_t start=i;unsigned char c=s[i++];char32_t cp=c;
        int extra=c>=0xf0?3:c>=0xe0?2:c>=0xc0?1:0;
        if(extra){cp=c&((1<<(6-extra))-1);for(int n=0;n<extra&&i<s.size();++n)cp=(cp<<6)|(static_cast<unsigned char>(s[i++])&63);}
        if(cp>=0x300&&cp<=0x36f)continue;
        // Apostrophes never split a word: "Baby’s" == "Baby's" == "Babys".
        if(cp=='\''||cp=='`'||cp==0xb4||cp==0x2bc||cp==0x2018||cp==0x2019)continue;
        if(cp=='&'){if(!space)result+=' ';result+="and ";space=true;continue;}
        static constexpr std::u32string_view groups[]={U"ÀÁÂÃÄÅàáâãäå",U"ÈÉÊËèéêë",U"ÌÍÎÏìíîï",U"ÒÓÔÕÖòóôõö",U"ÙÚÛÜùúûü",U"Çç",U"Ññ",U"ÝŸýÿ"};
        const char replacements[]="aeioucny";
        for(int g=0;g<8;++g)if(groups[g].find(cp)!=std::u32string::npos){cp=replacements[g];break;}
        if(cp<128) {
            if(std::isalnum(static_cast<unsigned char>(cp))){result+=char(std::tolower(static_cast<unsigned char>(cp)));space=false;}
            else if(!space){result+=' ';space=true;}
        } else if(cp<0x2000 || (cp>=0x2e80&&cp<=0x9fff)){result.append(s,start,i-start);space=false;}
        else if(!space){result+=' ';space=true;}
    }
    return trim(result);
}
Title title(const std::string &text) {
    Title t;t.original=text;t.normalized=normalize(text);
    std::string core;
    for(size_t i=0;i<text.size();) {
        const char c=text[i];
        if(c=='('||c=='[') {
            const auto end=text.find(c=='('?')':']',i+1);
            if(end==std::string::npos){core+=text.substr(i);break;}
            const std::string inner=text.substr(i+1,end-i-1);const auto q=classify(inner);
            // A leading group is part of the name: "(Don't Fear) The Reaper".
            if(trim(core).empty()||q.kind==Kind::Essential)core+=" "+inner+" ";else absorb(t,q,inner);
            i=end+1;continue;
        }
        core+=c;++i;
    }
    for(const std::string dash:{" - "," – "," — "}) {
        const auto pos=core.find(dash);if(pos==std::string::npos||pos==0)continue;
        const auto tail=core.substr(pos+dash.size());const auto q=classify(tail);
        if(q.kind!=Kind::Unknown&&q.kind!=Kind::Essential){absorb(t,q,tail);core.erase(pos);}
        break;
    }
    static const std::regex feat(R"(\s+(?:feat\.?|ft\.|featuring)\s+.*$)",std::regex::icase);
    std::smatch m;if(std::regex_search(core,m,feat)&&m.position(0)>0){absorb(t,Qualifier{Kind::Annotation},m.str(0));core.erase(size_t(m.position(0)));}
    t.core=canonicalNumbers(normalize(core));if(t.core.empty())t.core=canonicalNumbers(t.normalized);
    return t;
}
bool sameArtist(const std::string &requested,const std::string &credited) {
    const auto a=artistName(requested),b=artistName(credited);
    return !a.empty()&&(a==b||compact(a)==compact(b));
}
const char *code(Match m) {
    switch(m){case Match::Exact:return "EXACT_MATCH";case Match::Equivalent:return "EQUIVALENT_MATCH";case Match::Alternate:return "ALTERNATE_VERSION";
        case Match::Cover:return "COVER_OR_OTHER_ARTIST";case Match::WrongArtist:return "WRONG_ARTIST";case Match::WrongSong:return "WRONG_SONG";
        case Match::Ambiguous:return "AMBIGUOUS";case Match::NotFound:return "NOT_FOUND";case Match::Unavailable:return "UNAVAILABLE_IN_MARKET";
        case Match::LookupError:return "LOOKUP_ERROR";case Match::Incomplete:return "SEARCH_INCOMPLETE";case Match::ArtistAmbiguous:return "ARTIST_AMBIGUOUS";default:return "PENDING";}
}
Match matchFromCode(const std::string &value) {
    for(auto m:{Match::Exact,Match::Equivalent,Match::Alternate,Match::Cover,Match::WrongArtist,Match::WrongSong,Match::Ambiguous,Match::NotFound,Match::Unavailable,Match::LookupError,Match::Incomplete,Match::ArtistAmbiguous})
        if(value==code(m))return m;
    return Match::Pending;
}
std::string label(Match m) {
    switch(m){case Match::Exact:return "Encontrada";case Match::Equivalent:return "Equivalente";case Match::Alternate:return "Versão alternativa";
        case Match::Cover:return "Cover / outro intérprete";case Match::WrongArtist:return "Artista errado";case Match::WrongSong:return "Outra música do artista";
        case Match::Ambiguous:return "Ambígua";case Match::NotFound:return "Não encontrada";case Match::Unavailable:return "Indisponível na região";
        case Match::LookupError:return "Erro na busca";case Match::Incomplete:return "Busca incompleta";case Match::ArtistAmbiguous:return "Artista ambíguo";default:return "Aguardando";}
}
bool requestedArtistResult(Match m){return m==Match::Exact||m==Match::Equivalent||m==Match::Alternate||m==Match::Ambiguous||m==Match::Unavailable||m==Match::ArtistAmbiguous;}
const char *group(Match m){
    switch(m){case Match::Exact:case Match::Equivalent:case Match::Ambiguous:return "requested";case Match::Alternate:case Match::Unavailable:return "versions";
        case Match::ArtistAmbiguous:return "homonyms";default:return "other";}
}
Decision decide(const Song &song,const std::vector<Candidate> &candidates,const std::vector<ArtistHit> &artists,bool resolved,const Context &ctx) {
    Decision d;const Title req=title(song.title);std::vector<Scored> all;
    for(int i=0;i<int(candidates.size());++i)all.push_back(evaluate(song,req,candidates[size_t(i)],i,artists,resolved,ctx));
    // Two profiles with the requested name both offering the song, not proven to be the same act and none pinned: ask.
    // Extra credited names (band members) are not evidence of another artist: only profile IDs are compared.
    std::map<std::string,std::string> profiles;   // canonical profile -> album
    for(const auto &s:all)if(s.e.safe&&!s.artistId.empty())profiles.emplace(ctx.sameArtistIds.count(s.artistId)?std::string("same"):s.artistId,s.album);
    if(profiles.size()>1&&artists.size()>1&&ctx.preferredId.empty()){
        std::string albums;for(const auto &[id,album]:profiles)albums+=(albums.empty()?"":" e ")+album;
        for(auto &s:all)if(s.e.safe){s.e.safe=false;s.e.match=Match::ArtistAmbiguous;
            s.e.reason="Dois cadastros chamados "+song.artist+" no Spotify têm esta música (álbuns "+albums+") e as discografias não confirmam que são o mesmo artista; escolha a gravação ou fixe o cadastro para a lista";}
    }
    std::stable_sort(all.begin(),all.end(),[](const Scored &a,const Scored &b){const int pa=priority(a.e),pb=priority(b.e);return pa!=pb?pa<pb:a.e.score>b.e.score+1e-9;});
    for(auto &s:all)d.ordered.push_back(s.e);
    if(d.ordered.empty()){d.match=Match::NotFound;d.reason="Nenhum resultado no catálogo consultado";return d;}
    const auto &best=d.ordered.front();
    if(best.safe){d.match=best.match;d.choice=0;d.autoSelect=true;d.reason=best.reason;return d;}
    if(best.match==Match::WrongArtist||best.match==Match::WrongSong||best.match==Match::Cover){
        int others=0;for(const auto &e:d.ordered)others+=e.match==Match::WrongArtist||e.match==Match::Cover;
        d.match=Match::NotFound;d.choice=-1;
        d.reason=song.artist.empty()?"Nenhuma correspondência confiável":"Nenhuma gravação de "+song.artist+" com esse título";
        if(others)d.reason+="; "+std::to_string(others)+" resultado(s) de outros artistas separados";
        return d;
    }
    d.match=best.match;d.choice=0;d.reason=best.reason;return d;
}
Parsed parse(const std::string &text,int format,const std::string &defaultArtist) {
    if(text.size()>128*1024)throw std::length_error("Texto acima de 128 KiB");
    Parsed out;std::vector<std::string> lines;std::istringstream input(text);std::string line;
    while(std::getline(input,line)) {
        line=trim(line);if(line.empty())continue;
        for(const std::string dash:{"–","—"})for(size_t p=0;(p=line.find(dash,p))!=std::string::npos;p+=1)line.replace(p,dash.size(),"-");
        if(line.starts_with("•"))line=trim(line.substr(std::string("•").size()));
        if(line.size()>2000)throw std::length_error("Uma linha ultrapassa 2000 bytes");
        std::smatch m;
        if(std::regex_match(line,m,std::regex(R"(^(?:[Bb]anda|[Aa]rtista|[Aa]rtist|[Bb]and)\s*:\s*(.+)$)"))){if(out.detectedArtist.empty())out.detectedArtist=clean(m[1]);continue;}
        if(ignored(line))continue;
        if(lines.empty()&&out.detectedArtist.empty()) {
            auto dash=line.find(" - ");if(dash!=std::string::npos&&showTitle(line.substr(dash+3))){out.detectedArtist=clean(line.substr(0,dash));continue;}
        }
        lines.push_back(line);if(lines.size()>500)throw std::length_error("Limite de 500 músicas por importação");
    }
    if(out.detectedArtist.empty()&&lines.size()>1) {
        bool timestamps=false;for(const auto &s:lines)timestamps|=strip(s).second;
        if(timestamps)for(bool last:{false,true}) {
            const auto &s=last?lines.back():lines.front();
            if(s.size()<70&&!strip(s).second&&s.find(" - ")==std::string::npos){out.detectedArtist=clean(s);if(last)lines.pop_back();else lines.erase(lines.begin());break;}
        }
    }
    const auto artist=clean(defaultArtist.empty()?out.detectedArtist:defaultArtist);
    for(const auto &raw:lines) {
        auto s=strip(raw).first;if(s.empty())continue;
        Song song{artist,s,raw};bool separated=false;
        for(const std::string separator:{" - "," – "," — ","\t"," : "," / "," | "," -- ",","}) {
            auto pos=s.find(separator);if(pos==std::string::npos)continue;
            auto a=clean(s.substr(0,pos)),b=clean(s.substr(pos+separator.size()));
            const bool reverse=format==2||(format==0&&!artist.empty()&&normalize(b)==normalize(artist)&&normalize(a)!=normalize(artist));
            song.artist=reverse?b:a;song.title=reverse?a:b;separated=true;break;
        }
        if(!separated){std::smatch by;if(std::regex_match(s,by,std::regex(R"(^(.+)\s+[Bb]y\s+(.+)$)"))){song.title=clean(by[1]);song.artist=clean(by[2]);}}
        if(song.artist.empty())song.artist=artist;
        song.artist=clean(song.artist);song.title=clean(song.title);
        if(!song.title.empty())out.songs.push_back(std::move(song));
    }
    return out;
}
std::vector<std::string> queries(const Song &s) {
    std::vector<std::string> out;
    auto add=[&](std::string q){q=trim(q);if(!q.empty()&&std::find(out.begin(),out.end(),q)==out.end())out.push_back(q);};
    const auto artist=escaped(s.artist),name=escaped(s.title);if(name.empty())return out;
    if(artist.empty()){add("track:\""+name+"\"");add(name);return out;}
    const Title t=title(s.title);
    add("track:\""+name+"\" artist:\""+artist+"\"");
    add(artist+" "+name);
    if(t.core!=t.normalized)add(artist+" "+t.core);
    // Conservative spelling variants: only reached while the reference is still unresolved.
    if(t.core.find(" and ")!=std::string::npos)add(artist+" "+std::regex_replace(t.core,std::regex(" and ")," & "));
    auto w=words(t.core);
    if(!w.empty()&&w.back().size()>=4){auto &last=w.back();if(last.ends_with("s"))last.pop_back();else last+="s";
        std::string toggled;for(const auto &x:w)toggled+=(toggled.empty()?"":" ")+x;add(artist+" "+toggled);}
    return out;
}

// ---- duplicates ------------------------------------------------------------------------------
std::string referenceKey(const Song &s){return artistName(s.artist)+"|"+title(s.title).core;}
std::string fingerprint(const std::vector<Song> &songs){
    // FNV-1a over the ordered reference keys: identical pasted lists give the same value.
    unsigned long long h=1469598103934665603ull;
    for(const auto &s:songs){for(unsigned char c:referenceKey(s)+"\n"){h^=c;h*=1099511628211ull;}}
    char out[17];std::snprintf(out,sizeof out,"%016llx",h);return out;
}

// ---- progressive lookup --------------------------------------------------------------------
Lookup::Lookup(Song song,std::string market,int budget,int discographyBudget):song_(std::move(song)),market_(std::move(market)),
    budget_(std::clamp(budget,1,40)),discoBudget_(std::clamp(discographyBudget,1,600)) {
    text_=queries(song_);
    if(trim(song_.artist).empty()){textIndex_=0;stage_=2;}
}
int Lookup::backoffMs() const{return retry_?(attempts_<=1?1500:4000):0;}
Request Lookup::make(Request::Kind kind,std::string path,std::vector<std::pair<std::string,std::string>> query,std::string label) {
    Request r;r.kind=kind;r.path=std::move(path);r.query=std::move(query);r.label=std::move(label);
    if(!market_.empty()&&kind!=Request::Artists)r.query.emplace_back("market",market_);
    ++used_;(kind==Request::Albums||kind==Request::AlbumTracks?discoUsed_:searchUsed_)++;attempts_=0;current_=r;return r;
}
bool Lookup::hasRequestedArtistTrack() const {
    for(const auto &c:pool_)for(const auto &n:names(c))if(sameArtist(song_.artist,n))return true;return false;
}
Context Lookup::effectiveContext() const {
    Context ctx=context_;
    // Two profiles with the requested name whose discographies share several titles are one act with a duplicated
    // profile (common after label/distributor changes), not homonyms. Compare full title sets, never credited names.
    for(size_t i=0;i<artists_.size();++i)for(size_t j=i+1;j<artists_.size();++j){
        const auto a=titlesById_.find(artists_[i].id),b=titlesById_.find(artists_[j].id);
        if(a==titlesById_.end()||b==titlesById_.end())continue;
        int shared=0;for(const auto &t:a->second)shared+=b->second.count(t)!=0;
        if(shared<3)continue;
        ctx.sameArtistIds.insert(artists_[i].id);ctx.sameArtistIds.insert(artists_[j].id);
        ctx.sameArtistNote="dois cadastros de "+song_.artist+" no Spotify têm "+std::to_string(shared)+" músicas em comum (mesmo artista; créditos adicionais não indicam outro intérprete)";
        if(ctx.preferredId.empty()){
            const auto ri=releasesById_.find(artists_[i].id),rj=releasesById_.find(artists_[j].id);
            const int ni=ri==releasesById_.end()?0:ri->second,nj=rj==releasesById_.end()?0:rj->second;
            ctx.preferredId=ni>=nj?artists_[i].id:artists_[j].id;
            ctx.preferredReason="cadastro principal ("+std::to_string(std::max(ni,nj))+" lançamentos)";
        }
    }
    return ctx;
}
bool Lookup::safeNow() {
    size_t titles=0;for(const auto &[id,t]:titlesById_)titles+=t.size();
    const long key=long(pool_.size())*64+long(artists_.size())*2+(resolved_?1:0)+long(titles)*100000+long(context_.preferredId.size());
    if(key!=safeKey_){safeKey_=key;safe_=decide(song_,pool_,artists_,resolved_,effectiveContext()).autoSelect;}
    return safe_;
}
Request Lookup::next() {
    if(retry_&&!finished_){retry_=false;++used_;(current_.kind==Request::Albums||current_.kind==Request::AlbumTracks?discoUsed_:searchUsed_)++;return current_;}
    while(!finished_) {
        if(used_>0&&(resolved_||trim(song_.artist).empty())&&safeNow()){finished_=true;break;}
        const bool disco=stage_==3||stage_==4;
        if(!disco&&searchUsed_>=budget_){incomplete_=stage_<7;finished_=true;break;}
        if(disco&&discoUsed_>=discoBudget_){discoCut_=true;stage_=6;continue;}
        switch(stage_) {
        case 0:stage_=1;if(text_.empty())continue;
            return make(Request::Tracks,"/search",{{"q",text_[0]},{"type","track"},{"limit","10"}},"busca estruturada (artista + música)");
        case 1:stage_=2;if(resolved_)continue;
            return make(Request::Artists,"/search",{{"q","artist:\""+escaped(song_.artist)+"\""},{"type","artist"},{"limit","10"}},"busca do artista");
        case 2:
            if(textIndex_<text_.size()){const auto q=text_[textIndex_++];return make(Request::Tracks,"/search",{{"q",q},{"type","track"},{"limit","10"}},"busca textual: "+q);}
            stage_=trim(song_.artist).empty()?7:3;continue;
        case 3: {
            if(!discoStarted_){
                discoStarted_=true;
                // The pinned profile first; at most three profiles with the requested name.
                for(const auto &a:artists_)if(a.id==context_.preferredId)discoIds_.push_back(a.id);
                for(const auto &a:artists_)if(a.id!=context_.preferredId&&discoIds_.size()<3)discoIds_.push_back(a.id);
            }
            if(discoArtist_>=discoIds_.size()){stage_=4;continue;}
            const auto &id=discoIds_[discoArtist_];const int cap=listAppears_?50:300;
            if((listTotal_>=0&&listOffset_>=listTotal_)||listOffset_>=cap){
                if(listTotal_<0||listOffset_<listTotal_)discoCut_=discoCut_||listOffset_>=cap;
                if(!listAppears_)listAppears_=true;else{listAppears_=false;++discoArtist_;}
                listOffset_=0;listTotal_=-1;continue;
            }
            std::string name;for(const auto &a:artists_)if(a.id==id)name=a.name;
            // Own releases page by page (album, single, compilation), then appears_on: a split or compilation by others
            // only counts when the track itself credits the requested artist as performer.
            return make(Request::Albums,"/artists/"+id+"/albums",{{"include_groups",listAppears_?"appears_on":"album,single,compilation"},{"limit","10"},{"offset",std::to_string(listOffset_)}},
                "discografia de "+name+(listAppears_?" (participações)":" (lançamentos)")+" a partir de "+std::to_string(listOffset_));
        }
        case 4:
            if(!albumsOrdered_){
                const auto core=title(song_.title).core;
                auto rank=[&](const AlbumHit &x){const auto n=title(x.name).core;if(n==core)return 0;if(n.find(core)!=std::string::npos)return 1;if(x.appearsOn)return 6;
                    return x.type=="album"?2:x.type=="compilation"?3:4;};
                std::stable_sort(albums_.begin(),albums_.end(),[&](const AlbumHit &a,const AlbumHit &b){return rank(a)<rank(b);});
                albumsOrdered_=true;
            }
            if(albumIndex_<albums_.size()){const auto a=albums_[albumIndex_];
                auto r=make(Request::AlbumTracks,"/albums/"+a.id+"/tracks",{{"limit","50"},{"offset",std::to_string(trackOffset_)}},"faixas de "+a.name);r.album=a;current_.album=a;return r;}
            exhausted_=!discoCut_;stage_=6;continue;
        case 6:stage_=7;if(hasRequestedArtistTrack()||searchUsed_>=budget_)continue;
            return make(Request::Tracks,"/search",{{"q","track:\""+escaped(song_.title)+"\""},{"type","track"},{"limit","10"}},"busca só pelo título (homônimos ficam separados)");
        default:finished_=true;break;
        }
    }
    return {};
}
std::vector<int> Lookup::acceptTracks(const std::vector<Candidate> &input,int total) {
    std::vector<int> map;const Title req=title(song_.title);const bool album=current_.kind==Request::AlbumTracks;
    for(auto c:input) {
        const auto key=c.uri.empty()?c.id:c.uri;
        if(key.empty()){map.push_back(-1);continue;}
        if(album){
            const auto t=title(c.title);if(titlesById_.size()<8&&titlesById_[current_.album.artistId].size()<4000)titlesById_[current_.album.artistId].insert(t.core);
            // An album lists every song of the artist: keep only titles related to the request, not the whole tracklist.
            if(titleFit(req,t)==TitleFit::Different){map.push_back(-1);continue;}
        }
        auto found=std::find_if(pool_.begin(),pool_.end(),[&](const Candidate &x){return (x.uri.empty()?x.id:x.uri)==key;});
        if(found!=pool_.end()){if(found->playable<0)found->playable=c.playable;
            if(album&&found->album.empty()){found->album=current_.album.name;found->albumType=current_.album.type;found->releaseDate=current_.album.releaseDate;}
            map.push_back(int(found-pool_.begin()));continue;}
        if(pool_.size()>=200){map.push_back(-1);continue;}
        if(album){if(c.album.empty())c.album=current_.album.name;if(c.albumType.empty())c.albumType=current_.album.type;if(c.releaseDate.empty())c.releaseDate=current_.album.releaseDate;}
        if(c.source.empty())c.source=current_.label;
        pool_.push_back(std::move(c));map.push_back(int(pool_.size())-1);
    }
    if(album){
        // Album tracks page by page (50 each); a release with more than 200 tracks is cut and reported.
        const int fetched=trackOffset_+int(input.size());
        if(total>fetched&&!input.empty()){if(trackOffset_+50<200)trackOffset_+=50;else{discoCut_=true;trackOffset_=0;++albumIndex_;++releasesChecked_;}}
        else{trackOffset_=0;++albumIndex_;++releasesChecked_;}
    }
    return map;
}
void Lookup::acceptArtists(const std::vector<ArtistHit> &hits) {
    for(const auto &h:hits)if(validId(h.id)&&sameArtist(song_.artist,h.name)&&artists_.size()<5&&
        std::none_of(artists_.begin(),artists_.end(),[&](const ArtistHit &a){return a.id==h.id;}))artists_.push_back(h);
    resolved_=true;
}
void Lookup::seedArtists(const std::vector<ArtistHit> &hits){artists_.clear();resolved_=false;acceptArtists(hits);}
void Lookup::acceptAlbums(const std::vector<AlbumHit> &hits,int total) {
    const std::string owner=discoArtist_<discoIds_.size()?discoIds_[discoArtist_]:std::string();
    for(auto h:hits){
        if(!validId(h.id)||albums_.size()>=400||std::any_of(albums_.begin(),albums_.end(),[&](const AlbumHit &a){return a.id==h.id;}))continue;
        h.artistId=owner;h.appearsOn=listAppears_;albums_.push_back(h);if(!listAppears_)++releasesById_[owner];
    }
    listOffset_+=10;
    if(total>=0)listTotal_=total;else if(hits.size()<10)listTotal_=listOffset_;
}
void Lookup::fail(int status,const std::string &message) {
    // Catalog gaps are not lookup errors: a missing album or a rejected filter only skips that step.
    if(current_.kind==Request::AlbumTracks&&(status==400||status==404)){trackOffset_=0;++albumIndex_;return;}
    if(current_.kind==Request::Albums&&(status==400||status==404)){listTotal_=listOffset_;return;}
    if(current_.kind==Request::Artists&&(status==400||status==404)){resolved_=true;return;}
    if(current_.kind==Request::Tracks&&status==400)return;
    // Transient failures (network, timeout, 5xx) are repeated twice with growing waits before becoming LOOKUP_ERROR.
    const bool transient=status<=0||status==500||status==502||status==503||status==504;
    if(transient&&attempts_<2&&current_.kind!=Request::None){++attempts_;retry_=true;return;}
    error_=status==0?-1:status;errorMessage_=message.empty()?"Falha de rede ou da API":message;finished_=true;
}
Decision Lookup::decision() const {
    Decision d=decide(song_,pool_,artists_,resolved_,effectiveContext());
    if(d.autoSelect)return d;
    if(error_){d.match=Match::LookupError;d.reason="Busca incompleta: "+errorMessage_+(pool_.empty()?"":"; resultados parciais mantidos para revisão");return d;}
    if(!finished_){d.match=Match::Pending;return d;}
    if(d.match!=Match::NotFound)return d;
    std::string who;for(const auto &id:discoIds_)for(const auto &a:artists_)if(a.id==id)who=a.name;
    if(discoStarted_&&!exhausted_){
        d.match=Match::Incomplete;
        d.reason+="; busca incompleta: discografia de "+who+" cortada pelo limite de consultas ("+std::to_string(releasesChecked_)+" de "+std::to_string(albums_.size())+" lançamentos conferidos); use Tentar de novo";
    } else if(exhausted_){
        // Only now the "discography checked" claim is true: every page of releases and of their tracks was read.
        d.reason+="; discografia completa conferida ("+std::to_string(releasesChecked_)+" lançamentos de "+who+", inclusive coletâneas e participações) no catálogo disponível para esta conta; pode estar indisponível na sua região";
    } else if(incomplete_){d.match=Match::Incomplete;d.reason+="; limite de consultas atingido";}
    else if(resolved_&&artists_.empty()&&!trim(song_.artist).empty())d.reason+="; nenhum artista com esse nome no catálogo disponível para esta conta (buscas pelo título concluídas)";
    return d;
}
}
