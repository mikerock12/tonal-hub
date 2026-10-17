#include <jni.h>
#include "TinyMusicModel.h"
#include <string>
extern "C" JNIEXPORT jfloatArray JNICALL Java_app_tonal_android_MusicInsightModel_predict(JNIEnv *env,jclass,jbyteArray bytes){
  if(!bytes||env->GetArrayLength(bytes)>48000)return nullptr;
  std::string text(env->GetArrayLength(bytes),'\0');env->GetByteArrayRegion(bytes,0,text.size(),reinterpret_cast<jbyte*>(text.data()));
  const auto scores=tonal::muse::analyze(text);auto result=env->NewFloatArray(scores.size());env->SetFloatArrayRegion(result,0,scores.size(),scores.data());return result;
}
