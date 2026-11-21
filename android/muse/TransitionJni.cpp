// Ponte JNI do mixer de transições (mesmo núcleo C++ do Windows).
// As chamadas de push/pull ficam no thread do PCM do Android; o Java
// serializa o acesso ao handle com um único monitor.
#include <jni.h>
#include "playback/TransitionMixer.hpp"
#include <vector>

using tonal::playback::TransitionMixer;
namespace {
TransitionMixer *mixer(jlong handle) { return reinterpret_cast<TransitionMixer *>(handle); }
}

extern "C" JNIEXPORT jlong JNICALL Java_app_tonal_android_TransitionNative_create(JNIEnv *, jclass, jint rate) {
  try { return reinterpret_cast<jlong>(new TransitionMixer(rate)); } catch (...) { return 0; }
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TransitionNative_destroy(JNIEnv *, jclass, jlong handle) {
  delete mixer(handle);
}
extern "C" JNIEXPORT jboolean JNICALL Java_app_tonal_android_TransitionNative_arm(JNIEnv *, jclass, jlong handle, jint effect, jlong upcomingFrames, jlong outroFrames, jlong introFrames, jboolean exact) {
  if (!handle || effect < 0 || effect >= tonal::playback::kTransitionEffectCount) return JNI_FALSE;
  return mixer(handle)->arm(tonal::playback::TransitionEffect(effect), upcomingFrames, outroFrames, introFrames, exact == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TransitionNative_boundary(JNIEnv *, jclass, jlong handle) {
  if (handle) mixer(handle)->boundary();
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TransitionNative_reset(JNIEnv *, jclass, jlong handle) {
  if (handle) mixer(handle)->reset();
}
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TransitionNative_endCapture(JNIEnv *, jclass, jlong handle) {
  if (handle) mixer(handle)->endCapture();
}
extern "C" JNIEXPORT jboolean JNICALL Java_app_tonal_android_TransitionNative_wantsInput(JNIEnv *, jclass, jlong handle) {
  return handle && mixer(handle)->wantsInput() ? JNI_TRUE : JNI_FALSE;
}
// 0 parado, 1 armada (lendo A / aguardando a mescla), 2 mesclando, 3 só o atraso no FIFO.
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_state(JNIEnv *, jclass, jlong handle) {
  if (!handle) return 0;
  auto *m = mixer(handle);
  if (!m->active()) return 0;
  switch (m->state()) {
  case TransitionMixer::State::Armed: return 1;
  case TransitionMixer::State::Mixing: return 2;
  default: return 3;
  }
}
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_push(JNIEnv *env, jclass, jlong handle, jfloatArray samples, jint offset, jint count) {
  if (!handle || !samples || offset < 0 || count <= 0 || offset + count > env->GetArrayLength(samples)) return 0;
  jfloat *data = env->GetFloatArrayElements(samples, nullptr);
  if (!data) return 0;
  const size_t used = mixer(handle)->push(data + offset, size_t(count) / 2);
  env->ReleaseFloatArrayElements(samples, data, JNI_ABORT);
  return jint(used * 2);
}
extern "C" JNIEXPORT jfloatArray JNICALL Java_app_tonal_android_TransitionNative_pull(JNIEnv *env, jclass, jlong handle, jint frames) {
  if (!handle || frames <= 0) return env->NewFloatArray(0);
  std::vector<float> out(size_t(frames) * 2);
  const size_t got = mixer(handle)->pull(out.data(), size_t(frames));
  jfloatArray result = env->NewFloatArray(jsize(got * 2));
  if (result && got) env->SetFloatArrayRegion(result, 0, jsize(got * 2), out.data());
  return result;
}
extern "C" JNIEXPORT jlong JNICALL Java_app_tonal_android_TransitionNative_latency(JNIEnv *, jclass, jlong handle) {
  return handle ? jlong(mixer(handle)->latencyFrames()) : 0;
}
extern "C" JNIEXPORT jlongArray JNICALL Java_app_tonal_android_TransitionNative_plan(JNIEnv *env, jclass, jint outroSeconds, jint introSeconds, jlong duration, jlong audible, jlong upcoming) {
  const auto plan = tonal::playback::planTransition(outroSeconds, introSeconds, duration, audible, upcoming);
  jlong values[4] = {plan.arm ? 1 : 0, plan.upcomingMs, plan.outroMs, plan.introMs};
  jlongArray result = env->NewLongArray(4);
  if (result) env->SetLongArrayRegion(result, 0, 4, values);
  return result;
}
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_pickRandom(JNIEnv *, jclass, jint previous, jint random) {
  return jint(tonal::playback::pickRandomTransition(tonal::playback::TransitionEffect(previous), uint32_t(random)));
}
