# Transições entre músicas, fila e letra — 0.17.0 / 0.17.1 / 0.17.3

## Transições de DJ

Com **Transições** ligadas, o fim de cada música da fila se mistura com o
começo da próxima. O ícone fica ao lado de Descoberta 2+1 na barra do player
e na página da música (Windows) e junto de Fila e Jam na página da música
(Android). A escolha vale para toda lista de reprodução tocada pelo Tonalify.

| Efeito | O que se ouve |
|---|---|
| Aleatório | Um efeito diferente a cada troca, sem repetir o anterior. |
| Fade cruzado | A atual baixa enquanto a próxima sobe com potência constante. |
| Eco de despedida | A atual vira ecos pingue-pongue (0,36 s) que se apagam sob a próxima. |
| Troca de graves | Passa-baixa fecha a atual (18 kHz → 250 Hz); a próxima entra sem graves e os recebe no fim. |
| Freio de vinil | A atual desacelera até parar, como um toca-discos desligado; a próxima cai inteira em seguida. |
| Banho de reverb | A atual se dissolve num reverb de salão (oito pentes e quatro passa-tudo) sob a próxima. |
| Loop acelerado | Trechos de 1/2, 1/4, 1/8, 1/16 e 1/32 s da atual se repetem até a próxima entrar. |

O tempo de mesclagem vai de 3 a 10 segundos (padrão 6). Em músicas curtas
ele é limitado a um terço da duração; abaixo de 1,5 s não há transição.

### Como funciona

O núcleo é C++ compartilhado (`src/playback/TransitionMixer.cpp`), usado pelo
motor do Windows e, por JNI, pelo Android (`android/muse/TransitionJni.cpp`).
Perto do fim da faixa A o mixer é armado com o restante de A. Ele lê esse
restante adiantado, pois o librespot decodifica mais rápido que o tempo real,
e guarda até 14 s em memória reservada uma única vez (~5 MB). Quando o PCM da
faixa B chega, A e B tocam juntas durante a mesclagem, cada uma com a curva
do efeito. Depois disso B passa direto, sem atraso acumulado: o programa
fica mais curto exatamente pelo tempo da mesclagem.

* A interface pode mostrar B já durante a mesclagem, que é quando B
  começa a soar.
* Pausar mantém a mescla; pular, voltar, buscar um trecho, escolher outra
  música ou trocar de aparelho descarta a transição e troca na hora.
* Não há transição na repetição de uma música só, nos arquivos locais, no
  IndieTonal nem com reprodução em outro aparelho Spotify Connect.
* Windows: requer o processamento de áudio do Tonal ligado (padrão). A troca
  natural de faixa deixa de limpar o buffer enquanto a mescla está ativa.
  A última música conhecida da fila termina sem efeito.
* Android: o gatilho segue os eventos do motor (`TrackChanged` e posição),
  funciona em segundo plano e agenda a leitura com 400 ms de folga para o
  buffer do AudioTrack. Se o motor fechar o sink no fim natural, uma thread
  curta toca o restante de A até a próxima chegar ou a mescla acabar.
* Nada disso roda no callback de áudio além do próprio processamento: sem
  rede, banco, arquivo ou interface; nenhuma alocação durante a mescla.

### Limites

Os efeitos não analisam BPM, tonalidade nem batida; o Loop acelerado usa
durações fixas, não alinhadas ao compasso da música. A sobreposição exata
depende da posição informada pelo Spotify (Windows) e da velocidade com que
o motor entrega o fim da faixa. Os testes são sintéticos (tons e silêncio)
e de compilação; não houve medição acústica, de CPU/bateria ou teste em
aparelho físico com Spotify real nesta entrega.

## Fila: Colocar como próxima

**Colocar como próxima da fila** sempre entra logo depois da música atual.
**Adicionar à fila** entra depois do bloco de próximas, na ordem em que foi
adicionado. Exemplo: Próxima B, Adicionar C, Próxima D, Adicionar E resulta em
Atual → D → B → C → E → demais músicas. O marcador interno `queueAdded` é
negativo para "próxima", positivo para adições e zero para faixas comuns.
Arrastar uma música marcada para fora do bloco do início a tira do bloco.

## Cabeçalho da letra

Atualizar, tradução automática, fontes, tela cheia, sincronia e baixar o
ajuste ficam como ícones no cabeçalho da letra (Windows: página Letra e
página da música; Android: página da música). Tradução automática e
sincronia são alternadores: azul (#35A7FF) ligado, branco desligado.
Desligar a tradução oculta as linhas traduzidas sem apagar a tradução.
Na tela cheia do Windows, a sincronia também virou ícone.

## 0.17.1 — buffer antes da transição

O usuário relatou na 0.17.0: o som travava, depois vinha a transição e ainda
havia uma pausa antes da música seguinte. Causas encontradas no código:

* Windows: ao armar, o laço de áudio lia o fim da faixa A sem limite enquanto
  o librespot entregava dados; a fila do OpenAL (cerca de 70 ms) esvaziava.
* Android: a saída só era escrita dentro das chamadas do motor; quando ele
  parava para carregar B, o fim de A guardado ficava sem tocar.
* Nos dois: a mescla começava assim que A terminava de ser lida; se B demorasse,
  o efeito rodava com silêncio no lugar dela e B entrava depois.

Funcionamento desde a 0.17.1 (núcleo C++ igual nas duas plataformas):

1. O mixer é armado quando faltam mescla + 2,5 s + 0,5 s de A no som.
2. Lê todo o restante de A e, em seguida, até 2,5 s do começo de B, enquanto a
   saída continua tocando A do próprio buffer. Windows: no máximo 8 blocos por
   ciclo, a saída é atendida a cada ciclo. Android: thread de saída própria e
   escrita não bloqueante do motor até o limite do buffer.
3. A mescla começa num ponto fixo do fim de A, com B já guardada. Os quadros são
   renderizados à frente da saída e passam por um limitador com antecipação de
   256 quadros, que só reduz o ganho se a soma A+B passasse de 0 dBFS (antes era
   uma saturação suave). Fora disso, A e B saem exatamente como chegaram.
4. Depois da mescla, os 2,5 s de B ficam como atraso constante entre o motor e o
   som. A barra, a letra e os controles de mídia descontam esse atraso; a
   próxima transição reaproveita o buffer. Pular, buscar ou escolher outra
   música descarta o buffer e o atraso some.
5. Windows: o volume do Spotify é aplicado no librespot, antes do buffer. O motor
   compensa o trecho guardado (curva logarítmica de 60 dB do librespot) para a
   mudança soar na hora. Android usa o volume do sistema, sem atraso.

Testes da 0.17.1: núcleo com B atrasada (sem travar), duas transições seguidas,
limitador ≤ 0,999 com variação suave, `endCapture`; motor Windows real com um
produtor que demora 0,8 s para entregar B (0 underruns, pré-rolagem 2,5 s);
AudioTrack real na AVD com B atrasada 1 s (leitura de 6 s de A em 3 ms, 0
underruns, segunda transição com o atraso da primeira). Continuam pendentes o
teste com Spotify real, no aparelho físico, e medições acústicas.

Limites conhecidos: o título da próxima música pode aparecer alguns segundos
antes de ela soar, porque o Spotify já a considera em reprodução; se outro
aparelho Spotify Connect pular a música no Windows, até 2,5 s da anterior ainda
podem tocar; uma mudança de volume no Windows enquanto o pedido ainda não chegou
ao librespot pode oscilar por uma fração de segundo.

## 0.17.3 — fim exato da faixa e começo da próxima ajustável

Relato do usuário na 0.17.1: todas as transições só baixavam ou faziam oscilar
o volume da música atual no final, e a próxima começava sem efeito nenhum.

Causa: o mixer decidia onde a faixa A terminava dentro do fluxo de PCM por uma
estimativa: duração menos a posição informada. No Windows essa posição vem da
Web API do Spotify e é extrapolada pelo relógio, podendo errar em segundos; no
Android vem dos eventos de posição do motor. Quando a estimativa era curta, o
fim real de A ficava fora do buffer e entrava no lugar de B: a mescla misturava
A com ela mesma (A desce e o próprio fim sobe, a "oscilação"), e a B de verdade
só começava depois da mescla, crua. O teste `TransitionChecks` reproduz esse
caso com a estimativa 4 s curta.

Correção:

1. **Marcador exato no librespot.** Em `start_playback` o librespot grava no
   próprio fluxo, antes da primeira amostra de cada faixa, um quadro estéreo
   f32 que nunca é áudio: esquerda `0x7FA07A1B` (NaN sinalizador) e direita
   NaN silencioso com a posição inicial em ms. Android:
   `android/native/vendor/librespot-playback` e `AndroidSink` chamam
   `NativeBridge.onTrackBoundary` na mesma thread e ordem do PCM. Windows:
   `packaging/prepare-librespot-windows.py` aplica o mesmo trecho ao
   `librespot-playback` 0.8.0 fixado por SHA-256; `NativePcmPipe` remove o
   quadro e o `AudioEngine` o repassa ao mixer.
2. **O mixer espera o marcador.** Armado, ele lê A até o marcador (buffer de
   até 30 s, cerca de 10 MB a 44,1 kHz, reservado uma única vez). A região da
   mescla é posicionada pelo quadro exato do fim de A e a mescla só começa
   quando ele chega. Se o buffer encher sem marcador, A e B tocam inteiras, sem
   efeito, em vez de misturar A com ela mesma. Com um librespot sem marcador,
   a estimativa antiga continua valendo.
3. **Duas durações.** *Fim da música atual com o efeito* (3–10 s, o antigo
   tempo de mesclagem) e *Começo da próxima na mescla* (3–10 s, novo). A
   próxima entra por baixo da atual pelo tempo escolhido, com rampa de 10 ms,
   e as duas terminam a mescla juntas, no último quadro da atual. A região é a
   maior das duas; o efeito ocupa o fim da atual. Freio de vinil termina a
   frenagem em 70% do trecho da atual, e o Loop acelerado corta em 82%, para a
   próxima já estar soando quando a atual para.
4. **Posição exata para armar.** Windows e Android contam os quadros lidos desde
   o marcador (`AudioEngine::spotifyTrackPositionMs`,
   `AudioOutput.trackPositionMs`). Depois de um seek a contagem deixa de valer
   até a próxima faixa e volta a valer a posição informada.

Testes da 0.17.3:

* Núcleo: seis efeitos em três formatos (6/6, 4/9 e 9/3 s), com B alinhada
  exatamente `intro` antes do fim de A e intacta depois. Estimativas erradas
  em −4, −2, +3 e +5 s com o marcador. O defeito reproduzido sem marcador.
  Buffer cheio sem marcador, duas transições seguidas, limitador e `endCapture`.
* Motor Windows real com o librespot simulado emitindo o marcador e a
  estimativa 3 s errada: fim de A pelo marcador, posição de B recomeçando nele,
  0 underruns.
* QML: o diálogo tem os dois controles, de 3 a 10 s.
* Android: AVD com AudioTrack real e o marcador. Resultado em
  docs/release-0173-tests.json.

Continuam pendentes: teste com Spotify real (o marcador sai do librespot
compilado, mas não foi ouvido com uma conta), aparelho físico e medições
acústicas, de CPU ou bateria.


## 0.17.4 — correção estrutural (21/11/2026)

O usuário relatou de novo, na 0.17.3, que as transições só baixavam ou
oscilavam a música atual e a próxima começava sem efeito. A correção seguiu o
plano estrutural que ele escreveu (identificação TH-001 a TH-010) e foi
medida, desta vez, com o Spotify real no Windows.

### Causa no Android (confirmada no código)

O librespot embarcado no Android tem uma recuperação de buffering do Tonal
(0.15.x, `vendor/librespot-connect/src/spirc.rs`): um fim de faixa mais de 2 s
antes da duração é tratado como falta de dados. Ele pausa, espera 5 s e
recarrega a mesma faixa na última posição conhecida. O Spirc só conhece essa
posição pelos eventos, um por segundo.

Na transição o mixer lê o fim de A de uma vez, em fração de segundo, sem novos
eventos de posição. O fim real chegava com a posição conhecida segundos atrás
e virava "falta de dados".

O que se ouvia:

1. A tocava com o efeito sobre silêncio no lugar de B (baixando ou oscilando).
2. A recarga de A, alguns segundos antes do fim, entrava na mescla no lugar
   de B.
3. Depois A terminava de novo e B começava sem efeito.

No Windows o librespot usa o Spirc original, sem essa recuperação. Por isso o
teste com Spotify real abaixo mesclou A e B certo.

Correções:

* `vendor/librespot-playback/src/player.rs` (`tonal_end_of_track`): todo fim de
  faixa vai precedido da posição exata do decodificador (`PositionChanged`).
  O fim natural chega com a posição na duração e segue para a próxima. Uma
  falta de dados verdadeira continua sendo recuperada, agora a partir da
  posição real.
* Se o motor ainda assim pausar e retomar a mesma faixa durante uma transição,
  o marcador da retomada tem posição maior que zero. Ele não conta como B nem
  como nova ocorrência: `TransitionMixer::resumeCapture()` volta a ler A até o
  marcador real de B (Windows e Android).

### Mixer (TH-001, TH-008, TH-010)

* **B pronta antes da mescla.** A mescla só começa com o começo de B guardado
  (1 s, ou toda a entrada se for menor; `kTransitionReadyMs`). Sem B a tempo,
  A segue sem efeito e a região encolhe, sempre terminando no último quadro de
  A. Se não sobrar 1,5 s (`kTransitionMinOverlapMs`), a troca é normal, sem
  efeito: A inteira e depois B, com o motivo `next-not-prepared`. Nunca A com
  efeito sobre silêncio no lugar de B.
* **Registro da transição** (`TransitionTrace`): id do pedido, fase (armada,
  mesclando, concluída, abortada), motivo, durações pedidas e efetivas, quadro
  do marcador, B guardada no início, espera, quadros mesclados, quadros com B,
  quadros em que B faltou e energia de A e de B na mescla. Sem dados pessoais.
* A rampa de entrada de B tem 10 ms em qualquer taxa (`round(taxa × 0,010)`,
  antes 441 quadros fixos).
* O fim de A é sempre o próximo marcador do librespot empacotado. A estimativa
  só decide quando começar a ler. Sem marcador o buffer enche e não há efeito.

### Windows (TH-003, TH-004)

* `AudioEngine::armTransition` devolve o id do pedido. O aceite, a mescla, a
  conclusão ou o motivo do aborto chegam depois por `transitionTrace()`, sem
  esperar na thread da interface.
* "Última transição" só aparece depois de uma mescla concluída com B audível.
  Um aborto por falta de B mostra "Última troca sem efeito". Um pedido recusado
  pode ser tentado de novo.
* Cada ocorrência no PCM (sequência de marcadores) tem sua transição. A mesma
  música duas vezes seguidas também mescla. A duração exibida só é usada se a
  posição do Spotify bater com a contagem de PCM da ocorrência atual.

### Android (TH-004, TH-005, TH-007)

* `pullInto` escreve num `float[]` reutilizado. `push` lê direto do array do
  motor (região crítica JNI). Não há mais vetor nem array novo por bloco na
  saída da transição.
* O mixer é recriado quando a saída reabre em outra taxa (antes ficava na
  taxa antiga).
* A ocorrência vem da sequência de marcadores, e o status segue o trace, como
  no Windows.

### Testes da 0.17.4

* **Núcleo** (`TransitionChecks`), A em 440 Hz e B em 660 Hz:
  * Nos seis efeitos e três formatos, as duas soam na mesma mescla
    (Goertzel). O trace prova B em todos os quadros da entrada, com 0 faltando.
  * Com B atrasada 0; 0,1; 0,5; 1; 3; 4,8 e 7 s depois do começo planejado da
    mescla, nunca entra silêncio no lugar de B. Até 3 s a mescla encolhe; a
    partir de 4,8 s a troca é normal, A inteira e depois B.
  * Recuperação no meio: a faixa retomada continua sendo A e a mescla usa a B
    real.
  * 48 e 96 kHz.
  * Motivos e ids estáveis.
* **Motor Windows real** com produtor sintético: a mescla tem B pronta e 0
  quadros faltando.
* **Spotify real no Windows**:
  * Comando: `TonalSpotifyRecoveryTests --live-session --verify-natural-transitions`.
  * Aparelho de diagnóstico isolado, cópia da sessão salva, saída OpenAL nula
    (nada soa nas caixas), preferências só em memória.
  * Três transições naturais seguidas num álbum, sem reprodução prévia na conta:
    * Fade 6/6 s: mescla de 6 s, 2,5 s de B guardados, 0 quadros de B
      faltando, 0 falhas de áudio.
    * Eco 6/9 s: mescla de 9 s, 2,5 s de B, 0 faltando, 0 falhas.
    * Freio 9/4 s: mescla de 9 s, 2,5 s de B, 0 faltando, 0 falhas.
  * Isso mede o PCM entregue pelo motor real à saída, não o som num aparelho
    físico.
* **AVD Android** (`AudioSelfTest`): AudioTrack real, trace com A e B na
  mescla, retomada no meio e ocorrência por marcador. Resultados em
  docs/release-0174-tests.json.

### Limites

* O Android não tem login na AVD. A correção do Spirc foi conferida no código
  e o caminho Java/JNI na AVD, mas não com o Spotify real no Android. O
  primeiro teste real é o do usuário no celular.
* Sem medição acústica (loopback), Bluetooth, carro, CPU ou bateria.
* TH-006 (fila sem bloqueio entre o motor e a saída Android) e TH-009 (eventos
  de transporte tipados em vez das linhas do log do librespot) continuam como
  na 0.17.3. O produtor Android ainda espera quando o buffer da pré-rolagem
  enche, fora da thread de saída.


## 0.17.5 — fila SPSC e transporte tipado (22/11/2026)

TH-006: no Android, PCM e marcadores entram na mesma fila SPSC limitada de 256 entradas. O produtor pode aguardar espaço fora do monitor do mixer; cancelamento muda o epoch e acorda esperas. O writer reutilizado é o único que chama push/pull e não usa waits temporizados nem sleeps de polling. Depois que a primeira implementação apresentou um SIGSEGV na AVD por chamadas concorrentes ao AudioTrack, as operações nativas do dispositivo passaram a ser serializadas em uma exclusão separada, sem envolver o mixer.

TH-009: o librespot emite no PCM eventos UserLoad, UserSeek e UserStop. Cargas naturais e retomadas têm comandos próprios e não descartam a mescla. O Windows lê esses eventos pelo pipe da geração atual e não usa mais strings `command=Load`/`Seek` para decidir flush. O Android retém o PCM antigo até a fronteira efetiva e limpa o AudioTrack nela. Troca de rota cancela a transição. A compensação de volume do Windows é zerada ao encerrar/cancelar, para não vazar ganho antigo para outra mescla.

Na AVD, após a correção do AudioTrack: 0 underruns, seis esperas por fila cheia totalizando 101 ms, 0 fallbacks diretos, recuperação e barreira de transporte aprovadas. No Windows com Spotify real, fade 6/6 s, eco 6/9 s e freio 9/4 s concluíram 3/3 com A+B verificados, 0 quadros de B faltando e 0 underruns no OpenAL nulo. Esses números não são benchmark de aparelho nem captação acústica. O teste com Spotify real no Android, áudio acústico, Bluetooth e aparelho físico continua pendente.


## 0.17.6 — regressão do seek (23/11/2026)

A proteção `transportChanging` da 0.17.5 descartava PCM até receber um evento específico. Sem esse marcador numa retomada, a saída podia ficar muda e a posição parava, impedindo novas transições. A correção remove o bloqueio: o PCM continua e `transportDiscontinuity()` faz flush quando o evento tipado chega. Não há promessa de que áudio antigo seja preferível a continuidade; o evento real continua sendo a fronteira de descarte.

Na AVD, o teste cobre PCM com marcador atrasado, flush e nova mescla concluída após o seek. Resultado: 0 underruns e 0 fallbacks diretos; a transição adicional elevou os contadores acumulados para 47 esperas/1.024 ms, sem falha de saída. Uma primeira fixture com apenas 3 s de A abortou corretamente por `next-not-prepared`; foi substituída por uma janela com 6 s de A e 8 s de B, sem afrouxar a exigência de mescla concluída.

O teste de navegação com Spotify real não iniciou por falta de login na AVD após os testes anteriores. Sem teste físico, acústico ou validação Android real nesta correção.


## 0.17.7 — antecedência para carregar a próxima (23/11/2026)

O usuário relatou que as trocas naturais voltaram a não ter efeito. A janela de captura terminava cerca de três segundos antes do ponto da mescla; uma carga mais lenta de B chegava ao limite de 1,5 s e o mixer fazia a troca normal com `next-not-prepared`. As fixtures anteriores entregavam B quase imediatamente e não cobriam esse tempo de carga. Sem o trace do aparelho, essa rota de falha está confirmada no teste, mas não se afirma que foi a única causa possível no telefone.

A captura agora começa até 6 s mais cedo (`kTransitionLoadMarginMs` / `LOAD_MARGIN_MS`). Com 6/6 s, a janela passa de 9 para 15 s antes do fim de A; com 10/10 s, de 13 para 19 s. A pré-rolagem de B depois da mescla continua 2,5 s, a exigência de B real permanece e seeks/pulos/seleções continuam cancelando o efeito. Não há rede, alocação nem trabalho de UI novo no callback PCM.

Regressão: o planejamento antigo recusou a nova janela antes da correção. No teste com a mesma carga de B atrasada 8 s, a captura antiga de 9 s aborta e a de 15 s conclui com A+B, 0 quadros ausentes e a mesma pré-rolagem. AVD com AudioTrack real e PCM sintético: suíte completa aprovada, 0 underruns, 0 fallbacks diretos, 48 esperas/1.048 ms na fila, recuperação e transições consecutivas preservadas. Java host: 20.080 checks.

Windows: CTest 12/12 (81,55 s). Spotify real com saída OpenAL nula: primeira tentativa concluiu 3/3 mesclas com A+B e 0 quadros ausentes, mas foi reprovada por 1 underrun no eco; repetição concluiu 3/3 com 0 underruns. O episódio fica registrado, sem declarar benchmark ou teste acústico. Android com Spotify real e aparelho físico não foram testados.

## 0.17.8 — auditoria TH-006/TH-009 (23/11/2026)

Relatório completo: `docs/audit/TH006_TH009_AUDIT.md`. Resultados: `docs/audit/TH006_TH009_TEST_RESULTS.json`.

Uma suíte nova na AVD usa pacotes do tamanho real do librespot (1024 quadros) e entrega A, B e C sem pausa. Ela mostrou a causa provável do relato da 0.17.7, "não há efeito nenhum":
- **Leitura adiantada no tempo real:** desde a 0.17.5 o writer Android entregava ao mixer um pacote por bloco tocado. Quando o marcador de B chegava, A já tinha tocado até ele, e a região de mescla ficava com 0 quadros.
- **Segunda transição:** ao armar, a fila inteira era descartada (cerca de 6 s de A).
- **Atraso fora da conta:** a fila não entrava no atraso informado à tela, com 6 s guardados contra 0 ms informados.
- **Por que as fixtures antigas passavam:** usavam pacotes de 2048 quadros.

Correções:
- O writer entrega ao mixer tudo o que ele aceitar a cada ciclo.
- Armar não muda a geração.
- A fila tem 16 entradas e entra em `transitionLatencyMs()`.
- A geração é conferida dentro do `trackLock`.
- Uma falha do writer invalida a fila.
- Um único lock protege os produtores.
- Uma região vazia termina o trace com `next-not-prepared`.

No Windows, o evento tipado não esvazia mais o pipe, que já contém a posição nova. Antes eram perdidos 0,38 s por seek.

Nas duas plataformas, o fim natural da fila é `NaturalStop`, sem o evento de stop do usuário, e o fim já lido toca até o fim. O vigia do Windows trata `command=NaturalStop` como fim de reprodução esperada, sem reiniciar o motor.

Antes e depois:
- **AVD realista:**
  - Antes: transição sem efeito.
  - Depois: 2/2 mesclas de 4 s com 2,5 s de B guardados, 0 quadros perdidos, atraso informado igual ao real e 0 underruns durante as transições.
- **Motor Windows:** 115.473 → 132.300 de 132.300 quadros depois do seek.
- **Vigia:** reiniciava o motor; agora não reinicia.
- **CTest:** 12/12.
- **JVM:** 20.084 checagens, incluindo 1M itens e dois produtores.
- **Spotify real no Windows:** 3/3 com A+B e 0 underruns, com OpenAL nulo.

Pendente:
- Android com Spotify real e aparelho físico.
- Rotas e Bluetooth.
- CPU, GC e p99.
- Ciclos de AudioTrack e reabertura.
- Fim natural da fila com o motor real.
