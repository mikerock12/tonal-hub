// Ponte JNI do mixer de transições (mesmo núcleo C++ do Windows).
// As chamadas de push/pull ficam no thread do PCM do Android; o Java
// serializa o acesso ao handle com um único monitor. Desde a 0.17.4 push e
// pull trabalham direto nos arrays Java (região crítica, sem cópia nem
// alocação por bloco): o Java reutiliza o mesmo float[] de saída.
#include <jni.h>
#include "playback/TransitionMixer.hpp"
#include <algorithm>

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
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_rate(JNIEnv *, jclass, jlong handle) {
  return handle ? mixer(handle)->sampleRate() : 0;
}
extern "C" JNIEXPORT jboolean JNICALL Java_app_tonal_android_TransitionNative_arm(JNIEnv *, jclass, jlong handle, jint effect, jlong upcomingFrames, jlong outroFrames, jlong introFrames, jboolean exact, jlong id) {
  if (!handle) return JNI_FALSE;
  return mixer(handle)->arm(tonal::playback::TransitionEffect(effect), upcomingFrames, outroFrames, introFrames,
                            exact == JNI_TRUE, uint64_t(id)) ? JNI_TRUE : JNI_FALSE;
}
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_rejection(JNIEnv *, jclass, jlong handle) {
  return handle ? jint(mixer(handle)->lastRejection()) : 0;
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
extern "C" JNIEXPORT void JNICALL Java_app_tonal_android_TransitionNative_resumeCapture(JNIEnv *, jclass, jlong handle) {
  if (handle) mixer(handle)->resumeCapture();
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
  auto *data = static_cast<jfloat *>(env->GetPrimitiveArrayCritical(samples, nullptr));
  if (!data) return 0;
  const size_t used = mixer(handle)->push(data + offset, size_t(count) / 2);
  env->ReleasePrimitiveArrayCritical(samples, data, JNI_ABORT);
  return jint(used * 2);
}
// Escreve até `frames` quadros em `out` (reutilizado pelo Java); retorna quantos.
extern "C" JNIEXPORT jint JNICALL Java_app_tonal_android_TransitionNative_pullInto(JNIEnv *env, jclass, jlong handle, jfloatArray out, jint frames) {
  if (!handle || !out || frames <= 0) return 0;
  frames = std::min<jint>(frames, env->GetArrayLength(out) / 2);
  auto *data = static_cast<jfloat *>(env->GetPrimitiveArrayCritical(out, nullptr));
  if (!data) return 0;
  const size_t got = mixer(handle)->pull(data, size_t(frames));
  env->ReleasePrimitiveArrayCritical(out, data, 0);
  return jint(got);
}
extern "C" JNIEXPORT jlong JNICALL Java_app_tonal_android_TransitionNative_latency(JNIEnv *, jclass, jlong handle) {
  return handle ? jlong(mixer(handle)->latencyFrames()) : 0;
}
// {id, fase, motivo, efeito, taxa, saída pedida, entrada pedida, saída, entrada,
//  região, quadro do marcador, B guardada, espera, quadros mesclados, quadros
//  com B, B faltando, energia A x1000, energia B x1000} — chamado fora do PCM.
extern "C" JNIEXPORT jlongArray JNICALL Java_app_tonal_android_TransitionNative_trace(JNIEnv *env, jclass, jlong handle) {
  jlong values[18] = {};
  if (handle) {
    const auto &t = mixer(handle)->trace();
    const jlong filled[18] = {jlong(t.id), jlong(t.phase), jlong(t.reason), t.effect, t.sampleRate,
                              t.configuredOutro, t.configuredIntro, t.outro, t.intro, t.overlap,
                              t.boundaryFrame, t.preparedB, t.waitedFrames, t.mixedFrames, t.bFrames,
                              t.missingBFrames, jlong(t.energyA * 1000), jlong(t.energyB * 1000)};
    std::copy(filled, filled + 18, values);
  }
  jlongArray result = env->NewLongArray(18);
  if (result) env->SetLongArrayRegion(result, 0, 18, values);
  return result;
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
