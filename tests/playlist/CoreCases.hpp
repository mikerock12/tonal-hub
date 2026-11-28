#pragma once
#include "playlist/TextPlaylistCore.hpp"
#include <algorithm>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>
inline void expectCore(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
namespace playlistFixtures {
using namespace tonal::playlist;
inline Candidate track(std::string title,std::vector<std::string> artists,std::string uri={},std::vector<std::string> ids={},int playable=-1) {
    Candidate c;c.title=std::move(title);c.artists=std::move(artists);c.artistIds=std::move(ids);c.playable=playable;
    c.uri=uri.empty()?"spotify:track:"+std::to_string(std::hash<std::string>{}(c.title+c.artist())%10000000000000000000ull):uri;c.id=c.uri.substr(14);return c;
}
// The 15 regression cases from the user's Filadélfia prints (TONALIFY spec section 11). Candidates are the
// results shown by the app; "Look Down" uses a synthetic long title because the print did not show it in full.
struct PrintCase { const char *artist,*title; std::vector<Candidate> results; Match expected; bool autoSelect; int choice; };
inline std::vector<PrintCase> printCases() {
    return {
        {"Remember Sports","Get Bummed Out",{track("Get Bummed Out",{"Remember Sports"})},Match::Exact,true,0},
        {"Dr. Dog","Heart It Races",{track("Heart It Races - Live",{"Dr. Dog"})},Match::Alternate,false,0},
        {"The Wonder Years","Passing Through a Screen Door",{track("Passing Through A Screen Door",{"The Wonder Years"})},Match::Exact,true,0},
        {"Thin Lips","Non-Monogamy Nightmares",{track("Non-Monogamy Nightmare",{"Thin Lips"})},Match::Equivalent,true,0},
        {"Cayetana","Age of Consent",{track("Age of Consent (New Order)",{"Cayetana"})},Match::Equivalent,true,0},
        {"Kurt Vile","Baby\xE2\x80\x99s Arms",{track("Baby's Arms",{"Kurt Vile"})},Match::Exact,true,0},
        {"Algernon Cadwallader","Look Down",{track("Look Down, There's a Long Way to Go",{"Algernon Cadwallader"})},Match::Equivalent,false,0},
        {"The Superweaks","Glutton",{track("Gluttony",{"Buckcherry"})},Match::NotFound,false,-1},
        {"Free Energy","Dream City",{track("Dream City",{"Balcony Sunrise"})},Match::NotFound,false,-1},
        {"Free Energy","Hope Child",{track("Child of Hope",{"Chandelier"})},Match::NotFound,false,-1},
        {"The Superweaks","Call Me Away",{track("Call Me Away",{"The Superweaks"})},Match::Exact,true,0},
        {"Free Energy","Dance All Night",{track("Dance All Night",{"Free Energy"})},Match::Exact,true,0},
        {"Free Energy","All I Know",{track("All I Know",{"Screaming Trees"}),track("All I Know",{"Karmina"})},Match::NotFound,false,-1},
        {"Free Energy","Something In Common",{track("Something In Common",{"Bobby Brown","Whitney Houston"}),track("Something in Common",{"Kendra Morris"}),track("Something In Common",{"Dawes"})},Match::NotFound,false,-1},
        {"Free Energy","Girls Want Rock",{track("Girls Want Rock",{"Free Energy"})},Match::Exact,true,0},
    };
}
}
inline void playlistCoreCases(){
    using namespace tonal::playlist;using namespace playlistFixtures;
    auto p=parse("Nirvana - MTV Unplugged in New York (Full Concert)\n0:00 About a Girl\n03:39 Come as You Are\n[7:10] Jesus Doesn't Want Me for a Sunbeam");
    expectCore(p.detectedArtist=="Nirvana"&&p.songs.size()==3&&p.songs[1].title=="Come as You Are"&&p.songs[2].artist=="Nirvana","timestamped show/global artist");
    p=parse("1. Veruca Salt - Seether\n2) L7 – Shove\n3. Alice In Chains — Would?\n4. Green Day\tBasket Case");
    expectCore(p.songs.size()==4&&p.songs[0].artist=="Veruca Salt"&&p.songs[1].title=="Shove"&&p.songs[2].artist=="Alice In Chains"&&p.songs[3].artist=="Green Day","numbering, unicode separators and tabs");
    p=parse("Seether - Veruca Salt",2);expectCore(p.songs[0].artist=="Veruca Salt"&&p.songs[0].title=="Seether","reverse order");
    p=parse("Banda: L7\nSetlist:\n• Shove (00:00)\nhttps://example.test\nPretend We're Dead [3:10]");expectCore(p.songs.size()==2&&p.songs[1].artist=="L7","headers, bullets and trailing timestamps");
    p=parse("Oasis - Live Forever\n311 - Down");expectCore(p.songs.size()==2&&p.songs[0].title=="Live Forever"&&p.songs[1].artist=="311","song names not mistaken for shows or numbering");
    p=parse("Come as You Are\nLithium",0,"Nirvana");expectCore(p.songs[0].artist=="Nirvana","explicit default artist");
    p=parse("About a Girl - Nirvana",0,"Nirvana");expectCore(p.songs[0].title=="About a Girl"&&p.songs[0].artist=="Nirvana","default artist confirms reverse column order");
    p=parse("Seether by Veruca Salt");expectCore(p.songs[0].title=="Seether"&&p.songs[0].artist=="Veruca Salt","music by artist syntax");
    p=parse("Banda: Nirvana\nAbout a Girl\nAbout a Girl");expectCore(p.songs.size()==2,"intentional reprises retained");
    p=parse("001. Free Energy - Dream City");expectCore(p.songs[0].artist=="Free Energy"&&p.songs[0].title=="Dream City"&&p.songs[0].original=="001. Free Energy - Dream City","zero-padded numbering, original line preserved");

    // Conservative normalization: never split a word on an apostrophe; & and "and" are the same token.
    expectCore(normalize("João • É O Tchan!")=="joao e o tchan","portable accent/case normalization");
    expectCore(normalize("Baby\xE2\x80\x99s Arms")==normalize("Baby's Arms")&&normalize("Baby's Arms")=="babys arms","typographic and ASCII apostrophes");
    expectCore(normalize("Simon & Garfunkel")==normalize("Simon and Garfunkel"),"ampersand equivalence");
    expectCore(normalize("Track 1999")=="track 1999"&&normalize("Don't Stop")!=normalize("Stop"),"numbers and negations preserved");

    // Qualifiers are structured, not discarded.
    auto t=title("Heart It Races - Live");expectCore(t.version==Version::Live&&t.core=="heart it races","dash live qualifier");
    t=title("Come Together - 2009 Remaster");expectCore(t.version==Version::Studio&&t.annotated&&t.core=="come together","remaster is an edition, not a version");
    t=title("(Don't Fear) The Reaper");expectCore(t.core=="dont fear the reaper","leading parenthesis belongs to the name");
    t=title("Paranoid Android (Part 2)");expectCore(t.core.find("part 2")!=std::string::npos,"part number is essential");
    t=title("Age of Consent (New Order)");expectCore(t.core=="age of consent"&&t.version==Version::Studio,"original-artist credit is an annotation");
    t=title("Seether (Karaoke Version)");expectCore(t.coverMarker,"karaoke marker");
    t=title("Song (Radio Edit)");expectCore(t.version==Version::Edit,"radio edit is an alternate version");
    t=title("Song feat. Someone");expectCore(t.core=="song"&&t.annotated,"unbracketed featuring credit");

    // Regression matrix from the prints.
    for(const auto &k:printCases()){
        const auto d=decide({k.artist,k.title,{}},k.results);
        if(d.match!=k.expected||d.autoSelect!=k.autoSelect||d.choice!=k.choice)
            throw std::runtime_error(std::string("print case ")+k.artist+" - "+k.title+": got "+code(d.match)+(d.autoSelect?" auto":" review")+" choice "+std::to_string(d.choice)+" ("+d.reason+")");
        if(d.match==Match::NotFound)expectCore(!d.ordered.empty()&&!requestedArtistResult(d.ordered.front().match),"homonyms kept apart, never as a version");
    }
    auto d=decide({"Free Energy","Something In Common",{}},printCases()[13].results);
    expectCore(d.reason.find("3 resultado(s) de outros artistas")!=std::string::npos,"wrong artists counted in the explanation");

    // General cases.
    d=decide({"Veruca Salt","Seether",{}},{track("Seether",{"Karaoke Tribute"}),track("Seether",{"Veruca Salt"}),track("Seether (Live)",{"Other Artist"})});
    expectCore(d.autoSelect&&d.ordered[0].index==1&&d.match==Match::Exact,"exact artist/title beats popular karaoke");
    expectCore(d.ordered.back().match==Match::WrongArtist||d.ordered.back().match==Match::Cover,"other performers ranked last");
    d=decide({"","Tonight",{}},{track("Tonight",{"Band A"}),track("Tonight",{"Band B"})});expectCore(!d.autoSelect&&d.match==Match::Ambiguous,"title-only reference never auto-selected");
    d=decide({"Nirvana","Unknown Song",{}},{track("Smells Like Teen Spirit",{"Nirvana"})});expectCore(!d.autoSelect&&d.match==Match::NotFound&&d.choice==-1,"unrelated title of the right artist is not a match");
    d=decide({"Beatles","Come Together",{}},{track("Come Together - Remastered 2009",{"The Beatles"})});expectCore(d.autoSelect&&d.match==Match::Equivalent,"articles and remaster preserve identity");
    d=decide({"Veruca Salt","Seether",{}},{track("Seether",{"Veruca Salt","Other Guest"})});expectCore(d.autoSelect,"primary credited artist among collaborators");
    d=decide({"Other Guest","Seether",{}},{track("Seether",{"Veruca Salt","Other Guest"})});expectCore(!d.autoSelect&&d.match==Match::Ambiguous,"guest appearance is not the requested performer");
    d=decide({"Nirvana","About a Girl",{}},{track("About a Girl (Live at Reading)",{"Nirvana"}),track("About a Girl",{"Nirvana"})});
    expectCore(d.autoSelect&&d.ordered[0].index==1&&d.ordered[1].match==Match::Alternate,"studio preferred, live kept as alternate");
    d=decide({"Nirvana","About a Girl (Live)",{}},{track("About a Girl",{"Nirvana"}),track("About a Girl (Live)",{"Nirvana"})});expectCore(d.autoSelect&&d.ordered[0].index==1,"requested live version retained");
    d=decide({"Nirvana","Lithium",{}},{track("Lithium",{"Nirvana"},{}, {},0)});expectCore(d.match==Match::Unavailable&&!d.autoSelect&&d.choice==0,"unavailable in market is not selectable");
    d=decide({"Nirvana","Lithium",{}},{track("Lithium",{"Nirvana"},{}, {},0),track("Lithium - Live",{"Nirvana"},{}, {},1)});expectCore(!d.autoSelect,"unavailable studio does not turn into an automatic live");
    d=decide({"Simon and Garfunkel","The Boxer",{}},{track("The Boxer",{"Simon & Garfunkel"})});expectCore(d.autoSelect&&d.match==Match::Exact,"ampersand artist");
    d=decide({"Nirvanna","Lithium",{}},{track("Lithium",{"Nirvana"})});expectCore(!d.autoSelect&&d.match==Match::Ambiguous,"artist typo needs confirmation");
    d=decide({"Nirvana","Lithium",{}},{track("Lithium (Cover)",{"Nirvana Tribute Band"})});expectCore(d.match==Match::NotFound&&d.ordered[0].match==Match::Cover,"cover by another performer");
    d=decide({"Nirvana","Lithium",{}},{track("Lithiun",{"Nirvana"})});expectCore(d.match==Match::Equivalent&&!d.autoSelect,"title typo is a conditional equivalent");

    // Artist identity by Spotify ID; homonyms need extra evidence.
    const std::vector<ArtistHit> one{{"AAAAAAAAAAAAAAAAAAAAA1","Free Energy"}},two{{"AAAAAAAAAAAAAAAAAAAAA1","Free Energy"},{"BBBBBBBBBBBBBBBBBBBBB2","Free Energy"}};
    d=decide({"Free Energy","Dream City",{}},{track("Dream City",{"Free Energy"},{}, {"AAAAAAAAAAAAAAAAAAAAA1"})},one,true);
    expectCore(d.autoSelect&&d.reason.find("ID Spotify")!=std::string::npos,"artist confirmed by ID");
    d=decide({"Free Energy","Dream City",{}},{track("Dream City",{"Free Energy"},{}, {"CCCCCCCCCCCCCCCCCCCCC3"})},one,true);
    expectCore(!d.autoSelect&&d.match==Match::ArtistAmbiguous,"same name, different artist ID is not approved by name");
    d=decide({"Free Energy","Dream City",{}},{track("Dream City",{"Free Energy"},"spotify:track:1111111111111111111111",{"AAAAAAAAAAAAAAAAAAAAA1"}),track("Dream City",{"Free Energy"},"spotify:track:2222222222222222222222",{"BBBBBBBBBBBBBBBBBBBBB2"})},two,true);
    expectCore(!d.autoSelect&&d.match==Match::ArtistAmbiguous,"homonymous artists both offering the title");
    d=decide({"Free Energy","Dream City",{}},{track("Dream City",{"Free Energy"},"spotify:track:1111111111111111111111",{"AAAAAAAAAAAAAAAAAAAAA1"}),track("Dream City",{"Free Energy"},"spotify:track:3333333333333333333333",{"AAAAAAAAAAAAAAAAAAAAA1"})},two,true);
    expectCore(d.autoSelect,"duplicate releases of the same artist are not ambiguous");

    // Codes round-trip for persistence.
    for(auto m:{Match::Exact,Match::Equivalent,Match::Alternate,Match::Cover,Match::WrongArtist,Match::WrongSong,Match::Ambiguous,Match::NotFound,Match::Unavailable,Match::LookupError,Match::Incomplete,Match::ArtistAmbiguous})
        expectCore(matchFromCode(code(m))==m&&!label(m).empty(),"match code round-trip");

    auto q=queries({"Nirvana","Lithium",{}});expectCore(q.size()>=2&&q[0]=="track:\"Lithium\" artist:\"Nirvana\""&&q[1]=="Nirvana Lithium","directed query first, then joint text");
    q=queries({"Nirvana","About a Girl (2009 Remaster)",{}});expectCore(std::find(q.begin(),q.end(),"Nirvana about a girl")!=q.end(),"simplified title is a fallback query");

    // Progressive lookup: homonyms on the title search lead to the artist's own discography.
    {
        Lookup look({"Free Energy","Dream City",{}});
        auto r=look.next();expectCore(r.kind==Request::Tracks&&r.query[0].second=="track:\"Dream City\" artist:\"Free Energy\"","structured search first");
        bool market=false;for(auto &kv:r.query)market|=kv.first=="market"&&kv.second=="from_token";expectCore(market,"market from the authorized account");
        look.acceptTracks({track("Dream City",{"Balcony Sunrise"},{}, {"ZZZZZZZZZZZZZZZZZZZZZ9"})});
        r=look.next();expectCore(r.kind==Request::Artists,"artist resolved before giving up");
        look.acceptArtists({{"AAAAAAAAAAAAAAAAAAAAA1","Free Energy"},{"DDDDDDDDDDDDDDDDDDDDD4","Free Energy Band"}});
        expectCore(look.artists().size()==1,"only exact-name artists confirmed");
        int listings=0;bool sawAppears=false;
        while((r=look.next()).kind!=Request::None) {
            if(r.kind==Request::Tracks)look.acceptTracks({});
            else if(r.kind==Request::Albums){++listings;
                if(r.query[0].second=="appears_on"){sawAppears=true;look.acceptAlbums({},0);}
                else{expectCore(r.query[0].second=="album,single,compilation","own releases include compilations");
                    look.acceptAlbums({{"SSSSSSSSSSSSSSSSSSSSS1","Other Single","single","2010",""},{"SSSSSSSSSSSSSSSSSSSSS2","Dream City","single","2010",""},{"LLLLLLLLLLLLLLLLLLLLL1","Stuck on Nothing","album","2010",""}},3);}}
            else if(r.kind==Request::AlbumTracks){expectCore(r.path=="/albums/SSSSSSSSSSSSSSSSSSSSS2/tracks","release named after the song is fetched first");
                const auto map=look.acceptTracks({track("Bang Pop",{"Free Energy"},{}, {"AAAAAAAAAAAAAAAAAAAAA1"},1),track("Dream City",{"Free Energy"},{}, {"AAAAAAAAAAAAAAAAAAAAA1"},1)},2);
                expectCore(map[0]==-1&&map[1]>=0,"unrelated album tracks are not added to the review");}
        }
        d=look.decision();
        expectCore(sawAppears&&listings==2&&look.finished()&&d.autoSelect&&d.match==Match::Exact&&look.pool()[size_t(d.ordered[0].index)].album=="Dream City","discography lookup finds the requested artist");
        expectCore(d.ordered.back().match==Match::WrongArtist&&look.requests()<=12,"homonym kept apart and bounded requests");
    }
    // V3 (Ohio): every page of releases and every release's tracks, compilations included, before "not found".
    {
        Lookup look({"New Bomb Turks","Dragstrip Riot",{}});look.seedArtists({{"NNNNNNNNNNNNNNNNNNNNN1","New Bomb Turks"}});
        Request r;int pages=0,offsets=0,albumCalls=0;
        while((r=look.next()).kind!=Request::None){
            if(r.kind==Request::Albums){
                if(r.query[0].second=="appears_on"){look.acceptAlbums({},0);continue;}
                ++pages;int offset=0;for(auto &kv:r.query)if(kv.first=="offset")offset=std::stoi(kv.second);offsets+=offset;
                std::vector<AlbumHit> page;for(int i=0;i<10&&offset+i<25;++i){const auto n=std::to_string(offset+i);page.push_back({std::string(20-n.size(),'R')+"AB"+n,"Release "+n,offset+i==21?"compilation":"album","1995",""});}
                look.acceptAlbums(page,25);
            } else if(r.kind==Request::AlbumTracks){++albumCalls;
                look.acceptTracks(r.album.name=="Release 21"?std::vector<Candidate>{track("Dragstrip Riot",{"New Bomb Turks"},{}, {"NNNNNNNNNNNNNNNNNNNNN1"},1)}:std::vector<Candidate>{track("Other",{"New Bomb Turks"})},1);
            } else look.acceptTracks({});
        }
        d=look.decision();
        expectCore(pages==3&&offsets==30&&d.autoSelect&&d.match==Match::Exact,"all release pages read; the compilation track is recovered");
        expectCore(albumCalls>=7,"more than the 0.18.2 cap of six releases is checked");
    }
    {   // Not found after the complete scan: the claim names the work done. Cut by the budget: SEARCH_INCOMPLETE, no claim.
        auto scan=[](int budget){Lookup look({"The Dopamines","Kitchen Cleaners",{}},"from_token",18,budget);look.seedArtists({{"DPDPDPDPDPDPDPDPDPDPD1","The Dopamines"}});Request r;
            while((r=look.next()).kind!=Request::None){
                if(r.kind==Request::Albums){if(r.query[0].second=="appears_on")look.acceptAlbums({},0);else{std::vector<AlbumHit> page;for(int i=0;i<10;++i){const auto n=std::to_string(i);page.push_back({std::string(20,'D')+"0"+n,"Release "+n,"album","2010",""});}look.acceptAlbums(page,13);}}
                else if(r.kind==Request::AlbumTracks)look.acceptTracks({track("Other Song",{"The Dopamines"})},1);else look.acceptTracks({});}
            return look.decision();};
        d=scan(240);expectCore(d.match==Match::NotFound&&d.reason.find("discografia completa conferida")!=std::string::npos,"complete scan stated");
        d=scan(4);expectCore(d.match==Match::Incomplete&&d.reason.find("discografia completa")==std::string::npos&&d.reason.find("busca incompleta")!=std::string::npos,"cut scan is SEARCH_INCOMPLETE, never 'discography checked'");
    }
    {   // Part / Pt. / roman numerals share a key; the Dopamines title is found as an exact equivalent.
        expectCore(title("You're So Vain Part II").core==title("You\xE2\x80\x99re So Vain, Pt. 2").core,"Part II == Pt. 2");
        expectCore(title("Chapter IV").core==title("Chapter 4").core&&title("Rocky II").core!=title("Rocky 2").core,"roman numerals only after part/volume words");
        d=decide({"The Dopamines","You're So Vain Part II",{}},{track("You're So Vain, Pt. 2",{"The Dopamines"})});
        expectCore(d.autoSelect,"Part II variant auto-selected for the same artist");
        d=decide({"The Dopamines","You're So Vain Part II",{}},{track("You're So Vain, Pt. 2",{"Other Band"})});
        expectCore(!d.autoSelect&&d.match==Match::NotFound,"the variant by another artist is not accepted");
    }
    {   // Scrawl: two Spotify profiles with the same name. Extra member credits are not another artist;
        // shared discography titles prove one act; disjoint discographies keep the choice with the user.
        auto run=[](bool shared,const std::string &pinned){
            Lookup look({"Scrawl","Charles",{}});
            Context ctx;if(!pinned.empty()){ctx.preferredId=pinned;ctx.preferredReason="cadastro fixado por você";}look.setContext(ctx);
            Request r;
            while((r=look.next()).kind!=Request::None){
                if(r.kind==Request::Artists)look.acceptArtists({{"SCRAWLAAAAAAAAAAAAAAA1","Scrawl"},{"SCRAWLBBBBBBBBBBBBBBB2","Scrawl"}});
                else if(r.kind==Request::Albums){
                    if(r.query[0].second=="appears_on"){look.acceptAlbums({},0);continue;}
                    if(r.path.find("SCRAWLAAAAAAAAAAAAAAA1")!=std::string::npos)look.acceptAlbums({{"NATUREFILMAAAAAAAAAAA1","Nature Film","album","1990",""}},1);
                    else look.acceptAlbums({{"SMALLMOUTHBBBBBBBBBBB2","Smallmouth","album","1991",""}},1);
                } else if(r.kind==Request::AlbumTracks){
                    if(r.album.name=="Nature Film")look.acceptTracks({track("Charles",{"Scrawl"},"spotify:track:CHARLESAAAAAAAAAAAAAA1",{"SCRAWLAAAAAAAAAAAAAAA1"},1),track("Clock Song",{"Scrawl"}),track("Green Mind",{"Scrawl"}),track("Time",{"Scrawl"})},4);
                    else look.acceptTracks(shared?std::vector<Candidate>{track("Charles",{"Scrawl","Marcy Mays","Sue Harshe","Carolyn O'Leary"},"spotify:track:CHARLESBBBBBBBBBBBBBB2",{"SCRAWLBBBBBBBBBBBBBBB2","MMMMMMMMMMMMMMMMMMMMM1","SSSSSSSSSSSSSSSSSSSSS3","CCCCCCCCCCCCCCCCCCCCC4"},1),track("Clock Song",{"Scrawl"}),track("Green Mind",{"Scrawl"})}
                        :std::vector<Candidate>{track("Charles",{"Scrawl"},"spotify:track:CHARLESBBBBBBBBBBBBBB2",{"SCRAWLBBBBBBBBBBBBBBB2"},1),track("Unrelated",{"Scrawl"})},3);
                } else look.acceptTracks({track("Charles",{"Scrawl"},"spotify:track:CHARLESAAAAAAAAAAAAAA1",{"SCRAWLAAAAAAAAAAAAAAA1"},1),
                    track("Charles",{"Scrawl","Marcy Mays","Sue Harshe","Carolyn O'Leary"},"spotify:track:CHARLESBBBBBBBBBBBBBB2",{"SCRAWLBBBBBBBBBBBBBBB2","MMMMMMMMMMMMMMMMMMMMM1","SSSSSSSSSSSSSSSSSSSSS3","CCCCCCCCCCCCCCCCCCCCC4"},1)});
            }
            return std::make_pair(look.decision(),look.pool());};
        auto [same,pool]=run(true,"");
        expectCore(same.autoSelect&&pool[size_t(same.ordered[0].index)].uri=="spotify:track:CHARLESAAAAAAAAAAAAAA1"&&same.reason.find("músicas em comum")!=std::string::npos,"duplicated profiles of one act are not homonyms");
        auto [different,pool2]=run(false,"");
        expectCore(!different.autoSelect&&different.match==Match::ArtistAmbiguous&&std::string(group(different.match))=="homonyms","disjoint discographies stay ARTIST_AMBIGUOUS");
        expectCore(different.reason.find("Nature Film")!=std::string::npos,"ambiguity names the albums");
        auto [pinned,pool3]=run(false,"SCRAWLBBBBBBBBBBBBBBB2");
        expectCore(pinned.autoSelect&&pool3[size_t(pinned.ordered[0].index)].uri=="spotify:track:CHARLESBBBBBBBBBBBBBB2"&&pinned.reason.find("fixado")!=std::string::npos,"a pinned profile decides for the list");
    }
    {   // Duplicate detection keys and pasted-list fingerprints.
        expectCore(referenceKey({"The Dopamines","You're So Vain Part II",{}})==referenceKey({"Dopamines","You\xE2\x80\x99re So Vain, Pt. 2",{}}),"reference key ignores article, apostrophe and Pt./Part");
        expectCore(referenceKey({"Scrawl","Charles",{}})!=referenceKey({"Scrawl","Clock Song",{}}),"distinct songs differ");
        const std::vector<Song> pasted{{"Scrawl","Charles",{}},{"Scrawl","Clock Song",{}}},again{{"scrawl","Charles",{}},{"Scrawl","Clock song",{}}},other{{"Scrawl","Clock Song",{}},{"Scrawl","Charles",{}}};
        expectCore(fingerprint(pasted)==fingerprint(again)&&fingerprint(pasted)!=fingerprint(other),"the same list pasted twice is recognised");
    }
    {   // Technical failure never becomes "not found".
        Lookup look({"Free Energy","Dream City",{}});const auto first=look.next();look.fail(503,"Spotify HTTP 503");
        expectCore(!look.finished()&&look.backoffMs()>0&&look.decision().match==Match::Pending,"transient failure is retried with backoff");
        const auto again=look.next();expectCore(again.kind==first.kind&&again.query==first.query,"the same request is repeated");
        look.fail(503,"Spotify HTTP 503");look.next();look.fail(503,"Spotify HTTP 503");
        expectCore(look.finished()&&look.decision().match==Match::LookupError&&look.next().kind==Request::None,"lookup error is not NOT_FOUND");
        Lookup recovered({"Free Energy","Girls Want Rock",{}});recovered.next();recovered.fail(503,"Spotify HTTP 503");recovered.next();
        recovered.acceptTracks({track("Girls Want Rock",{"Free Energy"})});recovered.next();recovered.acceptArtists({});
        expectCore(recovered.next().kind==Request::None&&recovered.decision().autoSelect,"a retried request that succeeds continues normally");
        Lookup timeout({"Free Energy","Dream City",{}});timeout.next();timeout.acceptTracks({track("Dream City",{"Balcony Sunrise"})});timeout.next();
        for(int i=0;i<3;++i){timeout.fail(0,"Tempo esgotado");if(!timeout.finished())timeout.next();}
        expectCore(timeout.decision().match==Match::LookupError&&!timeout.decision().ordered.empty(),"partial results kept on timeout");
    }
    {   // Missing album (404) only skips that step; cached artist avoids a second artist search.
        Lookup look({"Free Energy","Hope Child",{}});look.seedArtists({{"AAAAAAAAAAAAAAAAAAAAA1","Free Energy"}});
        Request r;int artistsRequests=0,failures=0;
        while((r=look.next()).kind!=Request::None){if(r.kind==Request::Artists)++artistsRequests;
            if(r.kind==Request::Albums){look.acceptAlbums({{"EEEEEEEEEEEEEEEEEEEEE5","Love Sign","album","2011",""}},1);}
            else if(r.kind==Request::AlbumTracks){look.fail(404,"not found");++failures;}
            else look.acceptTracks({});}
        expectCore(artistsRequests==0&&failures==1&&look.decision().match==Match::NotFound,"404 on an album skips the step; seeded artist reused");
        expectCore(look.decision().reason.find("indisponível na sua região")!=std::string::npos,"absence after the discography is not claimed as absence from Spotify");
    }
    {   // Safe result ends early: one track search + one artist search.
        Lookup look({"Free Energy","Girls Want Rock",{}});look.next();look.acceptTracks({track("Girls Want Rock",{"Free Energy"},{}, {"AAAAAAAAAAAAAAAAAAAAA1"},1)});
        look.next();look.acceptArtists({{"AAAAAAAAAAAAAAAAAAAAA1","Free Energy"}});
        expectCore(look.next().kind==Request::None&&look.requests()==2&&look.decision().autoSelect,"early finish after artist confirmation");
    }
    {   // Budget exhaustion is reported as incomplete, not as absence.
        Lookup look({"Free Energy","Hope Child",{}},"from_token",2);Request r;while((r=look.next()).kind!=Request::None){if(r.kind==Request::Artists)look.acceptArtists({});else look.acceptTracks({});}
        expectCore(look.decision().match==Match::Incomplete,"budget exhaustion is SEARCH_INCOMPLETE");
    }

    // 500 references x 10 candidates stay fast enough for the UI thread between requests.
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<500;++i){std::vector<Candidate> c;for(int k=0;k<10;++k)c.push_back(track("Song "+std::to_string(i+k)+" (Live)",{"Band "+std::to_string(k)}));decide({"Band 3","Song "+std::to_string(i+3),{}},c);}
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    expectCore(ms<20000,"500 decisions took too long");

    bool rejected=false;try{parse(std::string(128*1024+1,'x'));}catch(const std::length_error &){rejected=true;}expectCore(rejected,"text size bounded");
    rejected=false;try{parse(std::string(2001,'x'));}catch(const std::length_error &){rejected=true;}expectCore(rejected,"line size bounded");
    std::string list;for(int i=0;i<501;++i)list+="Band - Song\n";rejected=false;try{parse(list);}catch(const std::length_error &){rejected=true;}expectCore(rejected,"playlist row count bounded");
}
