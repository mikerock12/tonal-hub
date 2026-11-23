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

// Amplitude de uma frequência no canal esquerdo (Goertzel), para separar A e B.
double tonePresence(const std::vector<float> &pcm, size_t fromFrame, size_t toFrame, double hz) {
  const double w = 2 * std::numbers::pi * hz / rate, c = 2 * std::cos(w);
  double s1 = 0, s2 = 0;
  size_t n = 0;
  for (size_t i = fromFrame; i < toFrame && i * 2 < pcm.size(); ++i, ++n) {
    const double s0 = pcm[i * 2] + c * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  if (!n)
    return 0;
  const double power = s1 * s1 + s2 * s2 - c * s1 * s2;
  return 2 * std::sqrt(std::max(0.0, power)) / double(n);
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
  TransitionTrace trace;
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
  require(mixer.arm(effect, estimate, int64_t(o.outro * rate), int64_t(o.intro * rate), o.exact, 7), "arm");
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
  run.trace = mixer.trace();
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
  require(planTransition(6, 6, 200000, 14900, 12400).arm,
          "slow next-track loading needs time before the mix");
  auto plan = planTransition(6, 6, 200000, 8900, 6400);
  require(plan.arm && plan.upcomingMs == 6400 && plan.outroMs == 6000 && plan.introMs == 6000,
          "arm with room for B preroll");
  plan = planTransition(4, 9, 200000, 11900, 9000);
  require(plan.arm && plan.outroMs == 4000 && plan.introMs == 9000 && plan.regionMs() == 9000,
          "a longer intro arms earlier");
  require(!planTransition(4, 9, 200000, 18100, 9000).arm, "intro sets the arming window");
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
      require(run.trace.id == 7 && run.trace.phase == TransitionPhase::Completed && run.trace.overlapVerified() &&
                  run.trace.missingBFrames == 0 && run.trace.bFrames == int64_t(intro) &&
                  run.trace.preparedB >= std::min<int64_t>(intro, rate) && run.trace.overlap == int64_t(region),
              "trace proves A and B in the same mix");
      // 440 Hz = A, 660 Hz = B: as duas soam juntas na metade final da entrada de B.
      require(tonePresence(run.out, aEnd - intro / 2, aEnd - intro / 4, 660) > 0.1,
              "B (660 Hz) is clearly present inside the mix");
      // (No freio A muda de altura: a presença dela é conferida pelo RMS abaixo.)
      require(effect == TransitionEffect::VinylBrake ||
                  tonePresence(run.out, run.mixStart, run.mixStart + rate / 4, 440) > 0.3,
              "A (440 Hz) is present when the mix starts");
      require(tonePresence(run.out, run.mixStart - rate / 2, run.mixStart, 660) < 0.005,
              "no B before the mix");
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
    // B atrasada em relação ao começo planejado da mescla (6 s antes do fim de
    // A): nunca A com efeito sobre silêncio. A espera sem efeito, a região
    // encolhe; sem o mínimo de 1,5 s a troca é normal.
    for (const double delay : {0.0, 0.1, 0.5, 1.0, 3.0, 4.8, 7.0}) {
      Options o;
      o.bDelayBlocks = int((12.0 - 6.0 + delay) * rate / 512);
      const auto run = simulate(effect, a, b, 12.0, o);
      const auto aOnly = simulate(effect, a, silence, 12.0, o);
      require(!run.stalledWhileArmed && run.finite, "late B: output continues");
      require(run.trace.missingBFrames == 0, "late B: never mixes A with silence in place of B");
      if (run.trace.phase == TransitionPhase::Completed) {
        require(delay < 4.6 && run.trace.overlapVerified() && run.trace.preparedB >= rate,
                "late B: mix starts only with B prepared");
        require(endsWithB(run, a, b, size_t(run.trace.intro)), "late B: B aligned to the shortened mix");
        require(tonePresence(run.out, aEnd - size_t(run.trace.intro) / 2, aEnd, 660) > 0.1,
                "late B: B audible inside the shortened mix");
        bool dryA = true;
        const size_t start = aEnd - size_t(run.trace.overlap);
        for (size_t k = aEnd - size_t(12.0 * rate); k < start; ++k)
          dryA = dryA && aOnly.out[k * 2] == a[k * 2];
        require(dryA, "late B: A untouched until the real mix start");
      } else {
        require(delay > 4.4 && run.trace.phase == TransitionPhase::Aborted &&
                    run.trace.reason == TransitionReason::NextNotPrepared && run.trace.mixedFrames == 0,
                "late B: no B for a minimal mix aborts the effect");
        bool plain = run.out.size() == a.size() + b.size();
        for (size_t k = 0; plain && k < a.size(); k += 3)
          plain = run.out[k] == a[k];
        for (size_t k = 0; plain && k < b.size(); k += 3)
          plain = run.out[a.size() + k] == b[k];
        require(plain, "late B: plain A then B, continuous, no fake mix");
      }
    }
    std::printf("effect %s ok\n", transitionEffectId(effect).data());
  }

  // Com a mesma carga lenta de B, a janela antiga aborta; capturar A mais cedo
  // dá tempo de B chegar sem alterar a pré-rolagem nem aceitar silêncio no mix.
  {
    Options slow;
    slow.bDelayBlocks = int(8.0 * rate / 512);
    const auto oldWindow = simulate(TransitionEffect::Fade, a, b, 9.0, slow);
    require(oldWindow.trace.phase == TransitionPhase::Aborted &&
                oldWindow.trace.reason == TransitionReason::NextNotPrepared,
            "fixture reproduces the old narrow loading window");
    const auto earlyCapture = simulate(TransitionEffect::Fade, a, b, 15.0, slow);
    require(earlyCapture.trace.phase == TransitionPhase::Completed && earlyCapture.trace.overlapVerified() &&
                earlyCapture.trace.missingBFrames == 0 && earlyCapture.trace.preparedB >= rate,
            "early capture completes the same slow-load transition");
    require(earlyCapture.latencyAfter > size_t(rate * 2) && earlyCapture.latencyAfter <= size_t(rate * 3),
            "early capture does not increase the post-mix preroll");
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
    require(run.trace.phase == TransitionPhase::Aborted && run.trace.reason == TransitionReason::NoExactBoundary,
            "missing marker is reported");
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

  // Fim da fila: o motor para antes do previsto e B nunca chega. Sem B não há
  // efeito (nada de mescla falsa): A toca inteira uma vez e o motivo fica no trace.
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
    require(mixer.trace().phase == TransitionPhase::Aborted &&
                mixer.trace().reason == TransitionReason::NextNotPrepared && mixer.trace().energyA == 0,
            "without B the effect is not applied");
  }
  // Captura atrasada (Android 0.17.5–0.17.7, um pacote por bloco tocado): A já
  // tocou até o marcador quando ele chega. Sem região não há efeito, e o trace
  // tem de dizer isso em vez de ficar "armado" para sempre.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 9 * rate, 4 * rate, 4 * rate, true, 41), "arm with a slow capture");
    std::vector<float> out(512 * 2);
    size_t played = 0, pushed = 0;
    for (size_t at = 0; at < size_t(4 * rate); at += 512) {
      pushed += mixer.push(a.data() + at * 2, 512);
      played += mixer.pull(out.data(), 512);
    }
    mixer.boundary();
    for (size_t at = 0; at < size_t(2 * rate); at += 512) {
      pushed += mixer.push(b.data() + at * 2, 512);
      played += mixer.pull(out.data(), 512);
    }
    require(mixer.trace().overlap == 0 && mixer.trace().phase == TransitionPhase::Aborted &&
                mixer.trace().reason == TransitionReason::NextNotPrepared && mixer.trace().id == 41,
            "a boundary after A already played ends the trace as a plain switch with its reason");
    require(played + mixer.latencyFrames() == pushed, "the late capture still plays A then B once");
  }
  // Android: o motor pausou no meio (recuperação de buffering) e retomou a
  // mesma faixa. O trecho retomado é A, não B: a captura continua até o
  // marcador real de B, e a mescla usa a B verdadeira.
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 9 * rate, 6 * rate, 6 * rate, true), "arm before a recovery pause");
    const size_t split = size_t(4 * rate);
    mixer.push(a.data(), split);
    mixer.endCapture();
    mixer.resumeCapture();
    mixer.push(a.data() + split * 2, aEnd - split);
    mixer.boundary();
    size_t pushed = 0;
    std::vector<float> out(512 * 2), program;
    for (int i = 0; i < 200000; ++i) {
      while (mixer.wantsInput() && pushed < b.size() / 2)
        pushed += mixer.push(b.data() + pushed * 2, std::min<size_t>(1024, b.size() / 2 - pushed));
      const size_t got = mixer.pull(out.data(), 512);
      program.insert(program.end(), out.begin(), out.begin() + ptrdiff_t(got * 2));
      if (!got && pushed >= b.size() / 2)
        break;
    }
    require(mixer.trace().phase == TransitionPhase::Completed && mixer.trace().overlapVerified() &&
                mixer.trace().boundaryFrame == int64_t(aEnd),
            "resumed A is captured as A; the real B is mixed");
    require(program.size() / 2 == aEnd + b.size() / 2 - size_t(6 * rate) &&
                tonePresence(program, aEnd - size_t(3 * rate), aEnd - size_t(1.5 * rate), 660) > 0.1,
            "after a recovery pause the mix still has the real B");
  }
  {
    TransitionMixer mixer(rate);
    require(mixer.arm(TransitionEffect::Fade, 2 * rate, 2 * rate, 2 * rate), "arm before reset");
    mixer.push(a.data(), 1000);
    require(!mixer.arm(TransitionEffect::Fade, 2 * rate, rate, rate, false, 9), "cannot arm twice");
    require(mixer.lastRejection() == TransitionReason::Busy && mixer.trace().phase == TransitionPhase::Armed,
            "refusal keeps the running trace");
    mixer.reset();
    require(mixer.trace().phase == TransitionPhase::Aborted && mixer.trace().reason == TransitionReason::Cancelled,
            "reset reports the cancelled transition");
    require(!mixer.active() && mixer.latencyFrames() == 0, "reset clears everything");
    require(!mixer.arm(TransitionEffect::Count, 2 * rate, rate, rate), "invalid effect is refused");
    require(mixer.lastRejection() == TransitionReason::InvalidRequest, "invalid request reason");
    require(!mixer.arm(TransitionEffect::Fade, 100, 100, 100), "too short tail is refused");
    require(mixer.lastRejection() == TransitionReason::BufferCapacity, "capacity reason");
    require(mixer.arm(TransitionEffect::BeatRoll, 2 * rate, rate, rate, true), "re-arm after reset");
  }
  // Taxas diferentes: as durações valem em milissegundos.
  for (const int other : {48000, 96000}) {
    TransitionMixer mixer(other);
    const auto ta = std::vector<float>(size_t(10 * other) * 2, 0.4f), tb = std::vector<float>(size_t(8 * other) * 2, 0.3f);
    require(mixer.arm(TransitionEffect::Fade, 0, 4 * other, 6 * other, true, 3), "arm at other rate");
    mixer.push(ta.data(), ta.size() / 2);
    mixer.boundary();
    size_t pushed = 0, total = 0;
    std::vector<float> out(512 * 2);
    for (int i = 0; i < 100000 && (mixer.active() || pushed < tb.size() / 2); ++i) {
      while (mixer.wantsInput() && pushed < tb.size() / 2)
        pushed += mixer.push(tb.data() + pushed * 2, std::min<size_t>(1024, tb.size() / 2 - pushed));
      const size_t got = mixer.pull(out.data(), 512);
      total += got;
      if (!got && pushed >= tb.size() / 2)
        break;
    }
    require(mixer.trace().overlap == 6 * other && mixer.trace().sampleRate == other &&
                mixer.trace().phase == TransitionPhase::Completed && mixer.trace().overlapVerified(),
            "durations follow the sample rate");
    require(total == size_t(10 * other + 8 * other - 6 * other), "program length at other rate");
  }
  require(transitionPhaseId(TransitionPhase::Completed) == "completed" &&
              transitionReasonId(TransitionReason::NextNotPrepared) == "next-not-prepared",
          "stable report ids");

  if (failures) {
    std::printf("%d transition check(s) failed\n", failures);
    return 1;
  }
  std::printf("PASS transition mixer: 6 effects x 3 shapes with A 440 Hz + B 660 Hz in the same mix, B late 0-7 s after the planned start (gate/abort), exact boundary vs wrong estimate, preroll, full buffer, two transitions, limiter, endCapture, reset, trace, 48/96 kHz\n");
  return 0;
}
