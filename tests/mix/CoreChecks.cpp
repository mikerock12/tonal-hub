#include "playback/ListeningMixCore.hpp"
#include <stdexcept>
#include <set>
#include <iostream>
#include <algorithm>
void check(bool v){if(!v)throw std::runtime_error("mix invariant failed");}
int main(){using namespace tonal::mix;auto a=order(50,42);check(a==order(50,42)&&std::set<int>(a.begin(),a.end()).size()==50);std::vector<Track>s;for(int i=0;i<8;++i)s.push_back({"s"+std::to_string(i),"Hole","album","grunge",1994});std::vector<Track>p={{"s0","New Band","other","grunge",1993},{"x","Unheard Band","other","grunge",1993},{"y","Known Band","other","grunge",1994,true,true},{"z","Second New Band","other","grunge",1995},{"a","Hole","other","grunge",1994},{"b","Wrong Genre","other","classical",2020},{"c","Same Album","album","grunge",1994},{"d","Unheard Band","other","grunge",1995}};auto q=plan(s,p,0,false,true,9,true);check(q.size()==11&&q[0]==0&&q[1]==1&&q[2]<0&&q[3]==2&&q[5]<0);std::set<int> outside;for(int i:q)if(i<0){check(i!=-1&&i!=-5&&i!=-6&&i!=-7);outside.insert(i);}check(outside.size()==3);auto plain=plan(s,p,3,false,false,1);check(plain==std::vector<int>({3,4,5,6,7}));auto shuffled=plan(s,{},3,true,false,1);check(shuffled.front()==3&&shuffled.size()==8);check(plan({},p,0,true,true,1).empty());check(queries("",1994).empty());check(queries("grunge, alternative rock",1994).size()==4);std::vector<Track> large;for(int i=0;i<500;++i)large.push_back({"source"+std::to_string(i),"Hole","album","grunge",1994});auto capped=plan(large,p,0,false,true,42,true);check(capped.size()==500&&std::all_of(capped.begin(),capped.end(),[](int n){return n>=0;}));std::vector<Track> varied;for(int i=0;i<20;++i)varied.push_back({"v"+std::to_string(i),"Old Band","album",i<15?"classical":"punk"});auto around=plan(varied,{{"new-punk","New Punk Band","other","punk"}},15,false,true,1);check(around.size()==6&&around[2]<0);std::set<std::vector<int>> visualOrders,playbackOrders,discoveries;
for(uint32_t seed=1;seed<=100;++seed){auto visual=order(8,seed);auto playback=plan(s,{},0,true,false,seed);check(std::set<int>(playback.begin(),playback.end()).size()==8);visualOrders.insert(visual);playbackOrders.insert(playback);check(playback.front()==0);discoveries.insert(plan(s,p,0,false,true,seed,true));}
check(visualOrders.size()>95&&playbackOrders.size()>90&&discoveries.size()>=3);
check(order(8,42)!=plan(s,{},0,true,false,42));
// A mode change must reuse the accepted discoveries, even if the network is
// unavailable or a fresh pool contains more attractive tracks.
std::vector<Track> mixed={s[0],s[1],p[1],s[2],s[3],p[3],s[4]};
mixed[2].external=true;mixed[5].external=true;
for(uint32_t seed=1;seed<100;++seed){
 auto retained=plan(mixed,p,0,true,true,seed);
 check(retained.front()==0&&retained.size()==mixed.size());
 check(std::set<int>(retained.begin(),retained.end())==std::set<int>({0,1,2,3,4,5,6}));
 check(mixed[retained[2]].external&&mixed[retained[5]].external);
 auto disabled=plan(mixed,p,0,true,false,seed);
 check(disabled.size()==5&&std::none_of(disabled.begin(),disabled.end(),[&](int i){return mixed[i].external;}));
}
check(plan(mixed,{},0,false,true,3)==std::vector<int>({0,1,2,3,4,5,6}));
auto externalCurrent=plan(mixed,{},2,false,false,3);
check(externalCurrent.front()==2&&externalCurrent.size()==4);
auto shortRemainder=plan(mixed,{},4,false,true,3);
check(shortRemainder.size()==3&&std::find(shortRemainder.begin(),shortRemainder.end(),5)!=shortRemainder.end());
// Cached themes can add evidence beyond release year alone. Unknown genres
// must not turn a same-year candidate into an automatic match.
std::vector<Track> lyricReferences={{"l1","Reference","a","",1994,false,false,1},{"l2","Reference","a","",1994,false,false,1}};
std::vector<Track> lyricCandidates={{"no-theme","Other","b","",1994},{"shared-theme","New Artist","c","",1994,false,false,1}};
check(plan(lyricReferences,lyricCandidates,0,false,true,42)==std::vector<int>({0,1,-2}));
std::cout<<"LISTENING_MIX_CORE_PASS order, restore identity, random selection, 2+1 cadence, novelty, exclusions, offline fallback, bounds\n";}
