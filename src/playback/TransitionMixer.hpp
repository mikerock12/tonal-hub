#pragma once
// Transições entre músicas da fila (Windows e Android).
//
// O mixer fica entre o PCM estéreo float32 do motor e o DSP/saída. Desde a
// 0.17.1 ele trabalha com antecedência:
//  1. Armado alguns segundos antes do fim da faixa A (mescla + pré-rolagem),
//     ele lê o restante de A de uma vez (o motor decodifica mais rápido que o
//     tempo real) e, logo depois, o começo da faixa B, até `prerollFrames`.
//  2. Enquanto isso a saída continua tocando A do próprio buffer, sem depender
//     do ritmo do motor.
//  3. A mescla começa num ponto fixo do fim de A, com B já guardada. Cada
//     bloco é renderizado à frente da saída e passa por um limitador com
//     antecipação, que só atua se a soma A+B passar de 0 dBFS.
//  4. Depois da mescla B segue do FIFO. A pré-rolagem vira um atraso
//     constante entre o motor e o som (latencyFrames), que a UI desconta.
// Buffers reservados no construtor; push/pull não alocam. Sem rede, banco ou UI.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tonal::playback {

enum class TransitionEffect : int {
  Fade = 0,     // Fade cruzado com potência constante
  Echo,         // Eco pingue-pongue que se despede de A
  FilterSweep,  // Troca de graves: A fecha o filtro, B abre os graves
  VinylBrake,   // Freio do toca-discos: A desacelera até parar
  ReverbWash,   // Banho de reverb: A se dissolve no ambiente
  BeatRoll,     // Loop acelerado: A repete trechos cada vez menores
  Count
};

constexpr int kTransitionEffectCount = int(TransitionEffect::Count);
constexpr int kTransitionMinSeconds = 3;
constexpr int kTransitionMaxSeconds = 10;
// Quanto do começo de B fica guardado antes da mescla começar.
constexpr int kTransitionPrerollMs = 2500;

std::string_view transitionEffectId(TransitionEffect effect);
// Retorna Count quando o identificador é desconhecido.
TransitionEffect transitionEffectFromId(std::string_view id);
int clampTransitionSeconds(int seconds);

// Escolha do modo Aleatório: sorteio uniforme entre os efeitos, sem repetir
// o anterior. `random` é qualquer inteiro de 32 bits do chamador.
TransitionEffect pickRandomTransition(TransitionEffect previous, uint32_t random);

// Quando armar. `audibleRemainingMs` = quanto falta de A até o fim no som;
// `upcomingMs` = quanto de A o motor ainda não entregou.
struct TransitionPlan {
  bool arm = false;
  int64_t upcomingMs = 0;  // restante de A ainda no motor
  int64_t overlapMs = 0;   // duração da mesclagem
};
TransitionPlan planTransition(int seconds, int64_t durationMs, int64_t audibleRemainingMs,
                              int64_t upcomingMs);

class TransitionMixer {
public:
  enum class State { Idle, Armed, Mixing };
  explicit TransitionMixer(int sampleRate = 44100);
  void setSampleRate(int sampleRate);
  int sampleRate() const { return m_rate; }

  // `upcomingFrames`: quanto de A o motor ainda vai entregar. O que já está
  // no FIFO (atraso de uma transição anterior) também é de A e entra no buffer.
  bool arm(TransitionEffect effect, int64_t upcomingFrames, int64_t overlapFrames);
  // Seek, troca de faixa pelo usuário, flush ou nova sessão: descarta tudo.
  void reset();
  // O motor terminou A antes do previsto: A acaba no que já foi lido.
  void endCapture();

  State state() const { return m_state; }
  bool capturing() const { return m_state != State::Idle && m_tailCount < m_tailTarget; }
  // Há algo armado, em mesclagem ou guardado no FIFO: use push/pull.
  bool active() const { return m_state != State::Idle || m_fifoCount > 0; }
  bool wantsInput() const;
  // Aceita até `frames` quadros; retorna quantos guardou.
  size_t push(const float *interleaved, size_t frames);
  // Produz até `frames` quadros. Antes da mescla toca A do buffer; na mescla
  // nunca para (sem B a tempo usa silêncio no lugar dela, sem consumi-la).
  size_t pull(float *out, size_t frames);
  // Áudio já entregue pelo motor e ainda não tocado.
  size_t latencyFrames() const;
  TransitionEffect effect() const { return m_effect; }
  double progress() const;
  int64_t overlapFrames() const { return m_overlap; }
  int64_t mixStartFrame() const { return m_mixStart; }
  float limiterGain() const { return m_gain; }

private:
  struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<double, 2> z1{}, z2{};
    void lowpass(double rate, double hz, double q);
    void highpass(double rate, double hz, double q);
    float run(int channel, float x);
  };
  struct Comb {
    std::vector<float> line;
    size_t index = 0;
    float store = 0;
  };
  struct AllPass {
    std::vector<float> line;
    size_t index = 0;
  };

  void finishMixing();
  void renderAhead(int64_t until);
  void mixFrame(float bL, float bR, bool haveB, float &outL, float &outR);
  void tailAt(double position, float &l, float &r) const;
  void clearEffects();
  void fifoPop(float &l, float &r);

  int m_rate = 44100;
  State m_state = State::Idle;
  TransitionEffect m_effect = TransitionEffect::Fade;

  std::vector<float> m_tail;  // A ainda não tocada (FIFO anterior + restante)
  int64_t m_tailTarget = 0, m_tailCount = 0, m_tailPlayed = 0;
  int64_t m_mixStart = 0, m_overlap = 0;
  int64_t m_rendered = 0, m_mixed = 0;  // quadros da mescla renderizados/tocados
  double m_brakeRead = 0;               // leitura fracionária de A no freio

  std::vector<float> m_fifo;  // B (ou A sem transição) ainda não tocada
  size_t m_fifoHead = 0, m_fifoCount = 0, m_fifoCapacity = 0, m_fifoLimit = 0;

  // Mescla renderizada à frente da saída + limitador com antecipação.
  std::vector<float> m_render;
  std::vector<float> m_blockNeed;
  float m_gain = 1, m_limiterSlope = 0;

  // Efeitos
  std::vector<float> m_delayL, m_delayR;
  size_t m_delayIndex = 0, m_delayFrames = 0;
  Biquad m_lowA, m_highB;
  int m_filterCountdown = 0;
  std::array<Comb, 8> m_combs;
  std::array<AllPass, 4> m_allpass;
  int64_t m_rollStart = 0, m_rollLength = 0, m_rollStage = -1, m_rollPhase = 0;
};

} // namespace tonal::playback
