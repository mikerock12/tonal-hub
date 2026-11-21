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
  int64_t overlap = 0, outro = 0, intro = 0;
  size_t latencyAfter = 0;
  bool finite = true, bounded = true, stalledWhileArmed = false;
};

struct Options {
  double outro = 6, intro = 6;
  int bDelayBlocks = 0;      // B só chega depois de N passos após o fim de A
  bool exact = true;         // o motor marca o começo de B (boundary)
  double estimateError = 0;  // erro da estimativa de A restante, em s
  bool sendBoundary = true;
};

// Simula a saída em tempo real: cada passo toca 512 quadros. O motor entrega
// tudo que o mixer pede (mais rápido que o tempo real), marca o começo de B
// e só entrega B depois de `bDelayBlocks` passos contados do fim de A.
Run simulate(TransitionEffect effect, const std::vector<float> &a, const std::vector<float> &b,
             double armBeforeEndSeconds, Options o = {}) {
  Run run;
  TransitionMixer mixer(rate);
  const size_t aFrames = a.size() / 2, bFrames = b.size() / 2;
  const size_t prefix = aFrames - size_t(armBeforeEndSeconds * rate);
  run.out.assign(a.begin(), a.begin() + ptrdiff_t(prefix * 2));
  const int64_t estimate = int64_t(aFrames - prefix) + int64_t(o.estimateError * rate);
  require(mixer.arm(effect, estimate, int64_t(o.outro * rate), int64_t(o.intro * rate), o.exact), "arm");
  size_t aRead = prefix, bRead = 0;
  bool marked = false;
  int blocksSinceAEnd = -1;
  std::vector<float> block(512 * 2);
  for (int guard = 0; guard < 400000; ++guard) {
    while (mixer.wantsInput()) {
      if (aRead < aFrames) {
        const size_t n = std::min<size_t>(1024, aFrames - aRead);
        const size_t used = mixer.push(a.data() + aRead * 2, n);
        aRead += used;
        if (used < n)
          break;
        continue;
      }
      if (!marked && o.sendBoundary) {
        mixer.boundary();
        marked = true;
      }
      if (blocksSinceAEnd < o.bDelayBlocks || bRead >= bFrames)
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
    if (before == TransitionMixer::State::Mixing && run.overlap == 0) {
      run.overlap = mixer.overlapFrames();
      run.outro = mixer.outroFrames();
      run.intro = mixer.introFrames();
      run.mixStart = prefix + size_t(mixer.mixStartFrame());
    }
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

// B entra no ponto certo: a saída termina com B intacta, alinhada `intro`
// quadros antes do fim de A.
bool endsWithB(const Run &run, const std::vector<float> &a, const std::vector<float> &b, size_t intro) {
  const size_t bOffset = a.size() / 2 - intro;
  if (run.out.size() / 2 != bOffset + b.size() / 2)
    return false;
  for (size_t k = run.out.size() / 2 - 2000; k < run.out.size() / 2; ++k)
    if (run.out[k * 2] != b[(k - bOffset) * 2])
      return false;
  return true;
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

  // Quando armar: região (maior das duas durações) + pré-rolagem de B + folga.
  require(!planTransition(6, 6, 200000, 30000, 30000).arm, "too early");
  auto plan = planTransition(6, 6, 200000, 8900, 6400);
  require(plan.arm && plan.upcomingMs == 6400 && plan.outroMs == 6000 && plan.introMs == 6000,
          "arm with room for B preroll");
  plan = planTransition(4, 9, 200000, 11900, 9000);
  require(plan.arm && plan.outroMs == 4000 && plan.introMs == 9000 && plan.regionMs() == 9000,
          "a longer intro arms earlier");
  require(!planTransition(4, 9, 200000, 12100, 9000).arm, "intro sets the arming window");
  require(!planTransition(6, 6, 200000, 900, 900).arm, "too late");
  plan = planTransition(10, 10, 9000, 3000, 3000);
  require(plan.arm && plan.outroMs == 3000 && plan.introMs == 3000, "short track limits the mix to a third");
  require(!planTransition(6, 6, 3000, 1000, 1000).arm, "very short track is skipped");

  const auto a = tone(440, 16, 0.7f), b = tone(660, 16, 0.7f);
  const auto silence = std::vector<float>(b.size(), 0.f);
  const auto mute = std::vector<float>(a.size(), 0.f);
  const size_t aEnd = a.size() / 2;
  struct Shape {
    double outro, intro;
  };
  for (int i = 0; i < kTransitionEffectCount; ++i) {
    const auto effect = TransitionEffect(i);
    for (const Shape shape : {Shape{6, 6}, Shape{4, 9}, Shape{9, 3}}) {
      Options o;
      o.outro = shape.outro;
      o.intro = shape.intro;
      const size_t outro = size_t(shape.outro * rate), intro = size_t(shape.intro * rate);
      const size_t region = std::max(outro, intro);
      const auto run = simulate(effect, a, b, 12.0, o);
      require(run.finite, "finite output");
      require(run.bounded, "output stays within full scale");
      require(run.overlap == int64_t(region) && run.outro == int64_t(outro) && run.intro == int64_t(intro),
              "mix region is the longer of the two lengths");
      require(run.mixStart == aEnd - region, "the mix region ends exactly at the end of A");
      require(!run.stalledWhileArmed, "output never stalls during the transition");
      require(endsWithB(run, a, b, intro), "B starts `intro` before the end of A and then plays intact");
      require(run.latencyAfter > size_t(rate * 2) && run.latencyAfter <= size_t(rate * 3),
              "B keeps a bounded preroll after the mix");
      bool before = true;
      for (size_t k = run.mixStart - 3000; k < run.mixStart; ++k)
        before = before && run.out[k * 2] == a[k * 2];
      require(before, "A is untouched until the mix starts");
      // Só A: o efeito acontece nos últimos `outro` quadros e A some no fim.
      const auto aOnly = simulate(effect, a, silence, 12.0, o);
      require(rms(aOnly.out, aEnd - outro / 25, aEnd) < 0.03, "A is gone at the end of the mix");
      if (effect != TransitionEffect::FilterSweep)
        require(rms(aOnly.out, aEnd - outro - 2000, aEnd - outro) > 0.45, "A is dry before its effect starts");
      require(rms(aOnly.out, aEnd - outro, aEnd - outro + 2000) > 0.2, "A still sounds when its effect starts");
      // Só B: nada de B antes de `intro`; B sobe e soa de verdade na região.
      const auto bOnly = simulate(effect, mute, b, 12.0, o);
      bool silentBefore = true;
      for (size_t k = aEnd - region; k < aEnd - intro; ++k)
        silentBefore = silentBefore && bOnly.out[k * 2] == 0.f;
      require(silentBefore, "B waits until `intro` before the end of A");
      require(rms(bOnly.out, aEnd - intro, aEnd - intro + intro / 10) < rms(bOnly.out, aEnd - intro / 10, aEnd),
              "B rises during the mix");
      require(rms(bOnly.out, aEnd - intro / 4, aEnd) > 0.25, "B is clearly heard before A ends");
      double jump = 0;
      for (size_t k = aEnd - 4; k < aEnd + 4; ++k)
        jump = std::max(jump, double(std::fabs(bOnly.out[(k + 1) * 2] - bOnly.out[k * 2])));
      require(jump < 0.12, "no click when the mix hands over to B");
    }
    // O defeito da 0.17.1: a estimativa do restante de A errava em segundos e
    // o fim de A virava "B". Com o marcador, B entra no ponto certo assim mesmo.
    for (const double error : {-4.0, -2.0, 3.0, 5.0}) {
      Options o;
      o.estimateError = error;
      const auto run = simulate(effect, a, b, 12.0, o);
      require(run.mixStart == aEnd - size_t(6 * rate), "marker fixes the mix region despite a wrong estimate");
      require(endsWithB(run, a, b, size_t(6 * rate)), "marker keeps B aligned despite a wrong estimate");
      const auto bOnly = simulate(effect, mute, b, 12.0, o);
      require(rms(bOnly.out, aEnd - size_t(1.5 * rate), aEnd) > 0.25, "real B is in the mix despite a wrong estimate");
    }
    // B atrasada 1,5 s: a mescla não trava nem cria pausa.
    Options late;
    late.bDelayBlocks = 65;
    require(!simulate(effect, a, b, 12.0, late).stalledWhileArmed, "late B never stalls the output");
    std::printf("effect %s ok\n", transitionEffectId(effect).data());
  }

  // Sem o marcador (motor antigo): continua usando a estimativa.
  {
    Options o;
    o.exact = false;
    o.sendBoundary = false;
    const auto run = simulate(TransitionEffect::Fade, a, b, 12.0, o);
    require(endsWithB(run, a, b, size_t(6 * rate)), "estimate mode still works when it is right");
    Options marked = o;
    marked.sendBoundary = true;
    marked.estimateError = 2.0;
    const auto fixed = simulate(TransitionEffect::Fade, a, b, 12.0, marked);
    require(endsWithB(fixed, a, b, size_t(6 * rate)), "a marker that arrives during capture wins over the estimate");
    // Reprodução do defeito relatado: sem marcador e com a estimativa 4 s curta,
    // a mescla acontece 4 s cedo, sem a B de verdade: A desce e o próprio fim
    // de A sobe no lugar de B (a "oscilação"); B começa depois, sem efeito.
    Options broken = o;
    broken.estimateError = -4.0;
    const auto wrong = simulate(TransitionEffect::Fade, a, b, 12.0, broken);
    require(wrong.mixStart == aEnd - size_t(10 * rate), "0.17.1 defect reproduced: mix placed 4 s early");
    const auto wrongB = simulate(TransitionEffect::Fade, mute, b, 12.0, broken);
    require(rms(wrongB.out, aEnd - size_t(10 * rate), aEnd - size_t(6.5 * rate)) < 0.01,
            "0.17.1 defect reproduced: real B is absent from most of the mix");
    // O mesmo caso com o marcador: A termina dentro da mescla.
    Options exact;
    exact.estimateError = -4.0;
    const auto rightA = simulate(TransitionEffect::Fade, a, silence, 12.0, exact);
    require(rms(rightA.out, aEnd - size_t(0.2 * rate), aEnd) < 0.03, "with the marker A ends inside the mix");
  }

  // Buffer cheio sem marcador: A e B tocam inteiras, sem mesclar A com ela mesma.
  {
    const auto longA = tone(440, 45, 0.7f);
    Options o;
    o.sendBoundary = false;
    const auto run = simulate(TransitionEffect::Echo, longA, b, 40.0, o);
    bool plain = run.out.size() == longA.size() + b.size();
    for (size_t k = 0; plain && k < longA.size(); k += 7)
      plain = run.out[k] == longA[k];
    for (size_t k = 0; plain && k < b.size(); k += 7)
      plain = run.out[longA.size() + k] == b[k];
    require(plain, "no marker before the buffer fills: plain playback, no fake mix");
  }

  // Limitador: duas faixas correlacionadas quase em fundo de escala.
  {
    const auto loudA = tone(440, 16, 0.95f), loudB = tone(440, 16, 0.95f);
    const auto run = simulate(TransitionEffect::Fade, loudA, loudB, 12.0);
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
    const auto c = tone(550, 16, 0.7f);
    std::vector<float> program;
    std::vector<const std::vector<float> *> tracks{&a, &b, &c};
    size_t track = 0, read = 0;
    const size_t prefix = aEnd - size_t(12.0 * rate);
    program.assign(a.begin(), a.begin() + ptrdiff_t(prefix * 2));
    read = prefix;
    require(mixer.arm(TransitionEffect::Echo, int64_t(aEnd - prefix) - rate, 6 * rate, 5 * rate, true), "first arm");
    std::vector<float> block(512 * 2);
    bool secondArmed = false;
    for (int guard = 0; guard < 400000; ++guard) {
      while (mixer.wantsInput() && track < tracks.size()) {
        const auto &t = *tracks[track];
        const size_t n = std::min<size_t>(1024, t.size() / 2 - read);
        const size_t used = mixer.push(t.data() + read * 2, n);
        read += used;
        if (read >= t.size() / 2) {
          ++track;
          read = 0;
          mixer.boundary();
        }
        if (used < n)
          break;
      }
      // B: arma quando faltar 12 s de B no som (estimativa 1 s maior que o real).
      if (!secondArmed && track == 1 && mixer.state() == TransitionMixer::State::Idle) {
        const size_t played = program.size() / 2;
        const size_t bAudibleStart = aEnd - 5 * rate;
        if (played > bAudibleStart && b.size() / 2 - (played - bAudibleStart) <= size_t(12.0 * rate)) {
          const int64_t upcoming = int64_t(b.size() / 2) - int64_t(read) + rate;
          require(mixer.arm(TransitionEffect::Fade, upcoming, 4 * rate, 7 * rate, true), "second arm with latency");
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
            "two transitions shorten the program by both intros");
    bool middle = true;
    const size_t bStart = aEnd - 5 * rate;
    for (size_t k = aEnd + 100; k < aEnd + 3000; ++k)
      middle = middle && program[k * 2] == b[(k - bStart) * 2];
    require(middle, "B plays intact between the transitions");
    bool last = true;
    const size_t cStart = bStart + b.size() / 2 - 7 * rate;
    for (size_t k = program.size() / 2 - 2000; k < program.size() / 2; ++k)
      last = last && program[k * 2] == c[(k - cStart) * 2];
    require(last, "C enters at its own intro and plays intact");
  }

  // Fim da fila: o motor para antes do previsto e A termina com o efeito.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Echo, 6 * rate, 3 * rate, 3 * rate, true), "arm without B");
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
    require(mixer.arm(TransitionEffect::Fade, 2 * rate, 2 * rate, 2 * rate), "arm before reset");
    mixer.push(a.data(), 1000);
    require(!mixer.arm(TransitionEffect::Fade, 2 * rate, rate, rate), "cannot arm twice");
    mixer.reset();
    require(!mixer.active() && mixer.latencyFrames() == 0, "reset clears everything");
    require(!mixer.arm(TransitionEffect::Count, 2 * rate, rate, rate), "invalid effect is refused");
    require(!mixer.arm(TransitionEffect::Fade, 100, 100, 100), "too short tail is refused");
    require(mixer.arm(TransitionEffect::BeatRoll, 2 * rate, rate, rate, true), "re-arm after reset");
  }
  if (failures) {
    std::printf("%d transition check(s) failed\n", failures);
    return 1;
  }
  std::printf("PASS transition mixer: 6 effects x 3 shapes, exact boundary vs wrong estimate, preroll, late B, full buffer, two transitions, limiter, endCapture, reset\n");
  return 0;
}
