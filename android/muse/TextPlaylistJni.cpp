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
extern "C" JNIEXPORT jobjectArray JNICALL Java_app_tonal_android_TextPlaylistNative_queries(JNIEnv *e,jclass,jbyteArray artist,jbyteArray title){return strings(e,tonal::playlist::queries({bytes(e,artist),bytes(e,title),{}}));}
extern "C" JNIEXPORT jdoubleArray JNICALL Java_app_tonal_android_TextPlaylistNative_rank(JNIEnv *e,jclass,jbyteArray artist,jbyteArray title,jobjectArray titles,jobjectArray artists,jintArray popularity){
    if(!titles||!artists||!popularity)return nullptr;int count=e->GetArrayLength(titles);if(count>10||count!=e->GetArrayLength(artists)||count!=e->GetArrayLength(popularity))return nullptr;
    std::vector<jint> pop(count);e->GetIntArrayRegion(popularity,0,count,pop.data());std::vector<tonal::playlist::Candidate> input;
    for(int i=0;i<count;++i){auto t=static_cast<jstring>(e->GetObjectArrayElement(titles,i)),a=static_cast<jstring>(e->GetObjectArrayElement(artists,i));input.push_back({fromString(e,t),fromString(e,a),pop[i]});e->DeleteLocalRef(t);e->DeleteLocalRef(a);}
    auto ranked=tonal::playlist::rank({bytes(e,artist),bytes(e,title),{}},input);std::vector<jdouble> out;for(const auto &r:ranked){out.push_back(r.index);out.push_back(r.score);out.push_back(r.highConfidence?1:0);}auto result=e->NewDoubleArray(jsize(out.size()));e->SetDoubleArrayRegion(result,0,jsize(out.size()),out.data());return result;
}
