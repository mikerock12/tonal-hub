#include "TextPlaylistCore.hpp"
#include <algorithm>
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
double similarity(const std::string &a,const std::string &b) {
    const auto x=normalize(a),y=normalize(b);
    if(x.empty()||y.empty())return 0;
    if(x==y)return 1;
    std::set<std::string> aa,bb;std::string word;
    std::istringstream ax(x),by(y);while(ax>>word)aa.insert(word);while(by>>word)bb.insert(word);
    int intersection=0;for(const auto &w:aa)intersection+=bb.count(w)!=0;
    return double(intersection)/double(aa.size()+bb.size()-intersection);
}
bool contains(const std::string &s,const std::string &word) {
    return (" "+normalize(s)+" ").find(" "+word+" ")!=std::string::npos;
}
std::string artistName(const std::string &s) {
    return std::regex_replace(normalize(s),std::regex(R"(^(?:the|a|an|o|os|as) )"),"");
}
std::string titleBase(const std::string &s) {
    auto value=normalize(s);
    static const std::regex suffix(R"(\s+(?:(?:\d{4} )?remaster(?:ed|izado|izada)?|deluxe(?: edition)?|expanded edition|bonus track|radio edit|single version|album version|mono|stereo)$)",std::regex::icase);
    while(true){auto next=std::regex_replace(value,suffix,"");if(next==value)break;value=std::move(next);}
    return trim(value);
}
double artistSimilarity(const std::string &requested,const std::string &credited) {
    double score=similarity(artistName(requested),artistName(credited));
    static const std::regex split(R"(\s*(?:,|;|\s+feat\.?\s+|\s+featuring\s+|\s+with\s+)\s*)",std::regex::icase);
    std::sregex_token_iterator part(credited.begin(),credited.end(),split,-1),end;
    for(;part!=end;++part)if(!part->str().empty())score=std::max(score,similarity(artistName(requested),artistName(part->str())));
    return score;
}
double titleSimilarity(const std::string &requested,const std::string &candidate) {
    return std::max(similarity(normalize(requested),normalize(candidate)),.995*similarity(titleBase(requested),titleBase(candidate)));
}
}
std::string normalize(const std::string &s) {
    std::string result;bool space=true;
    for(size_t i=0;i<s.size();) {
        const size_t start=i;unsigned char c=s[i++];char32_t cp=c;
        int extra=c>=0xf0?3:c>=0xe0?2:c>=0xc0?1:0;
        if(extra){cp=c&((1<<(6-extra))-1);for(int n=0;n<extra&&i<s.size();++n)cp=(cp<<6)|(static_cast<unsigned char>(s[i++])&63);}
        if(cp>=0x300&&cp<=0x36f)continue;
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
std::vector<Ranked> rank(const Song &song,const std::vector<Candidate> &candidates) {
    std::vector<Ranked> result;
    for(int i=0;i<int(candidates.size());++i) {
        const auto &c=candidates[i];const double a=song.artist.empty()?.8:artistSimilarity(song.artist,c.artist),t=titleSimilarity(song.title,c.title);
        double penalty=0;for(const auto *word:{"karaoke","tribute","cover","instrumental"})
            if((contains(c.title,word)||contains(c.artist,word))&&!contains(song.title+" "+song.artist,word))penalty+=.4;
        for(const auto *word:{"live","remix","acoustic","unplugged","demo"}){
            if(contains(song.title,word)&&!contains(c.title,word))penalty+=.25;
            else if(!contains(song.title,word)&&contains(c.title,word))penalty+=.12;
        }
        double score=c.popularity<0?a*.5+t*.5:a*.475+t*.475+std::clamp(c.popularity,0,100)*.0005;
        score-=penalty;result.push_back({i,score,a,t,a>=.7&&t>=.7&&penalty==0});
    }
    std::stable_sort(result.begin(),result.end(),[](const Ranked &a,const Ranked &b){return a.score>b.score;});
    if(result.size()>1&&result[0].score-result[1].score<.04 &&
       (artistName(candidates[result[0].index].artist)!=artistName(candidates[result[1].index].artist)||
        titleSimilarity(candidates[result[0].index].title,candidates[result[1].index].title)<.7))result[0].highConfidence=false;
    return result;
}
std::vector<std::string> queries(const Song &s) {
    auto escaped=[](std::string v){v.erase(std::remove(v.begin(),v.end(),'"'),v.end());return v;};
    std::vector<std::string> out;
    auto add=[&](std::string q){q=trim(q);if(!q.empty()&&std::find(out.begin(),out.end(),q)==out.end())out.push_back(q);};
    if(!s.artist.empty())add("track:\""+escaped(s.title)+"\" artist:\""+escaped(s.artist)+"\"");
    add(s.artist+" "+s.title);
    const auto base=titleBase(s.title);if(base!=normalize(s.title))add(s.artist+" "+base);
    add("track:\""+escaped(s.title)+"\"");return out;
}
}
