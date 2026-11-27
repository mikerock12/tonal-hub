#include <jni.h>
#include "playlist/TextPlaylistCore.hpp"
#include <string>
#include <vector>
#include <stdexcept>

namespace {
std::string bytes(JNIEnv *e,jbyteArray a){if(!a)return {};jsize n=e->GetArrayLength(a);if(n>128*1024)return {};std::string s(n,'\0');e->GetByteArrayRegion(a,0,n,reinterpret_cast<jbyte*>(s.data()));return s;}
jstring string(JNIEnv *e,const std::string &s){auto cls=e->FindClass("java/lang/String");auto ctor=e->GetMethodID(cls,"<init>","([BLjava/lang/String;)V");auto b=e->NewByteArray(jsize(s.size()));e->SetByteArrayRegion(b,0,jsize(s.size()),reinterpret_cast<const jbyte*>(s.data()));auto utf=e->NewStringUTF("UTF-8");auto result=static_cast<jstring>(e->NewObject(cls,ctor,b,utf));e->DeleteLocalRef(b);e->DeleteLocalRef(utf);e->DeleteLocalRef(cls);return result;}
std::string fromString(JNIEnv *e,jstring s){if(!s)return {};auto cls=e->FindClass("java/lang/String");auto method=e->GetMethodID(cls,"getBytes","(Ljava/lang/String;)[B");auto utf=e->NewStringUTF("UTF-8");auto b=static_cast<jbyteArray>(e->CallObjectMethod(s,method,utf));auto out=bytes(e,b);e->DeleteLocalRef(b);e->DeleteLocalRef(utf);e->DeleteLocalRef(cls);return out;}
jobjectArray strings(JNIEnv *e,const std::vector<std::string> &values){auto cls=e->FindClass("java/lang/String");auto a=e->NewObjectArray(jsize(values.size()),cls,nullptr);for(jsize i=0;i<jsize(values.size());++i){auto s=string(e,values[i]);e->SetObjectArrayElement(a,i,s);e->DeleteLocalRef(s);}e->DeleteLocalRef(cls);return a;}
void error(JNIEnv *e,const std::exception &x){auto cls=e->FindClass("java/lang/IllegalArgumentException");e->ThrowNew(cls,x.what());e->DeleteLocalRef(cls);}
}
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_TextPlaylistNative_parse(JNIEnv *e,jclass,jbyteArray text,jint format,jbyteArray artist){
    try{if(!text||e->GetArrayLength(text)>128*1024)throw std::length_error("Texto acima de 128 KiB");auto parsed=tonal::playlist::parse(bytes(e,text),format,bytes(e,artist));std::vector<std::string> out{parsed.detectedArtist};for(const auto &s:parsed.songs){out.push_back(s.artist);out.push_back(s.title);out.push_back(s.original);}return strings(e,out);}catch(const std::exception &x){error(e,x);return nullptr;}
}
// 0.18.2: the progressive lookup (artist identity, versions, availability) runs in shared C++.
// Java keeps one native Lookup per reference, only on the UI thread, and frees it when done.
namespace {
namespace tp=tonal::playlist;
tp::Lookup *lookup(jlong h){return reinterpret_cast<tp::Lookup*>(h);}
std::vector<std::string> list(JNIEnv *e,jobjectArray a,size_t maxItems){
    std::vector<std::string> out;if(!a)return out;const jsize n=e->GetArrayLength(a);if(size_t(n)>maxItems)throw std::length_error("Resposta grande demais");
    for(jsize i=0;i<n;++i){auto s=static_cast<jstring>(e->GetObjectArrayElement(a,i));out.push_back(fromString(e,s));if(s)e->DeleteLocalRef(s);}return out;
}
std::vector<std::string> split(const std::string &s){std::vector<std::string> out;if(s.empty())return out;size_t start=0;for(size_t p;(p=s.find('\x1f',start))!=std::string::npos;start=p+1)out.push_back(s.substr(start,p-start));out.push_back(s.substr(start));return out;}
std::vector<std::string> decisionStrings(const tp::Decision &d){
    std::vector<std::string> out{tp::code(d.match),tp::label(d.match),std::to_string(d.choice),d.autoSelect?"1":"0",d.reason};
    for(const auto &x:d.ordered){out.push_back(std::to_string(x.index));out.push_back(tp::code(x.match));out.push_back(tp::label(x.match));out.push_back(x.safe?"1":"0");out.push_back(x.reason);out.push_back(tp::requestedArtistResult(x.match)?"1":"0");}
    return out;
}
}
extern "C" JNIEXPORT jlong JNICALL Java_app_tonal_android_TextPlaylistNative_lookupCreate(JNIEnv *e,jclass,jbyteArray artist,jbyteArray title,jbyteArray original){
    try{return reinterpret_cast<jlong>(new tp::Lookup({bytes(e,artist),bytes(e,title),bytes(e,original)}));}catch(const std::exception &x){error(e,x);return 0;}
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TextPlaylistNative_lookupFree(JNIEnv*,jclass,jlong h){delete lookup(h);}
// [kind, path, label, albumId, albumName, albumType, albumDate, albumImage, key1, value1, ...]; kind "" = finished.
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_TextPlaylistNative_lookupNext(JNIEnv *e,jclass,jlong h){
    if(!h)return nullptr;const auto r=lookup(h)->next();
    static const char *kinds[]={"","tracks","artists","albums","albumTracks"};
    std::vector<std::string> out{kinds[r.kind],r.path,r.label,r.album.id,r.album.name,r.album.type,r.album.releaseDate,r.album.image};
    for(const auto &[k,v]:r.query){out.push_back(k);out.push_back(v);}return strings(e,out);
}
// 13 fields per track: id, uri, title, album, albumType, releaseDate, artists(0x1f), artistIds(0x1f), popularity, durationMs, playable, restriction, linkedFrom.
extern "C" JNIEXPORT jintArray JNICALL Java_app_tonal_android_TextPlaylistNative_lookupTracks(JNIEnv *e,jclass,jlong h,jobjectArray fields){
    try{if(!h)return nullptr;const auto f=list(e,fields,13*60);if(f.size()%13)throw std::length_error("Campos de faixa inválidos");std::vector<tp::Candidate> input;
        for(size_t i=0;i<f.size();i+=13){tp::Candidate c;c.id=f[i];c.uri=f[i+1];c.title=f[i+2];c.album=f[i+3];c.albumType=f[i+4];c.releaseDate=f[i+5];c.artists=split(f[i+6]);c.artistIds=split(f[i+7]);
            c.popularity=std::atoi(f[i+8].c_str());c.durationMs=std::atoi(f[i+9].c_str());c.playable=std::atoi(f[i+10].c_str());c.restriction=f[i+11];c.linkedFrom=f[i+12];input.push_back(std::move(c));}
        const auto map=lookup(h)->acceptTracks(input);auto a=e->NewIntArray(jsize(map.size()));e->SetIntArrayRegion(a,0,jsize(map.size()),map.data());return a;
    }catch(const std::exception &x){error(e,x);return nullptr;}
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TextPlaylistNative_lookupArtists(JNIEnv *e,jclass,jlong h,jobjectArray pairs,jboolean seed){
    try{if(!h)return;const auto f=list(e,pairs,2*50);std::vector<tp::ArtistHit> hits;for(size_t i=0;i+1<f.size();i+=2)hits.push_back({f[i],f[i+1]});
        if(seed)lookup(h)->seedArtists(hits);else lookup(h)->acceptArtists(hits);}catch(const std::exception &x){error(e,x);}
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TextPlaylistNative_lookupAlbums(JNIEnv *e,jclass,jlong h,jobjectArray fields){
    try{if(!h)return;const auto f=list(e,fields,5*50);std::vector<tp::AlbumHit> hits;for(size_t i=0;i+4<f.size();i+=5)hits.push_back({f[i],f[i+1],f[i+2],f[i+3],f[i+4]});lookup(h)->acceptAlbums(hits);}catch(const std::exception &x){error(e,x);}
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TextPlaylistNative_lookupFail(JNIEnv *e,jclass,jlong h,jint status,jbyteArray message){if(h)lookup(h)->fail(status,bytes(e,message));}
extern "C" JNIEXPORT jboolean JNICALL Java_app_tonal_android_TextPlaylistNative_lookupFinished(JNIEnv*,jclass,jlong h){return h&&lookup(h)->finished();}
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TextPlaylistNative_lookupRequests(JNIEnv*,jclass,jlong h){return h?lookup(h)->requests():0;}
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TextPlaylistNative_lookupBackoff(JNIEnv*,jclass,jlong h){return h?lookup(h)->backoffMs():0;}
// [code, label, choice, autoSelect, reason, then per ordered candidate: index, code, label, safe, reason, requestedArtist]
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_TextPlaylistNative_lookupDecision(JNIEnv *e,jclass,jlong h){return h?strings(e,decisionStrings(lookup(h)->decision())):nullptr;}
// ["1"|"0" resolved, id1, name1, ...] for the per-import artist cache.
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_TextPlaylistNative_lookupResolvedArtists(JNIEnv *e,jclass,jlong h){
    if(!h)return nullptr;std::vector<std::string> out{lookup(h)->artistsResolved()?"1":"0"};for(const auto &a:lookup(h)->artists()){out.push_back(a.id);out.push_back(a.name);}return strings(e,out);
}
extern "C" JNIEXPORT jstring JNICALL Java_app_tonal_android_TextPlaylistNative_normalize(JNIEnv *e,jclass,jbyteArray value){return string(e,tp::normalize(bytes(e,value)));}
extern "C" JNIEXPORT jstring JNICALL Java_app_tonal_android_TextPlaylistNative_label(JNIEnv *e,jclass,jbyteArray value){return string(e,tp::label(tp::matchFromCode(bytes(e,value))));}

#include "playback/ListeningMixCore.hpp"
#include "playback/QueueMergeCore.hpp"
extern "C" JNIEXPORT jintArray JNICALL Java_app_tonal_android_ListeningMixNative_alignQueue(JNIEnv *e,jclass,jobjectArray prepared,jobjectArray observed){
 if(!prepared||!observed||e->GetArrayLength(prepared)>500||e->GetArrayLength(observed)>101)return nullptr;
 auto read=[&](jobjectArray array){std::vector<std::string> out;for(jsize n=0;n<e->GetArrayLength(array);++n){auto value=static_cast<jstring>(e->GetObjectArrayElement(array,n));auto s=fromString(e,value);if(value)e->DeleteLocalRef(value);if(s.empty()||s.size()>256)throw std::length_error("URI inválida na fila");out.push_back(std::move(s));}return out;};
 try{auto old=read(prepared),seen=read(observed);auto result=tonal::queue::align(old,seen);result.matches.push_back(result.suffix);auto a=e->NewIntArray(jsize(result.matches.size()));e->SetIntArrayRegion(a,0,jsize(result.matches.size()),result.matches.data());return a;}catch(const std::exception &x){error(e,x);return nullptr;}
}
namespace {
jintArray mixInts(JNIEnv*e,const std::vector<int>&v){auto a=e->NewIntArray(jsize(v.size()));e->SetIntArrayRegion(a,0,jsize(v.size()),v.data());return a;}
std::vector<tonal::mix::Track> mixTracks(JNIEnv*e,jobjectArray array,int limit){std::vector<tonal::mix::Track> out;if(!array)return out;int n=e->GetArrayLength(array);if(n%7||n>limit*7)return out;for(int i=0;i<n;i+=7){std::string f[7];for(int j=0;j<7;++j){auto s=static_cast<jstring>(e->GetObjectArrayElement(array,i+j));f[j]=fromString(e,s);e->DeleteLocalRef(s);}int year=std::atoi(f[4].c_str()),heard=std::atoi(f[5].c_str());out.push_back({f[0],f[1],f[2],f[3],year,bool(heard&1),bool(heard&2),unsigned(std::atoi(f[6].c_str())),bool(heard&4)});}return out;}
}
extern "C" JNIEXPORT jintArray JNICALL Java_app_tonal_android_ListeningMixNative_order(JNIEnv*e,jclass,jint count,jint seed){return mixInts(e,tonal::mix::order(count,seed));}
extern "C" JNIEXPORT jintArray JNICALL Java_app_tonal_android_ListeningMixNative_plan(JNIEnv*e,jclass,jobjectArray source,jobjectArray pool,jint selected,jboolean random,jboolean discover,jint seed,jboolean album){try{return mixInts(e,tonal::mix::plan(mixTracks(e,source,500),mixTracks(e,pool,120),selected,random,discover,seed,album));}catch(const std::exception&x){error(e,x);return nullptr;}}
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_ListeningMixNative_queries(JNIEnv*e,jclass,jstring genres,jint year){return strings(e,tonal::mix::queries(fromString(e,genres),year));}
