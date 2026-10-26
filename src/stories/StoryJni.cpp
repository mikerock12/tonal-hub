#include "LocalStoryModel.hpp"
#include "CpuDispatch.hpp"
#include <jni.h>
#include <string>
#include <sys/resource.h>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#ifdef __aarch64__
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
static jbyteArray response(JNIEnv *e,const std::string &value){auto out=e->NewByteArray(value.size());if(out)e->SetByteArrayRegion(out,0,value.size(),reinterpret_cast<const jbyte*>(value.data()));return out;}
static std::string bytes(JNIEnv *e,jbyteArray a){if(!a||e->GetArrayLength(a)>16384)return {};std::string s(e->GetArrayLength(a),'\0');e->GetByteArrayRegion(a,0,s.size(),reinterpret_cast<jbyte*>(s.data()));return s;}
#ifndef TONAL_STORY_ARM_FAST
extern "C" JNIEXPORT jboolean JNICALL Java_app_tonal_android_StoryInferenceService_supportsAcceleratedCpu(JNIEnv*,jclass){
#ifdef __aarch64__
 const auto caps=getauxval(AT_HWCAP),caps2=getauxval(AT_HWCAP2);
 return tonal::stories::acceleratedReaderSupported(true,caps&HWCAP_ASIMD,caps&HWCAP_FPHP,caps&HWCAP_ASIMDHP,caps&HWCAP_ASIMDDP,caps2&HWCAP2_I8MM,caps&HWCAP_ATOMICS);
#else
 return false;
#endif
}
#define TONAL_GENERATE_NAME Java_app_tonal_android_StoryInferenceService_generate
#else
#define TONAL_GENERATE_NAME Java_app_tonal_android_StoryInferenceService_generateFast
#endif
extern "C" JNIEXPORT jbyteArray JNICALL TONAL_GENERATE_NAME(JNIEnv *e,jclass,jbyteArray path,jobjectArray input,jobjectArray previous,jint budget,jbyteArray checkpoint,jbyteArray key){
 // Runs only while playback is paused. Lower CPU priority only. Android's Java BACKGROUND task profile also
 // demotes mmap I/O and can exhaust the complete analysis budget while loading.
 errno=0;int priority=getpriority(PRIO_PROCESS,0);
 if(errno||(priority<2&&setpriority(PRIO_PROCESS,0,2)!=0))return response(e,"ERROR:Não foi possível reduzir a prioridade da análise local.");
 if(std::getenv("TONAL_STORY_TRACE"))std::fprintf(stderr,"story-cpu-nice=%d\n",getpriority(PRIO_PROCESS,0));
 if(!input||e->GetArrayLength(input)>8)return nullptr;std::vector<std::string> texts;
 for(int i=0;i<e->GetArrayLength(input);++i){auto a=static_cast<jbyteArray>(e->GetObjectArrayElement(input,i));texts.push_back(bytes(e,a));e->DeleteLocalRef(a);}
 std::vector<std::string> known;if(previous){if(e->GetArrayLength(previous)!=e->GetArrayLength(input))return response(e,"ERROR:Lote de cache incompatível.");for(int i=0;i<e->GetArrayLength(previous);++i){auto a=static_cast<jbyteArray>(e->GetObjectArrayElement(previous,i));known.push_back(bytes(e,a));e->DeleteLocalRef(a);}}
 auto r=tonal::stories::generate(bytes(e,path),texts,budget,known,bytes(e,checkpoint),bytes(e,key));std::string result=tonal::stories::reportJson(r);
 return response(e,result);
}
