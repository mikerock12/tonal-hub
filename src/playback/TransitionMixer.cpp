#include "TransitionMixer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace tonal::playback {
namespace {
constexpr size_t kFifoFrames = 32768;
constexpr size_t kFifoLow = 2048;
constexpr int kExtraTailSeconds = 4;
constexpr double kPi = std::numbers::pi;

double smooth(double from, double to, double x) {
  const double t = std::clamp((x - from) / (to - from), 0.0, 1.0);
  return t * t * (3 - 2 * t);
}
// Mantém picos da soma A+B abaixo de 0 dBFS sem cortar seco.
float soft(float x) {
  const float a = std::fabs(x);
  if (a <= 0.9f)
    return x;
  const float shaped = 0.9f + 0.1f * std::tanh((a - 0.9f) / 0.1f);
  return std::copysign(shaped, x);
}
constexpr std::array<std::string_view, kTransitionEffectCount> kIds{
    "fade", "echo", "filter", "brake", "reverb", "roll"};
} // namespace

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

TransitionPlan planTransition(int seconds, int64_t durationMs, int64_t remainingMs,
                              int64_t bufferedMs) {
  TransitionPlan plan;
  if (durationMs <= 0 || remainingMs <= 0)
    return plan;
  bufferedMs = std::max<int64_t>(0, bufferedMs);
  const int64_t overlap = std::min<int64_t>(clampTransitionSeconds(seconds) * 1000LL, durationMs / 3);
  if (overlap < 1500)
    return plan;
  constexpr int64_t cushion = 250;
  if (remainingMs > overlap + bufferedMs + cushion)
    return plan;
  const int64_t tail = remainingMs - bufferedMs;
  if (tail < 1000)
    return plan;
  plan.arm = true;
  plan.tailMs = tail;
  plan.overlapMs = std::min(tail, overlap);
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
  m_tail.assign(size_t(kTransitionMaxSeconds + kExtraTailSeconds) * size_t(m_rate) * 2, 0.f);
  m_fifo.assign(kFifoFrames * 2, 0.f);
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
}

void TransitionMixer::reset() {
  m_state = State::Idle;
  m_tailTarget = m_tailCount = m_tailStart = 0;
  m_tailRead = 0;
  m_overlap = m_mixed = 0;
  m_fifoHead = m_fifoCount = 0;
  clearEffects();
}

void TransitionMixer::endCapture() {
  if (m_state != State::Capturing)
    return;
  m_tailTarget = m_tailCount;
  beginMixing();
}

bool TransitionMixer::arm(TransitionEffect effect, int64_t tailFrames) {
  // O FIFO ainda guarda áudio anterior ao trecho de A: tente no próximo ciclo.
  if (m_state != State::Idle || m_fifoCount > 0 || int(effect) < 0 ||
      int(effect) >= kTransitionEffectCount)
    return false;
  const int64_t capacity = int64_t(m_tail.size() / 2);
  if (tailFrames < m_rate / 2)
    return false;
  clearEffects();
  m_effect = effect;
  m_tailTarget = std::min(tailFrames, capacity);
  m_tailCount = m_tailStart = m_mixed = m_overlap = 0;
  m_tailRead = 0;
  m_state = State::Capturing;
  return true;
}

bool TransitionMixer::wantsInput() const {
  switch (m_state) {
  case State::Capturing:
    return m_tailCount < m_tailTarget;
  case State::Mixing:
    return m_fifoCount < kFifoLow;
  case State::Idle:
    return false;
  }
  return false;
}

size_t TransitionMixer::push(const float *in, size_t frames) {
  size_t used = 0;
  if (m_state == State::Capturing) {
    const size_t room = size_t(m_tailTarget - m_tailCount);
    const size_t take = std::min(room, frames);
    std::copy(in, in + take * 2, m_tail.begin() + ptrdiff_t(m_tailCount * 2));
    m_tailCount += int64_t(take);
    used = take;
    if (m_tailCount >= m_tailTarget)
      beginMixing();
  }
  while (used < frames && m_fifoCount < kFifoFrames) {
    const size_t tail = (m_fifoHead + m_fifoCount) % kFifoFrames;
    m_fifo[tail * 2] = in[used * 2];
    m_fifo[tail * 2 + 1] = in[used * 2 + 1];
    ++m_fifoCount;
    ++used;
  }
  return used;
}

void TransitionMixer::beginMixing() {
  m_tailStart = int64_t(m_tailRead);
  m_overlap = m_tailCount - m_tailStart;
  m_mixed = 0;
  if (m_overlap <= 0) {
    finishMixing();
    return;
  }
  if (m_overlap < m_rate / 4)
    m_effect = TransitionEffect::Fade;
  m_state = State::Mixing;
}

void TransitionMixer::finishMixing() {
  m_state = State::Idle;
  m_tailTarget = m_tailCount = m_tailStart = 0;
  m_tailRead = 0;
  m_overlap = m_mixed = 0;
}

double TransitionMixer::progress() const {
  if (m_state != State::Mixing || m_overlap <= 0)
    return 0;
  return double(m_mixed) / double(m_overlap);
}

size_t TransitionMixer::bufferedFrames() const {
  const int64_t tail = m_state == State::Idle ? 0 : std::max<int64_t>(0, m_tailCount - int64_t(m_tailRead));
  return size_t(tail) + m_fifoCount;
}

void TransitionMixer::tailAt(double position, float &l, float &r) const {
  const int64_t last = std::max<int64_t>(0, m_tailCount - 1);
  position = std::clamp(position, 0.0, double(last));
  const int64_t i = int64_t(position);
  const int64_t j = std::min(i + 1, last);
  const float t = float(position - double(i));
  l = m_tail[size_t(i * 2)] * (1 - t) + m_tail[size_t(j * 2)] * t;
  r = m_tail[size_t(i * 2 + 1)] * (1 - t) + m_tail[size_t(j * 2 + 1)] * t;
}

void TransitionMixer::mixFrame(float bL, float bR, bool haveB, float &outL, float &outR) {
  const double p = double(m_mixed) / double(m_overlap);
  float aL = 0, aR = 0;
  double gB = 1;
  const bool updateFilters = m_filterCountdown-- <= 0;
  if (updateFilters)
    m_filterCountdown = 32;
  switch (m_effect) {
  case TransitionEffect::Fade: {
    tailAt(double(m_tailStart + m_mixed), aL, aR);
    const double gA = std::cos(p * kPi / 2);
    aL *= float(gA);
    aR *= float(gA);
    gB = std::sin(p * kPi / 2);
    break;
  }
  case TransitionEffect::Echo: {
    tailAt(double(m_tailStart + m_mixed), aL, aR);
    const double dry = 1 - smooth(0.2, 0.62, p);
    const double send = smooth(0.0, 0.35, p) * (1 - smooth(0.55, 0.7, p));
    const double feedback = 0.6 * (1 - smooth(0.78, 1.0, p));
    const double wet = 0.85 * (1 - smooth(0.9, 1.0, p));
    const float dl = m_delayL[m_delayIndex], dr = m_delayR[m_delayIndex];
    const float mono = float((aL + aR) * 0.5 * send);
    // Pingue-pongue: cada repetição troca de lado.
    m_delayL[m_delayIndex] = mono + float(feedback) * dr;
    m_delayR[m_delayIndex] = float(feedback) * dl;
    m_delayIndex = (m_delayIndex + 1) % m_delayFrames;
    aL = float(aL * dry + dl * wet);
    aR = float(aR * dry + dr * wet);
    gB = std::sin(kPi / 2 * smooth(0.12, 0.85, p));
    break;
  }
  case TransitionEffect::FilterSweep: {
    tailAt(double(m_tailStart + m_mixed), aL, aR);
    if (updateFilters) {
      m_lowA.lowpass(m_rate, 18000 * std::pow(250.0 / 18000.0, smooth(0, 0.85, p)), 1.05);
      m_highB.highpass(m_rate, 1800 * std::pow(25.0 / 1800.0, smooth(0.25, 0.95, p)), 0.707);
    }
    const double gA = 1 - smooth(0.7, 1.0, p);
    aL = float(m_lowA.run(0, aL) * gA);
    aR = float(m_lowA.run(1, aR) * gA);
    bL = m_highB.run(0, bL);
    bR = m_highB.run(1, bR);
    gB = smooth(0.0, 0.45, p);
    break;
  }
  case TransitionEffect::VinylBrake: {
    constexpr double brakeEnd = 0.5;
    if (p < brakeEnd) {
      const double q = p / brakeEnd;
      const double speed = std::pow(1 - q, 1.6);
      if (m_mixed == 0)
        m_tailRead = double(m_tailStart);
      tailAt(m_tailRead, aL, aR);
      m_tailRead += speed;
      if (updateFilters)
        m_lowA.lowpass(m_rate, 300 + 17700 * speed, 0.707);
      const double gA = 1 - smooth(0.75, 1.0, q);
      aL = float(m_lowA.run(0, aL) * gA);
      aR = float(m_lowA.run(1, aR) * gA);
    }
    if (updateFilters)
      m_highB.highpass(m_rate, 500, 0.707);
    const float hl = m_highB.run(0, bL), hr = m_highB.run(1, bR);
    const double drop = smooth(brakeEnd, brakeEnd + 0.08, p);
    bL = float(hl * (1 - drop) + bL * drop);
    bR = float(hr * (1 - drop) + bR * drop);
    gB = 0.3 * smooth(0.1, brakeEnd, p) * (1 - drop) + drop;
    break;
  }
  case TransitionEffect::ReverbWash: {
    tailAt(double(m_tailStart + m_mixed), aL, aR);
    const double dry = 1 - smooth(0.15, 0.6, p);
    const double send = smooth(0.0, 0.4, p) * (1 - smooth(0.6, 0.75, p));
    const float feedback = float(0.86 * (1 - 0.35 * smooth(0.7, 1.0, p)));
    const double wet = 0.9 * (1 - smooth(0.85, 1.0, p));
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
    gB = std::sin(kPi / 2 * smooth(0.1, 0.9, p));
    break;
  }
  case TransitionEffect::BeatRoll: {
    // Comprimentos em segundos de cada etapa; < 0 encerra A.
    constexpr std::array<double, 7> starts{0.0, 0.2, 0.4, 0.55, 0.67, 0.76, 0.82};
    constexpr std::array<double, 7> lengths{0, 0.5, 0.25, 0.125, 0.0625, 0.03125, -1};
    int64_t stage = 0;
    for (size_t i = 0; i < starts.size(); ++i)
      if (p >= starts[i])
        stage = int64_t(i);
    const double length = lengths[size_t(stage)];
    if (stage == 0) {
      tailAt(double(m_tailStart + m_mixed), aL, aR);
    } else if (length > 0) {
      if (stage != m_rollStage) {
        m_rollLength = std::max<int64_t>(64, int64_t(length * m_rate));
        if (m_rollStage <= 0)
          m_rollStart = m_tailStart + m_mixed;
        m_rollStart = std::clamp<int64_t>(m_rollStart, 0, std::max<int64_t>(0, m_tailCount - m_rollLength));
        m_rollPhase = 0;
        m_rollStage = stage;
      }
      tailAt(double(m_rollStart + m_rollPhase), aL, aR);
      const double edge = std::min({1.0, double(m_rollPhase) / 64.0,
                                    double(m_rollLength - m_rollPhase) / 64.0});
      const double gA = edge * (1 - smooth(0.78, 0.84, p));
      aL *= float(gA);
      aR *= float(gA);
      m_rollPhase = (m_rollPhase + 1) % m_rollLength;
    }
    gB = 0.25 * smooth(0.05, 0.6, p) + 0.75 * smooth(0.8, 0.86, p);
    break;
  }
  case TransitionEffect::Count:
    break;
  }
  if (!haveB)
    bL = bR = 0;
  outL = soft(float(aL + bL * gB));
  outR = soft(float(aR + bR * gB));
}

size_t TransitionMixer::pull(float *out, size_t frames, bool padMissing) {
  size_t produced = 0;
  while (produced < frames) {
    if (m_state == State::Capturing) {
      // Antes da mesclagem, A toca sem efeito enquanto o restante é lido.
      if (int64_t(m_tailRead) >= m_tailCount)
        break;
      const auto i = size_t(int64_t(m_tailRead));
      out[produced * 2] = m_tail[i * 2];
      out[produced * 2 + 1] = m_tail[i * 2 + 1];
      m_tailRead += 1;
      ++produced;
      continue;
    }
    if (m_state == State::Mixing) {
      const bool haveB = m_fifoCount > 0;
      if (!haveB && !padMissing)
        break;
      float bL = 0, bR = 0;
      if (haveB) {
        bL = m_fifo[m_fifoHead * 2];
        bR = m_fifo[m_fifoHead * 2 + 1];
        m_fifoHead = (m_fifoHead + 1) % kFifoFrames;
        --m_fifoCount;
      }
      mixFrame(bL, bR, haveB, out[produced * 2], out[produced * 2 + 1]);
      ++produced;
      if (++m_mixed >= m_overlap)
        finishMixing();
      continue;
    }
    if (m_fifoCount == 0)
      break;
    out[produced * 2] = m_fifo[m_fifoHead * 2];
    out[produced * 2 + 1] = m_fifo[m_fifoHead * 2 + 1];
    m_fifoHead = (m_fifoHead + 1) % kFifoFrames;
    --m_fifoCount;
    ++produced;
  }
  return produced;
}

} // namespace tonal::playback
