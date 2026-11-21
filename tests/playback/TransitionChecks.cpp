// Verificações do mixer de transições (sem Qt, sem dispositivo de áudio).
#include "playback/TransitionMixer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <set>
#include <vector>

using namespace tonal::playback;

namespace {
int failures = 0;
void require(bool ok, const char *what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

constexpr int rate = 44100;

std::vector<float> tone(double hz, double seconds, float amplitude) {
  std::vector<float> out(size_t(seconds * rate) * 2);
  for (size_t i = 0; i < out.size() / 2; ++i) {
    const float v = amplitude * float(std::sin(2 * std::numbers::pi * hz * double(i) / rate));
    out[i * 2] = v;
    out[i * 2 + 1] = v;
  }
  return out;
}

double rms(const std::vector<float> &pcm, size_t fromFrame, size_t toFrame) {
  double sum = 0;
  size_t n = 0;
  for (size_t i = fromFrame; i < toFrame && i * 2 + 1 < pcm.size(); ++i, ++n)
    sum += double(pcm[i * 2]) * pcm[i * 2] + double(pcm[i * 2 + 1]) * pcm[i * 2 + 1];
  return n ? std::sqrt(sum / double(2 * n)) : 0;
}

struct Run {
  std::vector<float> out;
  size_t mixStart = 0, mixEnd = 0;
  int64_t overlap = 0;
  size_t latencyAfter = 0;
  bool finite = true, bounded = true, stalledWhileArmed = false;
};

// Simula a saída em tempo real: cada passo toca 512 quadros. O motor entrega
// tudo que o mixer pede (mais rápido que o tempo real), exceto que B só fica
// disponível depois de `bDelayBlocks` passos contados a partir do fim de A.
Run simulate(TransitionEffect effect, const std::vector<float> &a, const std::vector<float> &b,
             double armBeforeEndSeconds, double overlapSeconds, int bDelayBlocks = 0) {
  Run run;
  TransitionMixer mixer(rate);
  const size_t aFrames = a.size() / 2, bFrames = b.size() / 2;
  const size_t prefix = aFrames - size_t(armBeforeEndSeconds * rate);
  run.out.assign(a.begin(), a.begin() + ptrdiff_t(prefix * 2));
  require(mixer.arm(effect, int64_t(aFrames - prefix), int64_t(overlapSeconds * rate)), "arm");
  run.mixStart = prefix + size_t(mixer.mixStartFrame());
  run.overlap = mixer.overlapFrames();
  size_t aRead = prefix, bRead = 0;
  int blocksSinceAEnd = -1;
  std::vector<float> block(512 * 2);
  for (int guard = 0; guard < 200000; ++guard) {
    while (mixer.wantsInput()) {
      if (aRead < aFrames) {
        const size_t n = std::min<size_t>(1024, aFrames - aRead);
        aRead += mixer.push(a.data() + aRead * 2, n);
        continue;
      }
      if (blocksSinceAEnd < bDelayBlocks || bRead >= bFrames)
        break;
      const size_t n = std::min<size_t>(1024, bFrames - bRead);
      const size_t used = mixer.push(b.data() + bRead * 2, n);
      bRead += used;
      if (used < n)
        break;
    }
    if (aRead >= aFrames)
      ++blocksSinceAEnd;
    const auto before = mixer.state();
    const size_t got = mixer.pull(block.data(), 512);
    if (before == TransitionMixer::State::Mixing && mixer.state() == TransitionMixer::State::Idle && run.mixEnd == 0) {
      run.mixEnd = run.out.size() / 2 + got;
      run.latencyAfter = mixer.latencyFrames();
    }
    if (got < 512 && before != TransitionMixer::State::Idle && bRead < bFrames)
      run.stalledWhileArmed = before == TransitionMixer::State::Mixing || run.stalledWhileArmed;
    run.out.insert(run.out.end(), block.begin(), block.begin() + ptrdiff_t(got * 2));
    if (got == 0 && bRead >= bFrames && aRead >= aFrames)
      break;
  }
  for (float v : run.out) {
    run.finite = run.finite && std::isfinite(v);
    run.bounded = run.bounded && std::fabs(v) <= 1.0f;
  }
  return run;
}
} // namespace

int main() {
  std::set<std::string_view> ids;
  for (int i = 0; i < kTransitionEffectCount; ++i) {
    const auto id = transitionEffectId(TransitionEffect(i));
    ids.insert(id);
    require(transitionEffectFromId(id) == TransitionEffect(i), "id roundtrip");
  }
  require(ids.size() == 6, "six distinct effects");
  require(transitionEffectFromId("nope") == TransitionEffect::Count, "unknown id");
  require(clampTransitionSeconds(1) == 3 && clampTransitionSeconds(42) == 10 && clampTransitionSeconds(7) == 7,
          "seconds clamp 3..10");
  std::set<int> picked;
  TransitionEffect previous = TransitionEffect::Count;
  for (uint32_t r = 0; r < 600; ++r) {
    const auto next = pickRandomTransition(previous, r * 2654435761u);
    require(next != previous, "random repeats previous effect");
    picked.insert(int(next));
    previous = next;
  }
  require(picked.size() == 6, "random reaches every effect");

  // Quando armar: mescla + pré-rolagem de B + folga.
  require(!planTransition(6, 200000, 30000, 30000).arm, "too early");
  auto plan = planTransition(6, 200000, 8900, 6400);
  require(plan.arm && plan.upcomingMs == 6400 && plan.overlapMs == 6000, "arm with room for B preroll");
  require(!planTransition(6, 200000, 900, 900).arm, "too late");
  plan = planTransition(10, 9000, 3000, 3000);
  require(plan.arm && plan.overlapMs == 3000, "short track limits overlap to a third");
  require(!planTransition(6, 3000, 1000, 1000).arm, "very short track is skipped");

  const auto a = tone(440, 14, 0.7f), b = tone(660, 14, 0.7f);
  const auto silence = std::vector<float>(b.size(), 0.f);
  for (int i = 0; i < kTransitionEffectCount; ++i) {
    const auto effect = TransitionEffect(i);
    const auto run = simulate(effect, a, b, 9.0, 6.0);
    require(run.finite, "finite output");
    require(run.bounded, "output stays within full scale");
    require(run.overlap == 6 * rate, "mix has exactly the requested length");
    require(!run.stalledWhileArmed, "output never stalls during the transition");
    const size_t expected = a.size() / 2 + b.size() / 2 - size_t(run.overlap);
    require(run.out.size() / 2 == expected, "the mix shortens the program by exactly the overlap");
    require(run.latencyAfter > size_t(rate * 2) && run.latencyAfter <= size_t(rate * 3),
            "B keeps a bounded preroll after the mix");
    bool tailExact = true;
    for (size_t k = run.out.size() / 2 - 2000; k < run.out.size() / 2; ++k) {
      const size_t bIndex = k - (a.size() / 2 - size_t(run.overlap));
      tailExact = tailExact && run.out[k * 2] == b[bIndex * 2];
    }
    require(tailExact, "after the mix B passes through unchanged");
    // Antes da mescla A toca intacta do buffer.
    bool before = true;
    for (size_t k = run.mixStart - 3000; k < run.mixStart; ++k)
      before = before && run.out[k * 2] == a[k * 2];
    require(before, "A is untouched until the mix starts");
    const auto aOnly = simulate(effect, a, silence, 9.0, 6.0);
    const size_t end = aOnly.mixStart + size_t(aOnly.overlap);
    require(rms(aOnly.out, end - size_t(aOnly.overlap) / 25, end) < 0.03, "A is gone at the end of the mix");
    require(rms(aOnly.out, aOnly.mixStart, aOnly.mixStart + 2000) > 0.2, "A still sounds when the mix starts");
    const auto bOnly = simulate(effect, std::vector<float>(a.size(), 0.f), b, 9.0, 6.0);
    const size_t bEnd = bOnly.mixStart + size_t(bOnly.overlap);
    require(rms(bOnly.out, bOnly.mixStart, bOnly.mixStart + size_t(bOnly.overlap) / 10) <
                rms(bOnly.out, bEnd - size_t(bOnly.overlap) / 10, bEnd),
            "B rises during the mix");
    // B entra do começo: o primeiro quadro não nulo de B é o primeiro de B.
    double jump = 0;
    for (size_t k = bEnd - 4; k < bEnd + 4; ++k)
      jump = std::max(jump, double(std::fabs(bOnly.out[(k + 1) * 2] - bOnly.out[k * 2])));
    require(jump < 0.12, "no click when the mix hands over to B");
    // B atrasada 1,5 s: a mescla não trava nem cria pausa.
    const auto late = simulate(effect, a, b, 9.0, 6.0, 65);
    require(!late.stalledWhileArmed, "late B never stalls the output");
    std::printf("effect %s overlap %.2fs latency %.2fs ok\n", transitionEffectId(effect).data(),
                double(run.overlap) / rate, double(run.latencyAfter) / rate);
  }

  // Limitador: duas faixas correlacionadas quase em fundo de escala.
  {
    const auto loudA = tone(440, 14, 0.95f), loudB = tone(440, 14, 0.95f);
    const auto run = simulate(TransitionEffect::Fade, loudA, loudB, 9.0, 6.0);
    float peak = 0, step = 0;
    for (size_t k = run.mixStart; k < run.mixStart + size_t(run.overlap); ++k) {
      peak = std::max(peak, std::fabs(run.out[k * 2]));
      step = std::max(step, std::fabs(run.out[(k + 1) * 2] - run.out[k * 2]));
    }
    require(peak <= 0.9991f, "limiter keeps the sum below 0 dBFS");
    require(step < 0.08f, "limiter changes gain smoothly");
    require(rms(run.out, run.mixStart - 4410, run.mixStart) > 0.66, "limiter does not touch A before the mix");
  }

  // Duas transições seguidas: o atraso da primeira entra no buffer da segunda.
  {
    TransitionMixer mixer(rate);
    const auto c = tone(550, 14, 0.7f);
    std::vector<float> program;
    std::vector<const std::vector<float> *> tracks{&a, &b, &c};
    size_t track = 0, read = 0;
    const size_t prefix = a.size() / 2 - size_t(9.0 * rate);
    program.assign(a.begin(), a.begin() + ptrdiff_t(prefix * 2));
    read = prefix;
    require(mixer.arm(TransitionEffect::Echo, int64_t(a.size() / 2 - prefix), 6 * rate), "first arm");
    std::vector<float> block(512 * 2);
    bool secondArmed = false;
    size_t delivered = prefix;  // quadros entregues de A+B+C
    for (int guard = 0; guard < 300000; ++guard) {
      while (mixer.wantsInput() && track < tracks.size()) {
        const auto &t = *tracks[track];
        const size_t n = std::min<size_t>(1024, t.size() / 2 - read);
        const size_t used = mixer.push(t.data() + read * 2, n);
        read += used;
        delivered += used;
        if (read >= t.size() / 2) {
          ++track;
          read = 0;
        }
        if (used < n)
          break;
      }
      // B: arma quando faltar 9 s de B no som.
      if (!secondArmed && track == 1 && mixer.state() == TransitionMixer::State::Idle) {
        const size_t played = program.size() / 2;
        const size_t bAudibleStart = a.size() / 2 - 6 * rate;
        if (played > bAudibleStart && b.size() / 2 - (played - bAudibleStart) <= size_t(9.0 * rate)) {
          const int64_t upcoming = int64_t(b.size() / 2) - int64_t(read);
          require(mixer.arm(TransitionEffect::Fade, upcoming, 6 * rate), "second arm with latency");
          secondArmed = true;
        }
      }
      const size_t got = mixer.pull(block.data(), 512);
      program.insert(program.end(), block.begin(), block.begin() + ptrdiff_t(got * 2));
      if (got == 0 && track >= tracks.size())
        break;
    }
    require(secondArmed, "second transition armed");
    require(program.size() / 2 == (a.size() + b.size() + c.size()) / 2 - size_t(12 * rate),
            "two transitions shorten the program by both overlaps");
    bool middle = true;
    const size_t bStart = a.size() / 2 - 6 * rate;
    for (size_t k = bStart + 6 * rate + 100; k < bStart + 6 * rate + 3000; ++k)
      middle = middle && program[k * 2] == b[(k - bStart) * 2];
    require(middle, "B plays intact between the transitions");
  }

  // Fim da fila: o motor para antes do previsto e A termina com o efeito.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Echo, 6 * rate, 3 * rate), "arm without B");
    mixer.push(a.data(), size_t(4 * rate));
    mixer.endCapture();
    std::vector<float> out(512 * 2);
    size_t total = 0;
    for (int i = 0; i < 2000 && mixer.active(); ++i)
      total += mixer.pull(out.data(), 512);
    require(!mixer.active() && total == size_t(4 * rate), "endCapture finishes A once");
  }
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 2 * rate, 2 * rate), "arm before reset");
    mixer.push(a.data(), 1000);
    require(!mixer.arm(TransitionEffect::Fade, 2 * rate, rate), "cannot arm twice");
    mixer.reset();
    require(!mixer.active() && mixer.latencyFrames() == 0, "reset clears everything");
    require(!mixer.arm(TransitionEffect::Count, 2 * rate, rate), "invalid effect is refused");
    require(!mixer.arm(TransitionEffect::Fade, 100, 100), "too short tail is refused");
    require(mixer.arm(TransitionEffect::BeatRoll, 2 * rate, rate), "re-arm after reset");
  }
  if (failures) {
    std::printf("%d transition check(s) failed\n", failures);
    return 1;
  }
  std::printf("PASS transition mixer: 6 effects, preroll, late B, two transitions, limiter, endCapture, reset\n");
  return 0;
}
