#include "LocalStoryModel.hpp"
#include <jni.h>
#include <string>
static std::string bytes(JNIEnv *e,jbyteArray a){if(!a||e->GetArrayLength(a)>16384)return {};std::string s(e->GetArrayLength(a),'\0');e->GetByteArrayRegion(a,0,s.size(),reinterpret_cast<jbyte*>(s.data()));return s;}
extern "C" JNIEXPORT jbyteArray JNICALL Java_app_tonal_android_StoryInferenceService_generate(JNIEnv *e,jclass,jbyteArray path,jobjectArray input,jint budget){
 if(!input||e->GetArrayLength(input)>8)return nullptr;std::vector<std::string> texts;
 for(int i=0;i<e->GetArrayLength(input);++i){auto a=static_cast<jbyteArray>(e->GetObjectArrayElement(input,i));texts.push_back(bytes(e,a));e->DeleteLocalRef(a);}
 auto r=tonal::stories::generate(bytes(e,path),texts,budget);std::string result=r.error.empty()?r.text:"ERROR:"+r.error;
 auto out=e->NewByteArray(result.size());e->SetByteArrayRegion(out,0,result.size(),reinterpret_cast<const jbyte*>(result.data()));return out;
}
