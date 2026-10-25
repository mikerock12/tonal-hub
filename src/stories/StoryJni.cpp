#include "LocalStoryModel.hpp"
#include <jni.h>
#include <string>
#include <sys/resource.h>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
static jbyteArray response(JNIEnv *e,const std::string &value){auto out=e->NewByteArray(value.size());if(out)e->SetByteArrayRegion(out,0,value.size(),reinterpret_cast<const jbyte*>(value.data()));return out;}
static std::string bytes(JNIEnv *e,jbyteArray a){if(!a||e->GetArrayLength(a)>16384)return {};std::string s(e->GetArrayLength(a),'\0');e->GetByteArrayRegion(a,0,s.size(),reinterpret_cast<jbyte*>(s.data()));return s;}
extern "C" JNIEXPORT jbyteArray JNICALL Java_app_tonal_android_StoryInferenceService_generate(JNIEnv *e,jclass,jbyteArray path,jobjectArray input,jint budget){
 // Runs only while playback is paused. Lower CPU priority only. Android's Java BACKGROUND task profile also
 // demotes mmap I/O and can exhaust the complete analysis budget while loading.
 errno=0;int priority=getpriority(PRIO_PROCESS,0);
 if(errno||(priority<2&&setpriority(PRIO_PROCESS,0,2)!=0))return response(e,"ERROR:Não foi possível reduzir a prioridade da análise local.");
 if(std::getenv("TONAL_STORY_TRACE"))std::fprintf(stderr,"story-cpu-nice=%d\n",getpriority(PRIO_PROCESS,0));
 if(!input||e->GetArrayLength(input)>8)return nullptr;std::vector<std::string> texts;
 for(int i=0;i<e->GetArrayLength(input);++i){auto a=static_cast<jbyteArray>(e->GetObjectArrayElement(input,i));texts.push_back(bytes(e,a));e->DeleteLocalRef(a);}
 auto r=tonal::stories::generate(bytes(e,path),texts,budget);std::string result=r.error.empty()?r.text:"ERROR:"+r.error;
 return response(e,result);
}
