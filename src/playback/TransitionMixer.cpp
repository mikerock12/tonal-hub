#include "TransitionMixer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tonal::playback {
namespace {
// Mescla máx. + pré-rolagem + atraso anterior + erro da estimativa até o marcador.
constexpr int kTailSeconds = 30;
constexpr int64_t kRenderFrames = 8192; // mescla renderizada à frente da saída
constexpr int64_t kBlock = 32;          // granularidade do limitador
constexpr int64_t kLookBlocks = 8;      // antecipação de 256 quadros (~6 ms)
constexpr float kCeiling = 0.999f;
constexpr double kPi = std::numbers::pi;

double smooth(double from, double to, double x) {
  const double t = std::clamp((x - from) / (to - from), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}
constexpr std::array<std::string_view, kTransitionEffectCount> kIds{
    "fade", "echo", "filter", "brake", "reverb", "roll"};
constexpr std::array<std::string_view, 5> kPhases{"idle", "armed", "mixing", "completed", "aborted"};
constexpr std::array<std::string_view, 7> kReasons{
    "none", "busy", "invalid-request", "buffer-capacity", "no-exact-boundary", "next-not-prepared", "cancelled"};
} // namespace

std::string_view transitionPhaseId(TransitionPhase phase) {
  const auto index = size_t(phase);
  return index < kPhases.size() ? kPhases[index] : std::string_view{};
}

std::string_view transitionReasonId(TransitionReason reason) {
  const auto index = size_t(reason);
  return index < kReasons.size() ? kReasons[index] : std::string_view{};
}

std::string_view transitionEffectId(TransitionEffect effect) {
  const int index = int(effect);
  return index >= 0 && index < kTransitionEffectCount ? kIds[size_t(index)] : std::string_view{};
}

TransitionEffect transitionEffectFromId(std::string_view id) {
  for (int i = 0; i < kTransitionEffectCount; ++i)
    if (kIds[size_t(i)] == id)
      return TransitionEffect(i);
  return TransitionEffect::Count;
}

int clampTransitionSeconds(int seconds) {
  return std::clamp(seconds, kTransitionMinSeconds, kTransitionMaxSeconds);
}

TransitionEffect pickRandomTransition(TransitionEffect previous, uint32_t random) {
  const int previousIndex = int(previous);
  if (previousIndex < 0 || previousIndex >= kTransitionEffectCount)
    return TransitionEffect(int(random % uint32_t(kTransitionEffectCount)));
  const int pick = int(random % uint32_t(kTransitionEffectCount - 1));
  return TransitionEffect(pick >= previousIndex ? pick + 1 : pick);
}

TransitionPlan planTransition(int outroSeconds, int introSeconds, int64_t durationMs,
                              int64_t audibleRemainingMs, int64_t upcomingMs) {
  TransitionPlan plan;
  if (durationMs <= 0 || audibleRemainingMs <= 0)
    return plan;
  const int64_t limit = durationMs / 3;
  const int64_t outro = std::min<int64_t>(clampTransitionSeconds(outroSeconds) * 1000LL, limit);
  const int64_t intro = std::min<int64_t>(clampTransitionSeconds(introSeconds) * 1000LL, limit);
  const int64_t region = std::max(outro, intro);
  if (region < 1500)
    return plan;
  // Arma cedo para ler A e dar tempo à carga de B, sem aumentar o atraso de B.
  if (audibleRemainingMs > region + kTransitionPrerollMs + kTransitionLoadMarginMs + 500)
    return plan;
  if (audibleRemainingMs < 1000 || upcomingMs < 0)
    return plan;
  plan.arm = true;
  plan.upcomingMs = upcomingMs;
  plan.outroMs = std::min(audibleRemainingMs, outro);
  plan.introMs = std::min(audibleRemainingMs, intro);
  return plan;
}

TransitionPlan planSilenceTrim(int64_t durationMs, int64_t audibleRemainingMs, int64_t upcomingMs) {
  TransitionPlan plan;
  if (durationMs <= 0 || upcomingMs < 0 || audibleRemainingMs < 1000 ||
      audibleRemainingMs > kTransitionPrerollMs + kTransitionLoadMarginMs + 500)
    return plan;
  plan.arm = true;
  plan.upcomingMs = upcomingMs;
  return plan;
}

void TransitionMixer::Biquad::lowpass(double rate, double hz, double q) {
  const double w = 2 * kPi * std::clamp(hz, 10.0, rate * 0.45) / rate;
  const double alpha = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + alpha;
  b0 = (1 - c) / 2 / a0;
  b1 = (1 - c) / a0;
  b2 = b0;
  a1 = -2 * c / a0;
  a2 = (1 - alpha) / a0;
}

void TransitionMixer::Biquad::highpass(double rate, double hz, double q) {
  const double w = 2 * kPi * std::clamp(hz, 10.0, rate * 0.45) / rate;
  const double alpha = std::sin(w) / (2 * q), c = std::cos(w), a0 = 1 + alpha;
  b0 = (1 + c) / 2 / a0;
  b1 = -(1 + c) / a0;
  b2 = b0;
  a1 = -2 * c / a0;
  a2 = (1 - alpha) / a0;
}

float TransitionMixer::Biquad::run(int channel, float x) {
  // Transposta direta II: estável com coeficientes variando a cada bloco.
  const double y = b0 * x + z1[channel];
  z1[channel] = b1 * x - a1 * y + z2[channel];
  z2[channel] = b2 * x - a2 * y;
  return float(y);
}

TransitionMixer::TransitionMixer(int sampleRate) { setSampleRate(sampleRate); }

void TransitionMixer::setSampleRate(int sampleRate) {
  sampleRate = std::clamp(sampleRate, 8000, 96000);
  if (sampleRate == m_rate && !m_tail.empty()) {
    reset();
    return;
  }
  m_rate = sampleRate;
  m_rampFrames = std::max<int64_t>(1, std::llround(m_rate * 0.010));
  m_tail.assign(size_t(kTailSeconds) * size_t(m_rate) * 2, 0.f);
  m_fifoLimit = size_t(int64_t(m_rate) * kTransitionPrerollMs / 1000);
  m_fifoCapacity = m_fifoLimit + 16384;
  m_fifo.assign(m_fifoCapacity * 2, 0.f);
  m_render.assign(size_t(kRenderFrames) * 2, 0.f);
  m_blockNeed.assign(size_t(kRenderFrames / kBlock), 1.f);
  m_delayFrames = size_t(m_rate * 0.36);
  m_delayL.assign(m_delayFrames, 0.f);
  m_delayR.assign(m_delayFrames, 0.f);
  const double scale = m_rate / 44100.0;
  constexpr std::array<int, 4> combTuning{1116, 1188, 1277, 1356};
  constexpr std::array<int, 2> allpassTuning{556, 441};
  for (size_t i = 0; i < 8; ++i)
    m_combs[i].line.assign(size_t((combTuning[i % 4] + (i >= 4 ? 23 : 0)) * scale), 0.f);
  for (size_t i = 0; i < 4; ++i)
    m_allpass[i].line.assign(size_t((allpassTuning[i % 2] + (i >= 2 ? 23 : 0)) * scale), 0.f);
  reset();
}

void TransitionMixer::clearEffects() {
  std::fill(m_delayL.begin(), m_delayL.end(), 0.f);
  std::fill(m_delayR.begin(), m_delayR.end(), 0.f);
  m_delayIndex = 0;
  m_lowA = {};
  m_highB = {};
  m_filterCountdown = 0;
  for (auto &comb : m_combs) {
    std::fill(comb.line.begin(), comb.line.end(), 0.f);
    comb.index = 0;
    comb.store = 0;
  }
  for (auto &pass : m_allpass) {
    std::fill(pass.line.begin(), pass.line.end(), 0.f);
    pass.index = 0;
  }
  m_rollStart = m_rollLength = m_rollPhase = 0;
  m_rollStage = -1;
  m_brakeRead = 0;
  m_brakeStarted = false;
}

void TransitionMixer::reset() {
  if (m_state != State::Idle && m_trace.phase != TransitionPhase::Aborted)
    abort(TransitionReason::Cancelled);
  finishMixing();
  m_trimSilence = m_trimOnly = m_trimLeadingB = false;
  m_fifoHead = m_fifoCount = 0;
  clearEffects();
}

void TransitionMixer::finishMixing() {
  m_state = State::Idle;
  m_tailTarget = m_tailCount = m_tailPlayed = 0;
  m_mixStart = m_overlap = m_rendered = m_mixed = 0;
  m_outro = m_intro = m_outroWanted = m_introWanted = 0;
  m_waitBoundary = false;
  m_trimOnly = false;
  m_gain = 1;
  m_limiterSlope = 0;
}

void TransitionMixer::abort(TransitionReason reason) {
  m_trace.phase = TransitionPhase::Aborted;
  m_trace.reason = reason;
}

bool TransitionMixer::bReady() const {
  // O começo de B que a mescla usa logo de saída já está guardado.
  const int64_t ready = int64_t(m_rate) * kTransitionReadyMs / 1000;
  const int64_t need = std::max<int64_t>(1, std::min(m_intro, ready));
  return int64_t(m_fifoCount) >= need || m_fifoCount >= m_fifoLimit;
}

void TransitionMixer::fifoPop(float &l, float &r) {
  l = m_fifo[m_fifoHead * 2];
  r = m_fifo[m_fifoHead * 2 + 1];
  m_fifoHead = (m_fifoHead + 1) % m_fifoCapacity;
  --m_fifoCount;
}

bool TransitionMixer::arm(TransitionEffect effect, int64_t upcomingFrames, int64_t outroFrames,
                          int64_t introFrames, bool exactBoundary, uint64_t id, bool trimSilence) {
  if (m_state != State::Idle) {
    m_rejection = TransitionReason::Busy;
    return false;
  }
  if (int(effect) < 0 || int(effect) >= kTransitionEffectCount || upcomingFrames < 0 ||
      (outroFrames < 0 || introFrames < 0) || (!trimSilence && (outroFrames <= 0 || introFrames <= 0)) ||
      (trimSilence && ((outroFrames == 0) != (introFrames == 0)))) {
    m_rejection = TransitionReason::InvalidRequest;
    return false;
  }
  const int64_t capacity = int64_t(m_tail.size() / 2);
  const int64_t total = int64_t(m_fifoCount) + upcomingFrames;
  if (exactBoundary ? int64_t(m_fifoCount) >= capacity - m_rate : total < m_rate / 2 || total > capacity) {
    m_rejection = TransitionReason::BufferCapacity;
    return false;
  }
  m_rejection = TransitionReason::None;
  m_trace = {};
  m_trace.id = id;
  m_trace.phase = TransitionPhase::Armed;
  m_trace.effect = int(effect);
  m_trace.sampleRate = m_rate;
  m_trace.exactBoundary = exactBoundary;
  m_trace.configuredOutro = outroFrames;
  m_trace.configuredIntro = introFrames;
  m_trimSilence = trimSilence;
  m_trimOnly = trimSilence && outroFrames == 0 && introFrames == 0;
  m_trimLeadingB = false;
  clearEffects();
  // O FIFO guarda A já entregue e ainda não tocada: vira o começo do buffer.
  int64_t count = 0;
  while (m_fifoCount > 0) {
    fifoPop(m_tail[size_t(count * 2)], m_tail[size_t(count * 2 + 1)]);
    ++count;
  }
  m_fifoHead = 0;
  m_effect = m_effectWanted = effect;
  m_tailCount = count;
  m_tailPlayed = 0;
  m_outroWanted = outroFrames;
  m_introWanted = introFrames;
  m_waitBoundary = exactBoundary;
  // Com o marcador, A vai até onde ele chegar; a estimativa só posiciona a
  // região até lá (a mescla não começa antes do marcador).
  m_tailTarget = exactBoundary ? capacity : total;
  m_rendered = m_mixed = 0;
  m_gain = 1;
  m_state = State::Armed;
  placeRegion(std::clamp<int64_t>(total, count, capacity));
  return true;
}

void TransitionMixer::placeRegion(int64_t end) {
  // Só quadros estéreo exatamente zero são removidos. O cálculo espera o fim
  // real chegar ao buffer para não confundir espaço ainda não preenchido com silêncio.
  if (m_trimSilence && !m_waitBoundary && end <= m_tailCount) {
    const int64_t untrimmed = end;
    while (end > m_tailPlayed && end > 0 && m_tail[size_t((end - 1) * 2)] == 0.f &&
           m_tail[size_t((end - 1) * 2 + 1)] == 0.f)
      --end;
    m_trace.trimmedA = std::max<int64_t>(0, untrimmed - end);
  }
  if (m_trimOnly) {
    m_overlap = m_outro = m_intro = 0;
    m_mixStart = end;
    m_trace.outro = m_trace.intro = m_trace.overlap = 0;
    return;
  }
  // A região termina no último quadro de A; não pode começar no que já tocou.
  const int64_t available = std::max<int64_t>(0, end - m_tailPlayed);
  m_overlap = std::min(std::max(m_outroWanted, m_introWanted), available);
  m_outro = std::min(m_outroWanted, m_overlap);
  m_intro = std::min(m_introWanted, m_overlap);
  m_effect = m_overlap < m_rate / 4 ? TransitionEffect::Fade : m_effectWanted;
  m_mixStart = end - m_overlap;
  m_trace.outro = m_outro;
  m_trace.intro = m_intro;
  m_trace.overlap = m_overlap;
}

void TransitionMixer::boundary() {
  // Só vale enquanto A ainda está sendo lida: depois disso o que chega já é B.
  if (m_state != State::Armed || m_tailCount >= m_tailTarget)
    return;
  m_waitBoundary = false;
  m_trimLeadingB = m_trimSilence;
  m_tailTarget = m_tailCount;
  m_trace.boundaryFrame = m_tailCount;
  placeRegion(m_tailTarget);
}

void TransitionMixer::endCapture() {
  if (m_state == State::Idle || m_tailCount >= m_tailTarget)
    return;
  m_waitBoundary = false;
  m_tailTarget = m_tailCount;
  if (m_state == State::Armed)
    placeRegion(m_tailTarget);
}

void TransitionMixer::resumeCapture() {
  if (m_state != State::Armed || m_trace.boundaryFrame >= 0 || m_trace.phase != TransitionPhase::Armed)
    return;
  m_waitBoundary = true;
  m_tailTarget = int64_t(m_tail.size() / 2);
  m_outroWanted = m_trace.configuredOutro;
  m_introWanted = m_trace.configuredIntro;
  placeRegion(m_tailTarget);
}

bool TransitionMixer::wantsInput() const {
  if (m_state != State::Idle && m_tailCount < m_tailTarget)
    return true;
  return m_fifoCount < m_fifoLimit;
}

size_t TransitionMixer::push(const float *in, size_t frames) {
  size_t used = 0;
  if (m_state != State::Idle && m_tailCount < m_tailTarget) {
    const size_t take = std::min(size_t(m_tailTarget - m_tailCount), frames);
    std::copy(in, in + take * 2, m_tail.begin() + ptrdiff_t(m_tailCount * 2));
    m_tailCount += int64_t(take);
    used = take;
    // Buffer cheio sem o marcador: não dá para saber onde B começa. A toca até
    // o fim do buffer e segue sem efeito, em vez de mesclar A com ela mesma.
    if (m_waitBoundary && m_tailCount >= m_tailTarget) {
      m_waitBoundary = false;
      m_outroWanted = m_introWanted = 0;
      placeRegion(m_tailTarget);
      abort(TransitionReason::NoExactBoundary);
    }
  }
  while (used < frames && m_trimLeadingB) {
    if (in[used * 2] != 0.f || in[used * 2 + 1] != 0.f) {
      m_trimLeadingB = false;
      break;
    }
    ++used;
    ++m_trace.trimmedB;
  }
  while (used < frames && m_fifoCount < m_fifoCapacity) {
    const size_t slot = (m_fifoHead + m_fifoCount) % m_fifoCapacity;
    m_fifo[slot * 2] = in[used * 2];
    m_fifo[slot * 2 + 1] = in[used * 2 + 1];
    ++m_fifoCount;
    ++used;
  }
  return used;
}

double TransitionMixer::progress() const {
  if (m_state != State::Mixing || m_overlap <= 0)
    return 0;
  return double(m_mixed) / double(m_overlap);
}

size_t TransitionMixer::latencyFrames() const {
  // Enquanto A chega, o atraso é de A; depois, do que já chegou de B.
  if (capturing())
    return size_t(std::max<int64_t>(0, m_tailCount - m_tailPlayed));
  return m_fifoCount + size_t(m_state == State::Mixing ? m_rendered - m_mixed : 0);
}

void TransitionMixer::tailAt(double position, float &l, float &r) const {
  // A ainda não lida (motor atrasado) soa como silêncio, sem repetir amostras.
  if (position < 0 || position > double(m_tailCount - 1) || m_tailCount <= 0) {
    l = r = 0;
    return;
  }
  const int64_t i = int64_t(position);
  const int64_t j = std::min(i + 1, m_tailCount - 1);
  const float t = float(position - double(i));
  l = m_tail[size_t(i * 2)] * (1 - t) + m_tail[size_t(j * 2)] * t;
  r = m_tail[size_t(i * 2 + 1)] * (1 - t) + m_tail[size_t(j * 2 + 1)] * t;
}

void TransitionMixer::renderAhead(int64_t until) {
  until = std::min({until, m_overlap, m_mixed + kRenderFrames});
  const int64_t bStart = m_overlap - m_intro;
  while (m_rendered < until) {
    float bL = 0, bR = 0;
    // B entra `intro` antes do fim de A; antes disso ela espera no FIFO.
    const bool wantB = m_rendered >= bStart;
    const bool haveB = wantB && m_fifoCount > 0;
    if (haveB) {
      fifoPop(bL, bR);
      ++m_trace.bFrames;
    } else if (wantB) {
      ++m_trace.missingBFrames;  // motor atrasou B no meio da mescla
    }
    float l, r;
    mixFrame(bL, bR, haveB, l, r);
    const size_t slot = size_t(m_rendered % kRenderFrames);
    m_render[slot * 2] = l;
    m_render[slot * 2 + 1] = r;
    const size_t block = size_t((m_rendered / kBlock) % (kRenderFrames / kBlock));
    if (m_rendered % kBlock == 0)
      m_blockNeed[block] = 1.f;
    const float peak = std::max(std::fabs(l), std::fabs(r));
    if (peak > kCeiling)
      m_blockNeed[block] = std::min(m_blockNeed[block], kCeiling / peak);
    ++m_rendered;
  }
}

void TransitionMixer::mixFrame(float bL, float bR, bool haveB, float &outL, float &outR) {
  // t percorre a região inteira; A tem efeito nos últimos `outro` quadros e B
  // entra nos últimos `intro`. As duas terminam juntas no fim de A.
  const int64_t t = m_rendered;
  const int64_t aStart = m_overlap - m_outro, bStart = m_overlap - m_intro;
  const double pA = m_outro > 0 ? std::clamp(double(t - aStart) / double(m_outro), 0.0, 1.0) : 1.0;
  const double pB = m_intro > 0 ? std::clamp(double(t - bStart) / double(m_intro), 0.0, 1.0) : 1.0;
  const double aPosition = double(m_mixStart + t);
  float aL = 0, aR = 0;
  double gB = 1;
  const bool updateFilters = m_filterCountdown-- <= 0;
  if (updateFilters)
    m_filterCountdown = 32;
  switch (m_effect) {
  case TransitionEffect::Fade: {
    tailAt(aPosition, aL, aR);
    const double gA = std::cos(pA * kPi / 2);
    aL *= float(gA);
    aR *= float(gA);
    gB = std::sin(pB * kPi / 2);
    break;
  }
  case TransitionEffect::Echo: {
    tailAt(aPosition, aL, aR);
    const double dry = 1 - smooth(0.2, 0.62, pA);
    const double send = smooth(0.0, 0.35, pA) * (1 - smooth(0.55, 0.7, pA));
    const double feedback = 0.6 * (1 - smooth(0.78, 1.0, pA));
    const double wet = 0.85 * (1 - smooth(0.9, 1.0, pA));
    const float dl = m_delayL[m_delayIndex], dr = m_delayR[m_delayIndex];
    const float mono = float((aL + aR) * 0.5 * send);
    // Pingue-pongue: cada repetição troca de lado.
    m_delayL[m_delayIndex] = mono + float(feedback) * dr;
    m_delayR[m_delayIndex] = float(feedback) * dl;
    m_delayIndex = (m_delayIndex + 1) % m_delayFrames;
    aL = float(aL * dry + dl * wet);
    aR = float(aR * dry + dr * wet);
    gB = std::sin(kPi / 2 * smooth(0.12, 0.85, pB));
    break;
  }
  case TransitionEffect::FilterSweep: {
    tailAt(aPosition, aL, aR);
    if (updateFilters) {
      m_lowA.lowpass(m_rate, 18000 * std::pow(250.0 / 18000.0, smooth(0, 0.85, pA)), 1.05);
      m_highB.highpass(m_rate, 1800 * std::pow(25.0 / 1800.0, smooth(0.25, 0.95, pB)), 0.707);
    }
    const double gA = 1 - smooth(0.7, 1.0, pA);
    aL = float(m_lowA.run(0, aL) * gA);
    aR = float(m_lowA.run(1, aR) * gA);
    bL = m_highB.run(0, bL);
    bR = m_highB.run(1, bR);
    gB = smooth(0.0, 0.45, pB);
    break;
  }
  case TransitionEffect::VinylBrake: {
    // A freia nos primeiros 70% do seu trecho; B cai inteira quando A para.
    constexpr double brakeEnd = 0.7;
    if (t < aStart) {
      tailAt(aPosition, aL, aR);
    } else if (pA < brakeEnd) {
      if (!m_brakeStarted) {
        m_brakeRead = double(m_mixStart + aStart);
        m_brakeStarted = true;
      }
      const double q = pA / brakeEnd;
      const double speed = std::pow(1 - q, 1.6);
      tailAt(m_brakeRead, aL, aR);
      m_brakeRead += speed;
      if (updateFilters)
        m_lowA.lowpass(m_rate, 300 + 17700 * speed, 0.707);
      const double gA = 1 - smooth(0.75, 1.0, q);
      aL = float(m_lowA.run(0, aL) * gA);
      aR = float(m_lowA.run(1, aR) * gA);
    }
    if (updateFilters)
      m_highB.highpass(m_rate, 500, 0.707);
    const float hl = m_highB.run(0, bL), hr = m_highB.run(1, bR);
    const double drop = smooth(brakeEnd, brakeEnd + 0.08, pA);
    bL = float(hl * (1 - drop) + bL * drop);
    bR = float(hr * (1 - drop) + bR * drop);
    gB = 0.3 * smooth(0.0, 0.6, pB) * (1 - drop) + drop;
    break;
  }
  case TransitionEffect::ReverbWash: {
    tailAt(aPosition, aL, aR);
    const double dry = 1 - smooth(0.15, 0.6, pA);
    const double send = smooth(0.0, 0.4, pA) * (1 - smooth(0.6, 0.75, pA));
    const float feedback = float(0.86 * (1 - 0.35 * smooth(0.7, 1.0, pA)));
    const double wet = 0.9 * (1 - smooth(0.85, 1.0, pA));
    const float input = float((aL + aR) * 0.5 * send * 0.04);
    float wetL = 0, wetR = 0;
    for (size_t i = 0; i < 8; ++i) {
      Comb &comb = m_combs[i];
      const float y = comb.line[comb.index];
      comb.store = y * 0.75f + comb.store * 0.25f;
      comb.line[comb.index] = input + comb.store * feedback;
      comb.index = (comb.index + 1) % comb.line.size();
      (i < 4 ? wetL : wetR) += y;
    }
    for (size_t i = 0; i < 4; ++i) {
      AllPass &pass = m_allpass[i];
      float &x = i < 2 ? wetL : wetR;
      const float buffered = pass.line[pass.index];
      pass.line[pass.index] = x + buffered * 0.5f;
      pass.index = (pass.index + 1) % pass.line.size();
      x = buffered - x;
    }
    aL = float(aL * dry + wetL * wet);
    aR = float(aR * dry + wetR * wet);
    gB = std::sin(kPi / 2 * smooth(0.1, 0.9, pB));
    break;
  }
  case TransitionEffect::BeatRoll: {
    // Início de cada etapa (fração do trecho de A) e comprimento do loop em s.
    constexpr std::array<double, 7> starts{0.0, 0.2, 0.4, 0.55, 0.67, 0.76, 0.82};
    constexpr std::array<double, 7> lengths{0, 0.5, 0.25, 0.125, 0.0625, 0.03125, -1};
    int64_t stage = 0;
    if (t >= aStart)
      for (size_t i = 0; i < starts.size(); ++i)
        if (pA >= starts[i])
          stage = int64_t(i);
    const double length = lengths[size_t(stage)];
    if (stage == 0) {
      tailAt(aPosition, aL, aR);
    } else if (length > 0) {
      if (stage != m_rollStage) {
        m_rollLength = std::max<int64_t>(64, int64_t(length * m_rate));
        if (m_rollStage <= 0)
          m_rollStart = m_mixStart + t;
        m_rollStart = std::clamp<int64_t>(m_rollStart, 0, std::max<int64_t>(0, m_tailCount - m_rollLength));
        m_rollPhase = 0;
        m_rollStage = stage;
      }
      tailAt(double(m_rollStart + m_rollPhase), aL, aR);
      const double edge = std::min({1.0, double(m_rollPhase) / 64.0,
                                    double(m_rollLength - m_rollPhase) / 64.0});
      const double gA = edge * (1 - smooth(0.78, 0.84, pA));
      aL *= float(gA);
      aR *= float(gA);
      m_rollPhase = (m_rollPhase + 1) % m_rollLength;
    }
    gB = 0.25 * smooth(0.05, 0.6, pB) + 0.75 * smooth(0.8, 0.86, pA);
    break;
  }
  case TransitionEffect::Count:
    break;
  }
  if (!haveB) {
    bL = bR = 0;
  } else if (t - bStart < m_rampFrames) {
    // Rampa de 10 ms na entrada de B: nunca começa com um degrau.
    gB *= double(t - bStart + 1) / double(m_rampFrames);
  }
  const double wetL = bL * gB, wetR = bR * gB;
  m_trace.energyA += double(aL) * aL + double(aR) * aR;
  m_trace.energyB += wetL * wetL + wetR * wetR;
  // Sem saturação: o limitador com antecipação cuida dos picos da soma.
  outL = float(aL + wetL);
  outR = float(aR + wetR);
}

size_t TransitionMixer::pull(float *out, size_t frames) {
  size_t produced = 0;
  const float release = 1.f / float(m_rate / 12);  // ~80 ms
  while (produced < frames) {
    if (m_state == State::Armed) {
      // Com o marcador a mescla só começa quando se sabe onde A termina.
      if (!m_waitBoundary && m_tailPlayed >= m_mixStart) {
        if (m_overlap <= 0) {
          if (m_trimOnly) {
            m_tailPlayed = m_tailTarget;
            m_trace.phase = TransitionPhase::Completed;
            finishMixing();  // B segue do FIFO, já sem o silêncio inicial
            continue;
          }
          // Sem região: A acabou antes de B chegar (o marcador veio depois de A
          // tocar). Troca normal, e o trace termina com o motivo em vez de
          // continuar "armado".
          if (m_trace.phase == TransitionPhase::Armed)
            abort(TransitionReason::NextNotPrepared);
          finishMixing();  // B segue do FIFO
          continue;
        }
        if (bReady()) {
          m_trace.phase = TransitionPhase::Mixing;
          m_trace.preparedB = int64_t(m_fifoCount);
          m_state = State::Mixing;
          continue;
        }
        // B ainda não chegou: A segue sem efeito e a região encolhe (continua
        // terminando no último quadro de A). Sem o mínimo, troca normal.
        if (m_tailTarget - m_tailPlayed < int64_t(m_rate) * kTransitionMinOverlapMs / 1000) {
          m_outroWanted = m_introWanted = 0;
          placeRegion(m_tailTarget);
          abort(TransitionReason::NextNotPrepared);
          continue;
        }
        if (m_tailPlayed >= m_tailCount)
          break;
        out[produced * 2] = m_tail[size_t(m_tailPlayed * 2)];
        out[produced * 2 + 1] = m_tail[size_t(m_tailPlayed * 2 + 1)];
        ++m_tailPlayed;
        ++produced;
        ++m_trace.waitedFrames;
        placeRegion(m_tailTarget);
        continue;
      }
      // Antes da mescla, A toca sem efeito do próprio buffer.
      if (m_tailPlayed >= m_tailCount)
        break;
      out[produced * 2] = m_tail[size_t(m_tailPlayed * 2)];
      out[produced * 2 + 1] = m_tail[size_t(m_tailPlayed * 2 + 1)];
      ++m_tailPlayed;
      ++produced;
      continue;
    }
    if (m_state == State::Mixing) {
      renderAhead(m_mixed + kLookBlocks * kBlock + 1);
      if (m_mixed % kBlock == 0) {
        // Desce o ganho a tempo do pico mais próximo; nunca acima do necessário.
        float slope = 0;
        for (int64_t b = 1; b <= kLookBlocks; ++b) {
          const int64_t start = m_mixed + b * kBlock;
          if (start >= m_rendered)
            break;
          const float need = m_blockNeed[size_t((start / kBlock) % (kRenderFrames / kBlock))];
          if (need < m_gain)
            slope = std::max(slope, (m_gain - need) / float(b * kBlock));
        }
        m_limiterSlope = slope;
      }
      const float need = m_blockNeed[size_t((m_mixed / kBlock) % (kRenderFrames / kBlock))];
      if (m_limiterSlope > 0)
        m_gain -= m_limiterSlope;
      else
        m_gain += (1.f - m_gain) * release;
      m_gain = std::min(m_gain, need);
      const size_t slot = size_t(m_mixed % kRenderFrames);
      out[produced * 2] = m_render[slot * 2] * m_gain;
      out[produced * 2 + 1] = m_render[slot * 2 + 1] * m_gain;
      ++produced;
      m_trace.mixedFrames = ++m_mixed;
      if (m_mixed >= m_overlap) {
        m_tailPlayed = m_tailTarget;
        m_trace.phase = TransitionPhase::Completed;
        finishMixing();
      }
      continue;
    }
    if (m_fifoCount == 0)
      break;
    fifoPop(out[produced * 2], out[produced * 2 + 1]);
    ++produced;
  }
  return produced;
}

} // namespace tonal::playback
