#pragma once
#include "playlist/TextPlaylistCore.hpp"
#include <stdexcept>
#include <string>
inline void expectCore(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
inline void playlistCoreCases(){
    using namespace tonal::playlist;
    auto p=parse("Nirvana - MTV Unplugged in New York (Full Concert)\n0:00 About a Girl\n03:39 Come as You Are\n[7:10] Jesus Doesn't Want Me for a Sunbeam");
    expectCore(p.detectedArtist=="Nirvana"&&p.songs.size()==3&&p.songs[1].title=="Come as You Are"&&p.songs[2].artist=="Nirvana","timestamped show/global artist");
    p=parse("1. Veruca Salt - Seether\n2) L7 – Shove\n3. Alice In Chains — Would?\n4. Green Day\tBasket Case");
    expectCore(p.songs.size()==4&&p.songs[0].artist=="Veruca Salt"&&p.songs[1].title=="Shove"&&p.songs[2].artist=="Alice In Chains"&&p.songs[3].artist=="Green Day","numbering, unicode separators and tabs");
    p=parse("Seether - Veruca Salt",2);expectCore(p.songs[0].artist=="Veruca Salt"&&p.songs[0].title=="Seether","reverse order");
    p=parse("Banda: L7\nSetlist:\n• Shove (00:00)\nhttps://example.test\nPretend We're Dead [3:10]");expectCore(p.songs.size()==2&&p.songs[1].artist=="L7","headers, bullets and trailing timestamps");
    p=parse("Oasis - Live Forever\n311 - Down");expectCore(p.songs.size()==2&&p.songs[0].title=="Live Forever"&&p.songs[1].artist=="311","song names not mistaken for shows or numbering");
    p=parse("Come as You Are\nLithium",0,"Nirvana");expectCore(p.songs[0].artist=="Nirvana","explicit default artist");
    p=parse("Banda: Nirvana\nAbout a Girl\nAbout a Girl");expectCore(p.songs.size()==2,"intentional reprises retained");
    expectCore(normalize("João • É O Tchan!")=="joao e o tchan","portable accent/case normalization");
    auto r=rank({"Veruca Salt","Seether",{}},{{"Seether","Karaoke Tribute",99},{"Seether","Veruca Salt",-1},{"Seether (Live)","Other Artist",100}});
    expectCore(r[0].index==1&&r[0].highConfidence,"exact artist/title beats popular karaoke without popularity data");
    r=rank({"","Tonight",{}},{{"Tonight","Band A",-1},{"Tonight","Band B",-1}});expectCore(!r[0].highConfidence,"homonymous recordings require review");
    r=rank({"Nirvana","Unknown Song",{}},{{"Smells Like Teen Spirit","Nirvana",100}});expectCore(!r[0].highConfidence,"unrelated title never automatically selected");
    auto q=queries({"Nirvana","Lithium",{}});expectCore(q.size()==3&&q[0]=="track:\"Lithium\" artist:\"Nirvana\"","shared directed and fallback queries");
    bool rejected=false;try{parse(std::string(128*1024+1,'x'));}catch(const std::length_error &){rejected=true;}expectCore(rejected,"text size bounded");
    rejected=false;try{parse(std::string(2001,'x'));}catch(const std::length_error &){rejected=true;}expectCore(rejected,"line size bounded");
    std::string list;for(int i=0;i<501;++i)list+="Band - Song\n";rejected=false;try{parse(list);}catch(const std::length_error &){rejected=true;}expectCore(rejected,"playlist row count bounded");
}
