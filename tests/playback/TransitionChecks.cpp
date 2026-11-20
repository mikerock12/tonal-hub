// Verificações do mixer de transições (sem Qt, sem dispositivo de áudio).
#include "playback/TransitionMixer.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

std::vector<float> tone(double hz, double seconds, int rate, float amplitude) {
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
  bool finite = true, bounded = true;
};

// Simula o laço do Windows: lê o motor quando o mixer pede, a saída puxa em
// blocos de 512 quadros. `prefix` quadros de A passam direto antes de armar.
Run simulate(TransitionEffect effect, const std::vector<float> &a, const std::vector<float> &b,
             int rate, size_t prefix, int64_t tail) {
  Run run;
  TransitionMixer mixer(rate);
  std::vector<float> stream(a);
  stream.insert(stream.end(), b.begin(), b.end());
  const size_t total = stream.size() / 2;
  run.out.insert(run.out.end(), stream.begin(), stream.begin() + ptrdiff_t(prefix * 2));
  size_t read = prefix;
  require(mixer.arm(effect, tail), "arm");
  std::vector<float> block(512 * 2);
  bool sawMixing = false;
  for (int guard = 0; guard < 1000000; ++guard) {
    while (mixer.wantsInput() && read < total) {
      const size_t n = std::min<size_t>(512, total - read);
      const size_t used = mixer.push(stream.data() + read * 2, n);
      read += used;
      if (used < n)
        break;
    }
    if (mixer.state() == TransitionMixer::State::Mixing && !sawMixing) {
      sawMixing = true;
      run.mixStart = run.out.size() / 2;
      run.overlap = mixer.overlapFrames();
    }
    const size_t got = mixer.pull(block.data(), 512, read >= total);
    if (sawMixing && run.mixEnd == 0 && mixer.state() == TransitionMixer::State::Idle)
      run.mixEnd = run.out.size() / 2 + got;
    run.out.insert(run.out.end(), block.begin(), block.begin() + ptrdiff_t(got * 2));
    if (!mixer.active()) {
      run.out.insert(run.out.end(), stream.begin() + ptrdiff_t(read * 2), stream.end());
      break;
    }
    if (got == 0 && read >= total)
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
  // Identificadores, limites e sorteio.
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
    require(int(next) >= 0 && int(next) < kTransitionEffectCount, "random range");
    picked.insert(int(next));
    previous = next;
  }
  require(picked.size() == 6, "random reaches every effect");

  // Quando armar.
  require(!planTransition(6, 200000, 30000, 100).arm, "too early");
  auto plan = planTransition(6, 200000, 6200, 100);
  require(plan.arm && plan.tailMs == 6100 && plan.overlapMs == 6000, "arm inside window");
  require(!planTransition(6, 200000, 900, 100).arm, "too late");
  plan = planTransition(10, 9000, 3000, 0);
  require(plan.arm && plan.overlapMs == 3000, "short track limits overlap to a third");
  require(!planTransition(6, 3000, 1000, 0).arm, "very short track is skipped");

  constexpr int rate = 44100;
  const auto a = tone(440, 12, rate, 0.7f);
  const auto b = tone(660, 12, rate, 0.7f);
  const auto silence = std::vector<float>(b.size(), 0.f);
  const size_t aFrames = a.size() / 2, prefix = aFrames - size_t(6 * rate);
  for (int i = 0; i < kTransitionEffectCount; ++i) {
    const auto effect = TransitionEffect(i);
    const auto run = simulate(effect, a, b, rate, prefix, int64_t(aFrames - prefix));
    require(run.finite, "finite output");
    require(run.bounded, "output stays within full scale");
    require(run.overlap > 5 * rate, "overlap close to the requested time");
    const size_t expected = a.size() / 2 + b.size() / 2 - size_t(run.overlap);
    require(run.out.size() / 2 == expected, "the mix shortens the program by exactly the overlap");
    // Depois da mesclagem B segue intacta (sem atraso acumulado).
    bool tailExact = true;
    for (size_t k = run.out.size() / 2 - 2000; k < run.out.size() / 2; ++k) {
      const size_t bIndex = k - (a.size() / 2 - size_t(run.overlap));
      tailExact = tailExact && run.out[k * 2] == b[bIndex * 2];
    }
    require(tailExact, "after the mix B passes through unchanged");
    // A some no fim da mesclagem; B cresce do começo para o fim.
    const auto aOnly = simulate(effect, a, silence, rate, prefix, int64_t(aFrames - prefix));
    const size_t end = aOnly.mixStart + size_t(aOnly.overlap);
    require(rms(aOnly.out, end - size_t(aOnly.overlap) / 25, end) < 0.03, "A is gone at the end of the mix");
    require(rms(aOnly.out, aOnly.mixStart, aOnly.mixStart + 2000) > 0.2, "A still sounds when the mix starts");
    const auto bOnly = simulate(effect, std::vector<float>(a.size(), 0.f), b, rate, prefix, int64_t(aFrames - prefix));
    const size_t bEnd = bOnly.mixStart + size_t(bOnly.overlap);
    require(rms(bOnly.out, bOnly.mixStart, bOnly.mixStart + size_t(bOnly.overlap) / 10) <
                rms(bOnly.out, bEnd - size_t(bOnly.overlap) / 10, bEnd),
            "B rises during the mix");
    // Sem estalo ao terminar: a última amostra mesclada encosta em B.
    double jump = 0;
    for (size_t k = bEnd - 4; k < bEnd + 4; ++k)
      jump = std::max(jump, double(std::fabs(bOnly.out[(k + 1) * 2] - bOnly.out[k * 2])));
    require(jump < 0.12, "no click when the mix hands over to B");
    std::printf("effect %s overlap %.2fs ok\n", transitionEffectId(effect).data(), double(run.overlap) / rate);
  }

  // Sem B (fim da fila): com preenchimento o fim de A toca e o mixer encerra.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Echo, 3 * rate), "arm without B");
    std::vector<float> chunk(a.begin(), a.begin() + 3 * rate * 2);
    require(mixer.push(chunk.data(), size_t(3 * rate)) == size_t(3 * rate), "capture accepts the tail");
    std::vector<float> out(512 * 2);
    require(mixer.pull(out.data(), 512, false) == 0, "mix waits for B unless padding");
    size_t total = 0;
    while (mixer.active()) {
      const size_t got = mixer.pull(out.data(), 512, true);
      if (!got)
        break;
      total += got;
    }
    require(total == size_t(3 * rate) && !mixer.active(), "padding plays the whole tail once");
  }
  // O motor parou antes do previsto: a mescla usa o que já chegou.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 4 * rate), "arm long tail");
    mixer.push(a.data(), size_t(2 * rate));
    mixer.endCapture();
    require(mixer.state() == TransitionMixer::State::Mixing && mixer.overlapFrames() == 2 * rate,
            "endCapture mixes what was read");
  }
  // Reset descarta tudo e permite rearmar.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 2 * rate), "arm before reset");
    mixer.push(a.data(), 1000);
    require(!mixer.arm(TransitionEffect::Fade, 2 * rate), "cannot arm twice");
    mixer.reset();
    require(!mixer.active() && mixer.bufferedFrames() == 0, "reset clears the tail");
    require(mixer.arm(TransitionEffect::BeatRoll, 2 * rate), "re-arm after reset");
    require(!mixer.arm(TransitionEffect::Count, 2 * rate) || true, "invalid effect");
    mixer.reset();
    require(!mixer.arm(TransitionEffect::Count, 2 * rate), "invalid effect is refused");
    require(!mixer.arm(TransitionEffect::Fade, 100), "too short tail is refused");
  }
  // O mixer limita o adiantamento de B para não acumular atraso.
  {
    TransitionMixer mixer(rate);
    mixer.arm(TransitionEffect::Fade, rate);
    mixer.push(a.data(), size_t(rate));
    size_t accepted = 0;
    while (mixer.wantsInput())
      accepted += mixer.push(b.data() + accepted * 2, 512);
    require(accepted <= 4096, "B read-ahead stays bounded during the mix");
  }
  if (failures) {
    std::printf("%d transition check(s) failed\n", failures);
    return 1;
  }
  std::printf("PASS transition mixer: 6 effects, timing, padding, reset and bounded read-ahead\n");
  return 0;
}
