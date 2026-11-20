#pragma once
// Transições entre músicas da fila (Windows e Android).
//
// O mixer recebe o PCM estéreo float32 intercalado do motor antes do DSP.
// Ao ser armado perto do fim da faixa A, ele lê o restante de A adiantado
// (o motor decodifica mais rápido que o tempo real) e guarda esse trecho.
// Quando o PCM da faixa B começa a chegar, A e B tocam juntas durante a
// mesclagem, cada uma com o seu efeito. Depois da mesclagem o PCM volta a
// passar direto. Nenhuma operação de processamento aloca memória: os buffers
// são reservados no construtor. Não há rede, banco ou UI aqui.
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

std::string_view transitionEffectId(TransitionEffect effect);
// Retorna Count quando o identificador é desconhecido.
TransitionEffect transitionEffectFromId(std::string_view id);
int clampTransitionSeconds(int seconds);

// Escolha do modo Aleatório: sorteio uniforme entre os efeitos, sem repetir
// o anterior. `random` é qualquer inteiro de 32 bits do chamador.
TransitionEffect pickRandomTransition(TransitionEffect previous, uint32_t random);

// Quando armar. Retorna os milissegundos do restante de A que ainda estão no
// fluxo (após descontar o que já está na saída) ou -1 se ainda não é a hora.
// `remainingMs` = duração - posição audível; `bufferedMs` = áudio já na saída.
struct TransitionPlan {
  bool arm = false;
  int64_t tailMs = 0;     // quanto de A ler adiantado
  int64_t overlapMs = 0;  // duração da mesclagem desejada
};
TransitionPlan planTransition(int seconds, int64_t durationMs, int64_t remainingMs,
                              int64_t bufferedMs);

class TransitionMixer {
public:
  enum class State { Idle, Capturing, Mixing };
  explicit TransitionMixer(int sampleRate = 44100);
  void setSampleRate(int sampleRate);
  int sampleRate() const { return m_rate; }

  // Lê `tailFrames` de A adiantados; a mesclagem dura o que sobrar deles.
  bool arm(TransitionEffect effect, int64_t tailFrames);
  // Seek, troca de faixa pelo usuário, flush ou nova sessão: descarta tudo.
  void reset();
  // O motor parou de entregar A antes do previsto (sink fechado no fim
  // natural): mescla com o que já foi lido.
  void endCapture();

  State state() const { return m_state; }
  // Há algo armado, em mesclagem ou ainda no FIFO: use push/pull.
  bool active() const { return m_state != State::Idle || m_fifoCount > 0; }
  bool wantsInput() const;
  // Aceita até `frames` quadros; retorna quantos guardou.
  size_t push(const float *interleaved, size_t frames);
  // Produz até `frames` quadros. Na mesclagem, sem B disponível, retorna 0,
  // a menos que `padMissing` seja verdadeiro (usa silêncio no lugar de B
  // para não interromper o fim de A, sem consumir B).
  size_t pull(float *out, size_t frames, bool padMissing);
  size_t bufferedFrames() const;
  TransitionEffect effect() const { return m_effect; }
  double progress() const;
  int64_t overlapFrames() const { return m_overlap; }

private:
  struct Biquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    std::array<double, 2> z1{}, z2{};
    void lowpass(double rate, double hz, double q);
    void highpass(double rate, double hz, double q);
    float run(int channel, float x);
    void clear() { z1 = {}; z2 = {}; }
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

  void beginMixing();
  void finishMixing();
  void mixFrame(float bL, float bR, bool haveB, float &outL, float &outR);
  void tailAt(double position, float &l, float &r) const;
  void clearEffects();

  int m_rate = 44100;
  State m_state = State::Idle;
  TransitionEffect m_effect = TransitionEffect::Fade;

  std::vector<float> m_tail;  // trecho final de A
  int64_t m_tailTarget = 0, m_tailCount = 0;
  double m_tailRead = 0;      // posição de leitura de A (fracionária no freio)
  int64_t m_tailStart = 0;    // posição de A quando a mesclagem começou
  int64_t m_overlap = 0, m_mixed = 0;

  std::vector<float> m_fifo;  // PCM de B ainda não reproduzido
  size_t m_fifoHead = 0, m_fifoCount = 0;

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
