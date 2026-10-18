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
std::vector<int> order(int count,uint32_t seed){count=std::clamp(count,0,3000);std::vector<int> out(count);std::iota(out.begin(),out.end(),0);std::mt19937 rng(seed);for(int i=count-1;i>0;--i)std::swap(out[i],out[rng()%(i+1)]);if(count>1&&std::is_sorted(out.begin(),out.end()))std::rotate(out.begin(),out.begin()+1,out.end());return out;}
std::vector<int> plan(const std::vector<Track>& source,const std::vector<Track>& pool,int selected,bool random,bool discover,uint32_t seed,bool album){
 const int count=std::min(500,int(source.size()));if(count==0||selected<0||selected>=count)return {};
 std::vector<int> src;if(random){src=order(count,seed);src.erase(std::find(src.begin(),src.end(),selected));src.insert(src.begin(),selected);}else for(int i=selected;i<count;++i)src.push_back(i);
 std::set<std::string> ids,artists,albums;for(const auto&t:source){ids.insert(t.id);artists.insert(playlist::normalize(t.artist));if(!t.album.empty())albums.insert(t.album);}
 std::vector<std::pair<double,int>> ranked;std::mt19937 rng(seed^0x73d1ac9u);
 if(discover)for(int i=0;i<std::min(120,int(pool.size()));++i){const auto&t=pool[i];auto artist=playlist::normalize(t.artist);if(t.id.empty()||artist.empty()||ids.count(t.id)||artists.count(artist)||(album&&!t.album.empty()&&albums.count(t.album)))continue;double score=0;for(int j=0;j<std::min(12,count);++j)score=std::max(score,affinity(source[(selected+j)%count],t));if(score<1.1)continue;score+=t.artistHeard?0:4.;score+=t.heard?0:1.;score+=(rng()%1000)/5000.;ranked.push_back({score,i});}
 std::sort(ranked.begin(),ranked.end(),[](auto a,auto b){return a.first>b.first;});std::vector<int> outside;std::set<std::string> usedArtists;for(auto [score,i]:ranked){const auto&t=pool[i];if(!ids.insert(t.id).second||!usedArtists.insert(playlist::normalize(t.artist)).second)continue;outside.push_back(i);}
 std::vector<int> result;size_t next=0;int inside=0;for(size_t pos=0;pos<src.size();++pos){int i=src[pos];if(result.size()==500)break;result.push_back(i);if(++inside==2){inside=0;const size_t remaining=src.size()-pos-1;if(next<outside.size()&&result.size()+remaining<500)result.push_back(-outside[next++]-1);}}return result;
}
std::vector<std::string> queries(const std::string& genres,int year){std::vector<std::string> out;for(auto g:tags(genres)){if(out.size()>=6)break;g.erase(std::remove(g.begin(),g.end(),'"'),g.end());auto q="genre:\""+g+"\"";if(year>=1900&&year<=2100)out.push_back(q+" year:"+std::to_string(std::max(1900,year-8))+"-"+std::to_string(std::min(2100,year+8)));out.push_back(q);}return out;}
}
