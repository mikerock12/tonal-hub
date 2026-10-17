# Plano de implementação — Motor de áudio avançado para player C++

**Escopo:** integrar **FFmpeg (decodificação + DSP)**, **OpenAL Soft (saída e virtualização HRTF)** e **IAMF/Eclipsa Audio (decodificação imersiva + Open Audio Renderer/OAR)** a um player de áudio já existente.

**Data de referência da pesquisa:** 14/10/2026  
**Status:** especificação de implementação; **não** representa código já integrado ou testado no aplicativo.  
**Plataforma inicial sugerida:** Windows x64; organizar interfaces para permitir portar para Linux e outras plataformas. Ajustar à arquitetura real do repositório antes de modificar arquivos.

> **Instrução para um agente de programação:** primeiro inspecione o projeto real (build, UI, serviço de reprodução, dependências, testes, persistência). **Não substitua nem reescreva o player inteiro.** Implemente incrementalmente, em módulos, preservando os comandos de play/pause/seek/playlist existentes. Não invente APIs de bibliotecas externas: confira os cabeçalhos e os exemplos correspondentes ao commit/versão fixados no projeto.

---

## 1. Resultado esperado

O player deve oferecer ao usuário:

1. Reprodução de MP3, FLAC, WAV, AAC e demais formatos suportados pela compilação do FFmpeg.
2. **Equalizador gráfico de 10 bandas**, opção de ativar/desativar e presets; ajuste opcional de bandas paramétricas posteriormente.
3. **Reforço de graves de 0 a +12 dB**, com limitação de picos para reduzir clipping.
4. **Compressor dinâmico** opcional.
5. **Normalização de volume entre faixas** (priorizar ReplayGain/EBU R128 e medições prévias; não confundir com compressão dinâmica).
6. **Modos espaciais**: estéreo original; HRTF/surround virtual para fones; áudio imersivo IAMF para arquivos compatíveis.
7. Detecção de formatos/capacidades e fallback automático, sem travar ou deixar o usuário sem áudio.
8. Preservação do sinal original quando todos os efeitos estiverem desligados, salvo conversão de formato necessária à saída.
9. Configurações persistidas por usuário, com aplicação suave e sem estalos.
10. Compatibilidade com saída estéreo e multicanal quando o dispositivo e as bibliotecas suportarem.

### Limitações que devem aparecer corretamente na interface

- **Dolby Atmos não é implementado nem decodificado por este projeto.** Não usar a marca como nome de recurso próprio.
- OpenAL Soft espacializa adequadamente fontes mono posicionadas e fontes multicanal; **uma música estéreo comum não vira uma mixagem 7.1.4 baseada em objetos** apenas por ativar HRTF.
- Eclipsa/IAMF depende de conteúdo efetivamente codificado nesse formato. Disponibilizar o modo somente quando houver suporte no arquivo/caminho de reprodução.
- OAR **renderiza** elementos de áudio imersivo; o decodificador IAMF e o demuxer do contêiner são etapas distintas. Algumas integrações de decoder já entregam áudio renderizado: nesse caso **não renderizar novamente com OAR**.
- **Nunca executar espacialização HRTF duas vezes**: PCM binaural do OAR deve ir diretamente à saída estéreo, sem passar por virtualização adicional.

---

## 2. Bibliotecas e fontes oficiais

| Componente | Papel | Fonte | Cuidados |
|---|---|---|---|
| FFmpeg: `libavformat`, `libavcodec`, `libavutil`, `libswresample`, `libavfilter` | Ler, decodificar, converter PCM, equalizar, comprimir e limitar | https://ffmpeg.org/ e https://ffmpeg.org/ffmpeg-filters.html | Revisar flags de compilação e licença LGPL/GPL |
| OpenAL Soft | Reproduzir PCM e aplicar HRTF/virtualização quando pertinente | https://github.com/kcat/openal-soft | Conferir extensão `ALC_SOFT_HRTF`, formatos multicanal e a licença do build |
| IAMF `libiamf` **ou** `iamf-tools` decoder | Decodificar conteúdo IAMF e tratar metadados | https://github.com/AOMediaCodec/libiamf e https://github.com/AOMediaCodec/iamf-tools | Escolher uma implementação inicial; não carregar dois decodificadores sem necessidade |
| Open Audio Renderer — OAR | Renderizar elementos de IAMF em alto-falantes ou binaural | https://github.com/AOMediaCodec/oar | Conferir entrada/saída, layouts e integração com o decodificador escolhido |
| IAMF (especificação) | Referência de conformidade e compatibilidade | https://github.com/AOMediaCodec/iamf | Registrar versão da especificação realmente suportada |

**Licenciamento:** FFmpeg costuma poder ser compilado sob LGPL, mas a inclusão de componentes GPL pode mudar a licença do conjunto. OpenAL Soft tem obrigações LGPL. Repositórios IAMF/OAR utilizam termos BSD-3-Clause-Clear e documentação adicional de patentes; conferir `LICENSE`, `PATENTS` e dependências efetivamente utilizadas. Distribuir avisos de terceiros, observar exigências de relink/substituição aplicáveis a bibliotecas LGPL e evitar concluir que “open source” significa “sem obrigações”. Fazer revisão jurídica antes de comercializar.

**Versionamento obrigatório:** fixar tags/commits em `third_party/versions.lock` ou equivalente, inclusive das dependências transitivas. Não depender da branch `main` em produção.

---

## 3. Arquitetura de áudio proposta

```text
                                    ┌───────────────────────────────────┐
                                    │ Player existente / Interface (UI) │
                                    │ play, pause, seek, volume, DSP    │
                                    └─────────────────┬─────────────────┘
                                                      │ comandos e eventos
                                    ┌─────────────────▼─────────────────┐
                                    │ AudioEngine / PlaybackController  │
                                    │ estado, filas, relógio, fallback  │
                                    └────────┬────────────────┬─────────┘
                                             │                │
                                  arquivo comum        arquivo IAMF válido
                                             │                │
                                  ┌──────────▼────┐  ┌────────▼─────────────┐
                                  │ FFmpeg demux  │  │ demux + IAMF decoder │
                                  │ + decoder     │  │ + metadados         │
                                  └──────────┬────┘  └────────┬─────────────┘
                                             │                │ elementos PCM
                                             │       ┌────────▼─────────────┐
                                             │       │ OAR (binaural ou     │
                                             │       │ layout de caixas)    │
                                             │       └────────┬─────────────┘
                                             │                │
                                  ┌──────────▼────────────────▼─────────┐
                                  │ DSP compartilhado / FFmpeg filters   │
                                  │ ganho → EQ/graves → compressor →    │
                                  │ normalização* → limiter → medidores │
                                  └─────────────────┬───────────────────┘
                                                    │
                                  ┌─────────────────▼───────────────────┐
                                  │ Roteador da saída / OpenAL Soft      │
                                  │ estéreo direto / multicanal HRTF    │
                                  │ / binaural OAR em estéreo direto    │
                                  └─────────────────┬───────────────────┘
                                                    ▼
                                          Fones / alto-falantes

* A normalização de faixa deve, preferencialmente, resultar de análise/metadata
  e ganho controlado; não aplicar normalização dinâmica indiscriminadamente.
```

### Contrato interno de PCM

- Representação de processamento: `float32` normalizado; preferir buffers **planar** enquanto necessário no FFmpeg e converter para o formato **interleaved** exigido pelo backend, na borda de saída.
- Amostragem: preservar frequência de origem quando possível; `48 kHz` é uma opção de referência para caminhos imersivos, **não** uma conversão obrigatória para todos os arquivos.
- Armazenar sempre: `sampleRate`, `frames`, `sampleFormat`, `channelLayout` com **ordem de canais explícita**, `pts`/timestamp e origem (`conventional`/`iamf`).
- Não presumir que seis canais estão sempre em uma mesma ordem. Traduzir `AVChannelLayout` do FFmpeg para os layouts aceitos pela saída/OAR.
- Fazer resampling apenas quando um estágio exigi-lo; utilizar `libswresample`/`aresample` com configuração consistente.
- Controlar volume de reprodução em um único ponto; DSP e OpenAL não devem multiplicar ganhos de forma inesperada.

### Políticas de roteamento espacial

| Fonte | Modo | Caminho correto |
|---|---|---|
| MP3/FLAC estéreo | Original | Decodificação → DSP → estéreo direto |
| MP3/FLAC estéreo | HRTF | Oferecer virtualização estéreo **somente quando o resultado for realmente diferente, validado auditivamente**; não rotular como surround 7.1 real |
| FLAC/WAV/filme 5.1 ou 7.1 | HRTF em fones | Decodificação multicanal → DSP preservando canais → OpenAL Soft HRTF → fones |
| Fonte mono/objetos de aplicativo | HRTF | Posicionar fontes no espaço do OpenAL Soft |
| IAMF com renderização binaural OAR | Imersivo | Decodificação IAMF → OAR binaural → DSP compatível → saída estéreo **sem segundo HRTF** |
| IAMF renderizado para caixas | Imersivo | IAMF → OAR layout de caixas → DSP → backend compatível com o layout físico |
| IAMF sem decoder/renderizador funcional | Imersivo indisponível | Avisar e tentar trilha convencional alternativa, **sem alegar compatibilidade** |

**Importante:** se o aplicativo atual já usa WASAPI/SDL/Qt Multimedia/outro backend de saída, considerar adaptadores para mantê-lo; não criar dois dispositivos de reprodução ativos simultaneamente. OpenAL Soft pode ser o backend único na primeira fase se os requisitos permitirem.

---

## 4. Organização sugerida do código

```text
src/
  audio/
    AudioEngine.hpp/.cpp             # coordena reprodução e eventos
    AudioSettings.hpp/.cpp           # estado validado, presets, persistência
    AudioFrame.hpp                   # PCM + layout + timestamps
    IAudioDecoder.hpp                # interface comum
    FfmpegDecoder.hpp/.cpp           # formatos tradicionais
    IamfDecoderAdapter.hpp/.cpp      # decoder IAMF e demux
    IamfRendererAdapter.hpp/.cpp     # OAR, layout e renderização
    DspGraph.hpp/.cpp                # wrapper de libavfilter
    LoudnessAnalyzer.hpp/.cpp        # LUFS, metadata, cache
    OpenALOutput.hpp/.cpp            # HRTF, buffers, estado do dispositivo
    AudioRouting.hpp/.cpp            # seleção de caminho e fallback
    AudioDiagnostics.hpp/.cpp        # logs, underruns, clipping, latência
  ui/
    AudioEffectsPanel.*              # integrar no framework UI já presente
    EqualizerPanel.*
resources/
  audio-presets.json
  third-party-notices/
tests/
  audio/
    unit/
    integration/
    fixtures/
third_party/
  versions.lock
```

Interfaces recomendadas:

```cpp
// Interface de arquitetura (contrato proposto, não API das bibliotecas externas)
struct AudioFrame {
    int sampleRate = 0;
    int channels = 0;
    int frames = 0;
    int64_t pts = 0;
    AudioChannelLayout layout;  // implementar: posições e ordem de cada canal
    std::vector<float> interleaved;
};

class IAudioDecoder {
public:
    virtual ~IAudioDecoder() = default;
    virtual bool open(const std::string& path) = 0;
    virtual bool read(AudioFrame& out) = 0;
    virtual bool seek(double seconds) = 0;
    virtual void close() = 0;
};

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;
    virtual bool start(const AudioFormat& format) = 0;
    virtual bool enqueue(const AudioFrame& pcm) = 0;
    virtual void stop() = 0;
};
```

Os tipos `AudioChannelLayout` e `AudioFormat` acima são **interfaces do aplicativo a criar**, não cabeçalhos fornecidos pelo FFmpeg/OpenAL. Preferir views/pools de memória para frames em produção e evitar cópias grandes dentro do callback de áudio.

---

## 5. Configurações, equalização e presets

### 5.1 Modelo de estado C++

```cpp
#include <array>
#include <cstdint>

enum class SpatialMode : uint8_t {
    OriginalStereo,
    HeadphoneHrtf,
    IamfImmersive
};

struct AudioSettings {
    bool eqEnabled = true;
    std::array<float, 10> eqGainsDb{}; // cada valor: -12 a +12 dB
    float bassBoostDb = 3.0f;         // 0 a +12 dB
    bool compressorEnabled = false;
    bool loudnessNormalization = true;
    float targetLufs = -16.0f;        // alvo ilustrativo, ajustável
    float ceilingDbTP = -1.0f;        // alvo de segurança de true peak
    SpatialMode spatialMode = SpatialMode::OriginalStereo;
    float masterVolume = 1.0f;       // 0.0 a 1.0
};
```

**Bandas do equalizador gráfico (Hz):** 31, 62, 125, 250, 500, 1.000, 2.000, 4.000, 8.000 e 16.000. O filtro paramétrico do FFmpeg usa largura (`Q` ou largura equivalente); calibrar sobreposições para os 10 controles não provocarem ganho excessivo. Para `31 Hz` e `16 kHz`, verificar resposta e limite de Nyquist conforme sample rate.

**Controles obrigatórios do painel:**

| Controle da interface | Comportamento | Persistência |
|---|---|---|
| Equalizador (liga/desliga) | `eqEnabled`; não alterar a origem em disco | Sim |
| 10 sliders de EQ | -12 a +12 dB; duplo clique restaura 0 | Sim |
| Reforço de graves | 0 a +12 dB; **0 = neutro** | Sim |
| Compressão dinâmica | on/off; avançado: threshold, ratio, attack, release | Sim |
| Normalização entre faixas | on/off; nível alvo, quando disponível | Sim |
| Áudio espacial | Original / HRTF / IAMF, com estados habilitados por capacidade | Sim, com fallback |
| Predefinições | Flat, Rock, Pop, Clássica, Eletrônica, Vocal, Personalizada | Sim |
| Restaurar padrão | Reset por seção e reset geral com confirmação | Sim |
| Indicador de saída | Ex.: “PCM estéreo 48 kHz”, “HRTF 5.1”, “IAMF binaural” | Derivado |

Presets são **preferências de escuta**, não prometem melhorar todas as gravações. Exemplo inicial conservador (validar por audição e medição):

```json
{
  "schemaVersion": 1,
  "name": "Rock",
  "eqEnabled": true,
  "eqHz": [31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000],
  "eqGainsDb": [0, 2, 1, -1, -1, 0, 1, 2, 1, 0],
  "bassBoostDb": 2,
  "compressorEnabled": false,
  "loudnessNormalization": true,
  "targetLufs": -16,
  "ceilingDbTP": -1,
  "spatialMode": "original"
}
```

Validação: rejeitar `NaN`/infinito; limitar sliders aos intervalos; `schemaVersion` para futuras migrações; persistir atomicamente (salvar arquivo temporário e renomear); alterações vindas da UI devem passar pelo mesmo validador.

### 5.2 DSP: ordem e prevenção de clipping

Ordem de referência, sujeita a testes:

1. **Ganho calculado da faixa** (ReplayGain/R128 quando disponível), com headroom apropriado;
2. **EQ e graves** (filtros shelving/peaking);
3. **Compressor** quando habilitado;
4. **Limitador final** para controlar picos digitais;
5. **Medição** de pico e loudness, com observação no destino efetivo.

A normalização é preferencialmente um **ganho de faixa definido antes da reprodução**. Para uma biblioteca musical, ler ReplayGain, R128 ou analisar em segundo plano/cache. O filtro `loudnorm` permite análise e normalização, mas pode envolver look-ahead, reamostragem e custo adicionais; não ativá-lo cegamente a cada bloco de áudio em tempo real. O `dynaudnorm` é alternativa opcional para fluxos/rádio, e **não substitui** a normalização entre faixas.

**Observação de segurança:** `alimiter` do FFmpeg atua nos picos de amostra e não é garantia isolada de teto em **true peak**. Para afirmar teto em dBTP, medir oversampling/true peak e validar em testes apropriados. Reservar headroom e evitar ganhos extremos.

### 5.3 Expressões de filtros FFmpeg — exemplos de prototipagem

Estas linhas são **exemplos CLI para verificar o resultado sonoro**, não funções C++ compiláveis por si só:

```bash
# Reforço discreto de graves, compressão leve e limitação de picos de amostra:
ffmpeg -i entrada.flac -af "bass=g=3:f=110,acompressor=threshold=0.125:ratio=2:attack=20:release=250,alimiter=limit=0.89" teste.wav

# Exemplo de EQ com uma banda de presença (1 kHz, +2 dB):
ffmpeg -i entrada.flac -af "equalizer=f=1000:t=q:w=1:g=2" teste_eq.wav

# Medição de loudness de uma faixa; registrar os resultados no cache:
ffmpeg -i entrada.flac -af "loudnorm=I=-16:TP=-1:LRA=11:print_format=json" -f null -
```

**IMPORTANTE:** nos testes iniciais, validar se todos os filtros existem na compilação local (`ffmpeg -filters`) e sua sintaxe na versão travada (`ffmpeg -h filter=bass`, `...acompressor`, `...alimiter`). O terceiro comando é um fluxo de medição/normalização de referência, não necessariamente um analisador de primeira passagem livre de processamento; para cálculo rigoroso de ReplayGain/R128 escolher um método de análise dedicado, documentado, e validar LUFS/true peak.

### 5.4 Integração real via `libavfilter`

- Alimentar `abuffer` com `AVFrame`s do decoder; conectar os filtros selecionados; ler de `abuffersink`.
- Usar APIs contemporâneas `AVChannelLayout`, `av_buffersrc_add_frame_flags` e `av_buffersink_get_frame`.
- Configurar e validar `sample_fmt`, `sample_rate`, `time_base` e `channel_layout` (sem usar exemplos antigos baseados apenas em bitmask `channel_layout` legado).
- Aplicar comandos de filtro quando a implementação os suportar; caso contrário, construir **novo grafo fora da thread crítica**, fazer troca controlada em um limite de bloco, com breve crossfade se necessário.
- Ao mudar frequência de amostragem, número/ordem de canais ou rota espacial, recriar os trechos afetados e limpar corretamente o estado dos filtros para evitar artefatos.
- Preservar canais do áudio multicanal até o ponto de virtualização: **não colocar um `aformat=...:channel_layouts=stereo` global** antes do HRTF.

Exemplo de algoritmo (pseudocódigo explícito):

```cpp
// PSEUDOCÓDIGO: adaptar às APIs e ao ownership real de AVFrame.
while (running) {
    AVFrame* decoded = decoder.receiveFrame();
    if (!decoded) break;

    dspGraph.sendFrame(decoded);           // av_buffersrc_add_frame_flags(...)
    while (dspGraph.hasProcessedFrame()) { // av_buffersink_get_frame(...)
        AVFrame* filtered = dspGraph.receiveFrame();
        outputQueue.push(convertForOutput(filtered));
    }
    releaseFrame(decoded);
}
// Propagar EOF, drenar filtros com atraso e esvaziar fila da saída.
```

Referência oficial de integração: https://ffmpeg.org/doxygen/trunk/filtering_audio_8c-example.html

---

## 6. OpenAL Soft: áudio espacial HRTF e saída

### Objetivo

Integrar OpenAL Soft como backend de saída, permitindo HRTF para fontes multicanal/posicionais e um caminho estéreo direto para conteúdo que já esteja binauralizado.

### Inicialização ilustrativa em C++

```cpp
#include <stdexcept>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

struct OpenAlContext {
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    bool open(bool requestHrtf) {
        device = alcOpenDevice(nullptr);
        if (!device) return false;

        const bool extensionAvailable =
            alcIsExtensionPresent(device, "ALC_SOFT_HRTF") == ALC_TRUE;
        const ALCint attributesHrtf[] = { ALC_HRTF_SOFT, ALC_TRUE, 0 };
        const ALCint attributesDefault[] = { 0 };
        context = alcCreateContext(device,
            (requestHrtf && extensionAvailable)
                ? attributesHrtf : attributesDefault);

        if (!context || alcMakeContextCurrent(context) != ALC_TRUE)
            return false; // no app real, chamar close() e reportar erro

        if (requestHrtf && extensionAvailable) {
            ALCint active = ALC_FALSE;
            alcGetIntegerv(device, ALC_HRTF_SOFT, 1, &active);
            return active == ALC_TRUE;
        }
        return true;
    }

    void close() {
        if (context) { alcMakeContextCurrent(nullptr);
                       alcDestroyContext(context); context = nullptr; }
        if (device) { alcCloseDevice(device); device = nullptr; }
    }
};
```

**Atenção:** exemplo minimalista de inicialização e verificação, **não** de playback completo. Em produção: tratar erros/cleanup em todos os caminhos, RAII, disponibilidade de dispositivo, hotplug, threads e status HRTF separadamente do sucesso na abertura. No exemplo, `open(true)` pode retornar `false` se HRTF não estiver ativo **mesmo com contexto aberto**; o chamador deve decidir se mantém a saída estéreo ou realiza fallback.

### Reproduzir PCM sem cortes

1. Criar `ALuint source` e uma fila circular de buffers (`alGenBuffers`, `alSourceQueueBuffers`).
2. Converter PCM final para formato aceito (`AL_FORMAT_STEREO16` para MVP ou `AL_EXT_FLOAT32` e formatos multicanal suportados).
3. Processar blocos regularmente, recuperar os buffers já tocados via `AL_BUFFERS_PROCESSED`/`alSourceUnqueueBuffers` e reabastecer.
4. Monitorar `AL_SOURCE_STATE`, underruns e tempo; usar pausa/seek do player para gerenciar fila e timestamps.
5. Para virtualização 5.1/7.1, usar **formato multicanal e ordem de canais corretos**, conferir `AL_EXT_MCFORMATS`/outros formatos efetivamente disponíveis.
6. Conferir a extensão `ALC_SOFT_HRTF` e o estado real, não apenas o toggle da UI. Seleção de HRTFs pode usar `alcGetStringiSOFT` e `alcResetDeviceSOFT`, quando disponíveis.
7. Ao desabilitar HRTF ou sair de IAMF, evitar estado residual e estalos. Recriar/resetar contexto/dispositivo quando exigido, fora do callback crítico.

**Caso estéreo:** OpenAL Soft pode aceitar fontes estéreo, mas não espere que um MP3 estéreo simplesmente ganhe objetos 3D. Se o produto desejar o botão “Som envolvente para estéreo”, implementar/avaliar um algoritmo próprio de crossfeed, largura estéreo ou upmix **claramente separado** e etiquetado como virtualização, com bypass A/B. Não inventar canais discretos onde não existem.

Referências: https://github.com/kcat/openal-soft/blob/master/docs/hrtf.txt e https://github.com/kcat/openal-soft/blob/master/examples/alhrtf.c

---

## 7. IAMF / Eclipsa Audio + OAR

### 7.1 Estratégia correta

A implementação tem **etapas distintas**:

1. **Detecção:** reconhecer fluxos IAMF e o contêiner real (`.iamf`, MP4 compatível etc.); testar demux por capacidade do build.
2. **Decodificação:** escolher inicialmente `libiamf` **ou** `iamf-tools` `IamfDecoderInterface` e obter elementos PCM e metadados necessários.
3. **Renderização:** verificar se o decoder escolhido já fornece PCM final renderizado. Se **não** fornecer, alimentar o OAR com elementos de áudio, layouts, timestamps e metadados/posicionamento.
4. **Saída:** solicitar a OAR binaural para fones ou layout físico adequado para alto-falantes.
5. **DSP e playback:** aplicar efeitos sem destruir a espacialidade, depois enviar PCM final ao backend sem repetir HRTF.

Não assumir que passar uma sequência de bytes `.iamf` diretamente a `oar_render()` é suficiente. O README do OAR descreve a criação de instância e o fornecimento de **elementos e metadados** antes da renderização.

### 7.2 Passos para integrar OAR

O README atual da biblioteca documenta, entre outras, estas funções C:

- Configurar alvo de renderização e criar instância `oar_t` conforme a versão escolhida;
- `oar_add_audio_group` para os grupos;
- `oar_add_audio_element` com tipo/layout específico;
- `oar_update_audio_element_data` para blocos de áudio decodificados;
- `oar_update_audio_element_metadata` quando existirem metadados de cena/objetos;
- `oar_render` para gerar o bloco de saída;
- Encerrar e liberar o renderizador conforme a API vigente.

Configurações documentadas de referência:

- Layout de saída binaural: `ck_oar_layout_binaural`;
- Exemplos de entrada por canais: `ck_oar_layout_51`, `ck_oar_layout_714`;
- Renderização de conteúdo baseado em canais, em cena/Ambisonics e em objetos;
- Recursos opcionais de limitador, processamento de loudness e head tracking (conforme versão/build).

**Cuidado:** em um mesmo `oar_t`, a implementação documenta restrição à mistura de elementos **object-based** com tipos channel/scene-based. Implementar agrupamento/roteamento respeitando tais limites. Desabilitar o limitador/loudness redundante em OAR ou no DSP compartilhado quando houver risco de dupla compressão/normalização.

**Validação da integração:** inicialmente criar um **programa de teste isolado** que leia amostra IAMF legalmente utilizável e gere WAV binaural, comparando a saída com as ferramentas de referência. Só depois ligar ao player em streaming, preservando pausas, seek e troca de faixa.

Referências oficiais:

- https://github.com/AOMediaCodec/oar
- https://github.com/AOMediaCodec/libiamf
- https://github.com/AOMediaCodec/iamf-tools/blob/main/docs/iamf_decoder_interface.md
- https://github.com/AOMediaCodec/iamf-tools

### 7.3 Regras para a interface

- Se a faixa não possui IAMF, exibir “**IAMF indisponível para esta faixa**” e desabilitar sua seleção. Manter “Original” e os recursos realmente disponíveis.
- Mostrar “IAMF binaural” **somente se a cadeia IAMF → renderizador binaural estiver efetivamente ativa**.
- Para fones, preferir renderização binaural do OAR. Se o OpenAL Soft estiver na ponta final, usar a rota direta sem HRTF.
- Head tracking deve ficar **desabilitado** sem hardware/sensores e implementação adequada.
- Versões de IAMF não suportadas: diagnóstico legível, fallback sem crash e registro da versão detectada.

---

## 8. Construção e dependências

### 8.1 Ferramentas

- Compilador C++ com suporte a **C++20** quando requerido por dependências atuais; adequar às ferramentas já usadas no projeto.
- CMake + Ninja/MSBuild, Git e opcionalmente vcpkg/Conan.
- FFmpeg **dev headers e bibliotecas** da mesma versão do runtime.
- OpenAL Soft dev headers/biblioteca.
- IAMF decoder e OAR compilados com a mesma arquitetura ABI/configuração do aplicativo.

### 8.2 Ordem de instalação sugerida

1. Criar branch de integração e testes de reprodução de referência.
2. Adicionar FFmpeg e reproduzir MP3/FLAC/WAV **sem efeitos**.
3. Adicionar `libavfilter` e um EQ simples com bypass.
4. Adicionar compressor/normalização/limitador e medidores.
5. Adicionar OpenAL Soft e alternar original/HRTF com conteúdo multicanal.
6. Compilar decoder IAMF e OAR **separadamente**, rodar testes oficiais.
7. Integrar IAMF por feature flag, com fallback.
8. Criar pacote de distribuição e verificar licenças e carregamento de DLLs.

### 8.3 CMake: exemplo de ligação parcial

```cmake
# ESQUELETO: demonstra dependências; adaptar ao target real e ao sistema.
cmake_minimum_required(VERSION 3.24)
project(AudioPlayer LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

find_package(OpenAL REQUIRED)
find_package(PkgConfig QUIET)

# Exemplo para um ambiente com .pc configurados (frequente em Linux/MSYS2).
# Para Windows/MSVC + vcpkg/Conan, usar targets/exportações correspondentes
# à instalação efetiva, sem presumir que estes nomes sempre existirão.
if(PkgConfig_FOUND)
  pkg_check_modules(AVFORMAT IMPORTED_TARGET libavformat)
  pkg_check_modules(AVCODEC  IMPORTED_TARGET libavcodec)
  pkg_check_modules(AVUTIL   IMPORTED_TARGET libavutil)
  pkg_check_modules(AVFILTER IMPORTED_TARGET libavfilter)
  pkg_check_modules(SWRESAMPLE IMPORTED_TARGET libswresample)
endif()

# Anexar aos targets já existentes, se os módulos foram encontrados:
# target_link_libraries(<target_existente> PRIVATE OpenAL::OpenAL
#   PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL
#   PkgConfig::AVFILTER PkgConfig::SWRESAMPLE)
# OAR e IAMF: compilar conforme README da revisão fixada e conectar
# com o target/export efetivamente definido; não supor nome sem conferir.
```

**Não copiar cegamente:** o bloco é uma referência de dependências e não um `CMakeLists.txt` completo da aplicação; os nomes dos targets e descoberta de pacotes variam. Preferir link dinâmico em distribuição LGPL quando apropriado, manter as DLLs e arquivos de licença exigidos.

### 8.4 Recursos de runtime

- Empacotar apenas as DLLs/arquivos de dados efetivamente necessários.
- Manter HRTFs redistribuídas apenas com licença compatível.
- Registrar versão de cada biblioteca e disponibilidade das extensões no menu “Sobre → Diagnóstico de áudio”.
- Não aceitar caminhos externos de bibliotecas sem validação (evitar DLL hijacking).

---

## 9. Concorrência e mudanças de configuração em tempo real

Separar pelo menos **UI**, **decodificação/DSP** e **saída**. Regras:

- A UI publica um `AudioSettings` validado numa fila de comandos; **não** altera diretamente objetos OpenAL nem FFmpeg de outra thread.
- Decodificador abastece um buffer em anel pré-alocado; a saída consome sem esperar leitura de disco, JSON ou construção de filtros.
- Usar clocks/timestamps para sincronizar seek, progresso, pausa e troca de faixa.
- Novos ganhos e filtros devem ser aplicados com rampa/interpolação/crossfade curto (p. ex. 10–50 ms, a validar), evitando pops.
- Trocas de HRTF/layout e outros filtros que exijam reconfiguração são feitas fora da thread crítica, com troca coordenada e rebuffer controlado.
- Evitar mutexes demorados, alocações excessivas, logs síncronos e operações de arquivo no callback de áudio.
- Drenar filtros com latência interna e tratar EOF/seek; invalidar cache de PCM após mudança de timestamp.
- Implementar `AudioDiagnostics`: underruns, sample rate, canais, HRTF ativo, limite de pico, latência estimada, falhas de decoder e fallback.

---

## 10. Estratégia de implementação por entregas

### Marco 0 — Auditoria do projeto existente

- [ ] Identificar GUI (Qt, ImGui, Win32, SDL etc.), sistema de build e plataforma-alvo.
- [ ] Localizar decodificador, saída, controles atuais, volume e persistência.
- [ ] Estabelecer ponto de interceptação de PCM e escrever um teste de áudio “sem efeitos”.
- [ ] Documentar a API e eventos existentes para preservá-los.

### Marco 1 — FFmpeg DSP e equalizador funcional

- [ ] Vincular `libavfilter` sem quebrar o player.
- [ ] Criar `AudioSettings` e painel de efeitos.
- [ ] Implementar EQ de 10 bandas com bypass real; criar presets.
- [ ] Implementar graves 0–12 dB; 0 dB deve ser neutro.
- [ ] Implementar compressor opcional e limitador no fim da cadeia.
- [ ] Implementar normalização entre faixas baseada em metadata/análise prévia com cache.
- [ ] Validar `aformat`, layouts e ausência de clipping evidente nas predefinições.
- [ ] Salvar e carregar configurações JSON.

### Marco 2 — OpenAL Soft

- [ ] Backend de saída com fila de buffers, pausa/seek e tratamento de underrun.
- [ ] Detectar `ALC_SOFT_HRTF`; confirmar resultado real da ativação.
- [ ] Testar espacialização em fonte **5.1/7.1** e saída para fones estéreo.
- [ ] Manter sinal estéreo original disponível e sem efeitos não solicitados.
- [ ] Evitar reprodução por dois backends simultâneos.
- [ ] Testar troca de dispositivo e retorno de suspensão do computador.

### Marco 3 — IAMF + OAR

- [ ] Escolher decoder IAMF e versão; testar standalone e, se aplicável, contêiner MP4.
- [ ] Compilar e exercitar OAR fora da aplicação.
- [ ] Produzir WAV binaural validado com amostra IAMF de teste.
- [ ] Integrar decoder → renderer → DSP → saída estéreo direta.
- [ ] Implementar disponibilidade condicional do modo IAMF.
- [ ] Testar layouts, metadata, seek, erros, formatos não suportados e memória.
- [ ] Documentar claramente versões/perfis do IAMF suportados.

### Marco 4 — Qualidade e entrega

- [ ] Benchmark CPU/memória/latência nos perfis desligado, EQ, HRTF e IAMF.
- [ ] Testar transições entre faixas, presets, sample rates e dispositivos.
- [ ] Executar testes automatizados, sanitizers quando suportados e análise estática.
- [ ] Organizar `licenses/`, notices e hashes/versões das bibliotecas.
- [ ] Atualizar ajuda do usuário e publicar changelog.

---

## 11. Plano de testes e critérios de aceite

| Cenário | Resultado esperado |
|---|---|
| EQ desligado e graves 0 dB | Sem alteração intencional de timbre; tolerar apenas conversão de PCM necessária |
| Slider +3 dB em banda de 1 kHz | Resposta de frequência mensurável, sem mudança irrelevante de outras bandas |
| Graves +12 dB | Sem overflow/NaN; limitar picos; advertir sobre clipping/possível mudança de dinâmica |
| Compressor ligado/desligado | Diferença mensurável de dinâmica, sem pops durante comutação |
| Normalização entre 2 músicas de loudness distinto | Percepção mais consistente, dentro da tolerância de LUFS configurada |
| MP3/FLAC estéreo + modo original | Canais esquerdo/direito corretos, sem surround indevido |
| FLAC/WAV 5.1 + HRTF | Canais na posição correta; HRTF ativo confirmado; saída para fones |
| IAMF compatível + OAR binaural | Renderização binaural funcional; OpenAL sem segundo HRTF |
| MP3 comum + seleção de IAMF | Recurso indisponível informado; playback preservado |
| Seek repetido e troca de faixa | Sem crash, vazamento, silêncio prolongado ou som residual |
| Sample rates 44,1/48/96 kHz | Taxa correta/reamostragem controlada, sem alteração indevida da duração |
| Hotplug USB/Bluetooth/HDMI | Fallback ou recuperação clara, sem travar a UI |
| Arquivo truncado/malformado | Falha controlada, sem leitura fora de limites ou travamento |

**Métricas sugeridas para instrumentação (não são resultados de benchmark):** underruns = 0 em playback estável; clipping = 0 no caminho de saída configurado; tempo médio/p95 de DSP por bloco; latência fim-a-fim medida; vazamentos = 0 em testes de ciclo; CPU e memória comparadas ao player antes da integração. Para fidelidade, comparar bypass por null test quando as condições de sample rate, ganho e alinhamento permitirem.

**Fixtures:** senoides em diversas frequências, impulso, ruído rosa, varredura, arquivos estéreo L/R, 5.1/7.1 identificados canal a canal, formatos compactados e conteúdos IAMF com licença de uso para testes. Em testes binaurais, usar fones sem outra virtualização ativada no Windows/driver.

---

## 12. Erros conhecidos a evitar

1. **HRTF em dobro:** OAR binaural + OpenAL HRTF ligado para esse mesmo sinal.
2. **Downmix prematuro:** converter 5.1 para estéreo antes da virtualização de canais.
3. **Normalização equivocada:** confundir aumentar volume com uniformizar loudness; aplicar compressor e loudnorm sempre ligados e gerar bombeamento.
4. **Clipping:** somar +12 dB de graves, várias bandas positivas e ganho sem headroom/limitador.
5. **Dados antigos do FFmpeg:** usar APIs obsoletas de layouts/decodificação em projeto novo.
6. **Falso suporte a IAMF:** mostrar modo imersivo quando apenas foi identificado um arquivo sem decodificação/renderização operacional.
7. **Estéreo falso 7.1:** duplicar L/R em oito canais e anunciar objetos espaciais inexistentes.
8. **UI travando:** recriar dispositivos, carregar HRTF ou analisar LUFS diretamente na thread da interface.
9. **Falha de licenças:** redistribuir build GPL sob a suposição de que tudo é LGPL; omitir avisos e bibliotecas substituíveis onde exigido.
10. **Dependências frágeis:** rastrear `main` em produção e copiar exemplos de API incompatíveis com o commit travado.

---

## 13. Texto pronto para instruir uma IA de programação

> Analise este repositório C++ e implemente **incrementalmente** o motor de áudio detalhado neste documento. Preserve player, biblioteca de músicas, interface e persistência existentes. Comece auditando a arquitetura e apresente quais arquivos precisará alterar. Implemente primeiro FFmpeg/libavfilter: EQ 10 bandas, reforço de graves 0–12 dB, compressor opcional, normalização entre faixas e limitador. Em seguida, integre OpenAL Soft como saída com modo original e HRTF condicionado à capacidade real. Só então implemente suporte IAMF com decodificador escolhido e OAR binaural, sob feature flag. Não aplique HRTF duas vezes, não converta multicanal para estéreo antes da espacialização e não anuncie Dolby Atmos/7.1.4 para arquivos estéreo comuns. Adapte as dependências à plataforma e ao CMake já presentes. Use APIs verificadas nas versões fixadas, trate erros e cleanups, escreva testes e execute build após cada marco. Se uma biblioteca ou recurso ainda não compilar, mantenha fallback funcional e documente o bloqueio; não simule sucesso.

### Definição de pronto

A implementação estará pronta quando o usuário puder reproduzir uma música normalmente, ajustar o EQ de 10 bandas e o grave em tempo real sem interrupções, ativar/desativar compressor e normalização, alternar original/HRTF quando suportado, reproduzir um arquivo IAMF validado em binaural pelo OAR sem HRTF duplicado, persistir preferências e recuperar-se de erros de formato/dispositivo sem encerrar o aplicativo.

---

## 14. Referências adicionais de implementação

- FFmpeg — referência de filtragem de áudio em C: https://ffmpeg.org/doxygen/trunk/filtering_audio_8c-example.html
- FFmpeg — todos os filtros e opções: https://ffmpeg.org/ffmpeg-filters.html
- FFmpeg — licença: https://ffmpeg.org/legal.html
- OpenAL Soft — repositório: https://github.com/kcat/openal-soft
- OpenAL Soft — HRTF: https://github.com/kcat/openal-soft/blob/master/docs/hrtf.txt
- OpenAL Soft — seleção de HRTF: https://github.com/kcat/openal-soft/blob/master/examples/alhrtf.c
- AOM — IAMF: https://github.com/AOMediaCodec/iamf
- AOM — libiamf: https://github.com/AOMediaCodec/libiamf
- AOM — iamf-tools: https://github.com/AOMediaCodec/iamf-tools
- AOM — OAR: https://github.com/AOMediaCodec/oar

**Nota final:** este documento propõe a arquitetura, os controles, a ordem de implementação e exemplos de referência. O código ilustrativo não foi compilado contra o projeto do usuário, que não foi fornecido nesta conversa. A integração final depende do backend atual, framework gráfico, plataforma-alvo, versão das bibliotecas e conteúdo de teste IAMF disponível.
