#include "ListeningMixCore.hpp"
#include "playlist/TextPlaylistCore.hpp"
#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <cmath>
namespace tonal::mix {
namespace {
std::set<std::string> tags(std::string s){for(auto& c:s)if(c==';'||c=='|')c=',';std::set<std::string> out;std::istringstream in(s);while(std::getline(in,s,',')){s=playlist::normalize(s);if(!s.empty())out.insert(s);}return out;}
double affinity(const Track&a,const Track&b){auto aa=tags(a.genres),bb=tags(b.genres);int common=0;for(const auto&t:aa)common+=bb.count(t)!=0;double score=common?3.+double(common)/std::max(aa.size(),bb.size()):0.;if(a.year&&b.year)score+=std::max(0.,1.-std::abs(a.year-b.year)/20.);if(a.themes&&b.themes){unsigned both=a.themes&b.themes;while(both){score+=.2;both&=both-1;}}return score;}
}
std::vector<int> order(int count,uint32_t seed){count=std::clamp(count,0,3000);std::vector<int> out(count);std::iota(out.begin(),out.end(),0);std::mt19937 rng(seed);for(int i=count-1;i>0;--i){const uint32_t bound=i+1,threshold=(UINT32_MAX-bound+1u)%bound;uint32_t draw;do{draw=rng();}while(draw<threshold);std::swap(out[i],out[draw%bound]);}if(count>1&&std::is_sorted(out.begin(),out.end()))std::rotate(out.begin(),out.begin()+1,out.end());return out;}
std::vector<int> plan(const std::vector<Track>& source,const std::vector<Track>& pool,int selected,bool random,bool discover,uint32_t seed,bool album){
 const int count=std::min(500,int(source.size()));if(count==0||selected<0||selected>=count)return {};
 // Playback draws independent random priorities, rather than reusing the visual
 // Fisher-Yates shuffle. Preserve a song explicitly chosen by the listener.
 std::vector<int> src;if(random){std::mt19937 playback(seed^0x9e3779b9u);std::vector<std::pair<uint64_t,int>> keys;for(int i=0;i<count;++i)if(i!=selected)keys.push_back({(uint64_t(playback())<<32)|playback(),i});std::sort(keys.begin(),keys.end());src.push_back(selected);for(auto key:keys)src.push_back(key.second);}else for(int i=selected;i<count;++i)src.push_back(i);
 std::set<std::string> ids,artists,albums;for(const auto&t:source){ids.insert(t.id);artists.insert(playlist::normalize(t.artist));if(!t.album.empty())albums.insert(t.album);}
 std::vector<std::pair<double,int>> ranked;std::mt19937 rng(seed^0x73d1ac9u);
 if(discover)for(int i=0;i<std::min(120,int(pool.size()));++i){const auto&t=pool[i];auto artist=playlist::normalize(t.artist);if(t.id.empty()||artist.empty()||ids.count(t.id)||artists.count(artist)||(album&&!t.album.empty()&&albums.count(t.album)))continue;double score=0;for(int j=0;j<std::min(12,count);++j)score=std::max(score,affinity(source[(selected+j)%count],t));if(score<1.1)continue;double weight=score*(t.artistHeard?1.:8.)*(t.heard?1.:2.);double u=(double(rng())+1.)/(double(UINT32_MAX)+2.);ranked.push_back({std::log(u)/weight,i});}
 std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first>b.first;});std::vector<int> outside;std::set<std::string> usedArtists;for(auto [score,i]:ranked){const auto&t=pool[i];if(!ids.insert(t.id).second||!usedArtists.insert(playlist::normalize(t.artist)).second)continue;outside.push_back(i);}
 std::vector<int> result;size_t next=0;int inside=0;for(size_t pos=0;pos<src.size();++pos){int i=src[pos];if(result.size()==500)break;result.push_back(i);if(++inside==2){inside=0;const size_t remaining=src.size()-pos-1;if(next<outside.size()&&result.size()+remaining<500)result.push_back(-outside[next++]-1);}}return result;
}
std::vector<std::string> queries(const std::string& genres,int year){std::vector<std::string> out;for(auto g:tags(genres)){if(out.size()>=6)break;g.erase(std::remove(g.begin(),g.end(),'"'),g.end());auto q="genre:\""+g+"\"";if(year>=1900&&year<=2100)out.push_back(q+" year:"+std::to_string(std::max(1900,year-8))+"-"+std::to_string(std::min(2100,year+8)));out.push_back(q);}return out;}
}
