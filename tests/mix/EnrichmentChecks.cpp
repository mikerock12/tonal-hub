#include "playback/DiscoveryLyrics.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>
#include <QDir>
#include <iostream>
#include <stdexcept>
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
bool spin(const std::function<bool()> &done,int timeout=8000){QElapsedTimer t;t.start();while(t.elapsed()<timeout){QCoreApplication::processEvents();if(done())return true;QThread::msleep(5);}return done();}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);QTemporaryDir folder;const auto path=folder.filePath("cache.json");
 QVariantMap track{{"uri","spotify:track:AAAAAAAAAAAAAAAAAAAAAA"},{"title","Test Song"},{"artist","Test Band"},{"album","Test Album"},{"durationMs",180000}};
 int requests=0;
 {
   DiscoveryLyrics cache(path,nullptr,nullptr,[&](const QVariantMap &t,DiscoveryLyrics::Reply done){
     ++requests;QJsonObject response{{"trackName",t["title"].toString()},{"artistName",t["artist"].toString()},{"duration",180.},
       {"plainLyrics","I fight against the rules. I resist and rebel. We break the chains and stand together."}};
     if(t["title"]=="Wrong version")response["duration"]=280.;
     done(t["title"]=="Missing"?404:200,response,0);
   });
   auto wrong=track;wrong["title"]="Wrong version";
   auto missing=track;missing["title"]="Missing";
   cache.enqueue({track},{track,wrong,missing});
   require(spin([&]{return requests==3;}),"background batch did not finish");
   require(cache.lookup(track)["lyricsAnalyzed"].toBool(),"matched lyrics not analyzed");
   require(cache.lookup(wrong).isEmpty(),"different recording duration accepted");
   require(cache.lookup(missing).isEmpty(),"missing lyrics invented");
   require(cache.lookup(track)["topics"].toMap().size()==8,"shared model scores missing");
   cache.enqueue({track},{wrong,missing});spin([]{return false;},1400);
   require(requests==3,"cache or negative-cache failed to suppress network");
   auto invalid=track;invalid["uri"]="fixture:invalid";cache.enqueue({invalid},{});spin([]{return false;},1400);
   require(requests==3,"synthetic URI sent to external provider");
   cache.setEnabled(false);auto fresh=track;fresh["title"]="Not requested";cache.enqueue({fresh},{});
   spin([]{return false;},1400);require(requests==3,"disabled analysis still requested lyrics");
 }
 {
   DiscoveryLyrics restored(path,nullptr);
   require(!restored.enabled(),"analysis preference not persisted");
   require(restored.lookup(track)["lyricsAnalyzed"].toBool(),"derived cache did not survive reopen");
 }
 QFile data(path);require(data.open(QIODevice::ReadOnly),"cache missing");auto bytes=data.readAll();
 require(!bytes.contains("fight against")&&!bytes.contains("Test Song")&&!bytes.contains("plainLyrics"),"lyrics or raw metadata leaked to derived cache");
 auto other=track;other["album"]="Different edition";require(DiscoveryLyrics::signature(other)!=DiscoveryLyrics::signature(track),"recording identity ignores album");
 require(!DiscoveryLyrics::matches(track,{{"trackName","Different song"},{"artistName","Test Band"},{"duration",180.}}),"wrong title accepted");
 {
   DiscoveryLyrics::Reply delayed;int calls=0;
   DiscoveryLyrics cancelled(folder.filePath("cancelled.json"),nullptr,nullptr,[&](const QVariantMap&,DiscoveryLyrics::Reply done){++calls;delayed=std::move(done);});
   cancelled.enqueue({track},{});require(spin([&]{return bool(delayed);}),"inflight fixture not dispatched");
   cancelled.setEnabled(false);
   delayed(200,{{"trackName","Test Song"},{"artistName","Test Band"},{"duration",180.},{"plainLyrics","We fight and resist together"}},0);
   require(cancelled.lookup(track).isEmpty(),"late response survived opt-out");
   require(calls==1,"cancellation dispatched another request");
 }
 {
   int calls=0;
   DiscoveryLyrics cooldown(folder.filePath("cooldown.json"),nullptr,nullptr,[&](const QVariantMap&,DiscoveryLyrics::Reply done){++calls;done(503,{},60);});
   cooldown.enqueue({track},{});require(spin([&]{return calls==1;}),"cooldown fixture not dispatched");
   auto fresh=track;fresh["title"]="Another recording";
   cooldown.enqueue({fresh},{});spin([]{return false;},1500);
   require(calls==1,"new selection bypassed provider cooldown");cooldown.setEnabled(false);
 }
 std::cout<<"DISCOVERY_ENRICHMENT_PASS cache, negative-cache, version matching, shared model, opt-out, persistence, privacy, no transport\n";
}
