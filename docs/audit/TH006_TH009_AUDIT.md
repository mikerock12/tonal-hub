# Auditoria TH-006 / TH-009 — 23/11/2026

Plano seguido: `PLANO_AUDITORIA_E_CORRECAO_TH006_TH009_TONAL_HUB.md` (arquivo local do usuário,
fora do Git). Branch `codex/audit-th006-th009`, criada de `main` em
`ca9afbb11ec6f89312a1922404b00ec5b99ee4c2` (0.17.7, versionCode 48). O plano foi escrito sobre
`c01e9bc` (0.17.6). A 0.17.7 só antecipou a captura em 6 s (`kTransitionLoadMarginMs`) e não mexeu
na fila nem no transporte, então as hipóteses continuaram válidas e foram reavaliadas no HEAD real.

## 1. Resumo

1. **O contrato:**
   - TH-006 devia tirar do caminho de saída do Android as esperas temporizadas, com fila
     limitada, cancelamento por geração e medições.
   - TH-009 devia descartar áudio antigo por eventos tipados aplicados pelo player, sem decidir
     por texto de log.
2. **O que existia:** a 0.17.5 já tinha as estruturas pedidas, ou seja, a fila de 256 entradas,
   `mixEpoch`, `trackLock`, os eventos `UserLoad/UserSeek/UserStop`, `NaturalLoad/ResumeLoad` e a
   leitura de eventos no pipe do Windows.
3. **D1 (P0, Android):** o writer entregava ao mixer **um pacote por bloco tocado**. Com os pacotes
   reais do librespot (1024 quadros, Vorbis), a leitura adiantada ficava no tempo real. Quando o
   marcador de B chegava, A já tinha tocado até ele e a região de mescla ficava em 0 quadros:
   **troca sem efeito nenhum**, que é o relato do usuário na 0.17.7. As fixtures usavam pacotes de
   2048 quadros e mascaravam o defeito.
4. **D2 (P0, Android):** `armTransition` incrementava `mixEpoch`. A partir da segunda transição,
   com o motor entregando sem parar, a fila cheia (até 256 pacotes, cerca de 6 s) era descartada
   ao armar, e A pulava segundos.
5. **D3 (P1, Android):** o áudio guardado na fila não entrava em `transitionLatencyMs()`. Medido:
   6.004 ms na fila e 0 ms informados à tela, o que deslocava a posição, a letra e o planejamento
   da próxima transição.
6. **D4 (P1, Windows):** o evento tipado chamava `flushSpotify()`, que esvazia o pipe. Isso jogava
   fora o começo da nova posição, que já está atrás do evento. Medido: 16.827 quadros (0,38 s)
   perdidos por seek, e o marcador de começo da faixa seguinte podia ir junto.
7. **D5 (P1, ambos):** no fim natural da fila, o Spirc chama `player.stop()`, que emitia
   `UserStop`. O fim já lido da última música, e de uma transição armada, era descartado. Ao
   separar `NaturalStop`, o vigia do Windows reiniciaria o motor por "falta de PCM" se não fosse
   ajustado. O ajuste entrou e tem teste.
8. **Defeitos menores corrigidos:**
   - trace "armado" para sempre quando a região ficava vazia;
   - bloco antigo que podia chegar ao AudioTrack depois do flush;
   - ordem invertida no instante da drenagem;
   - fila sem proteção contra dois produtores;
   - writer que deixava o motor preso se morresse;
   - resto do bloco descartado numa pausa no meio do `write`.
9. **Testes antes/depois:**
   - Os testes adversariais novos falham no código antigo e passam no corrigido: AVD realista,
     mixer C++, motor Windows, watchdog e fila Java.
   - CTest 12/12 e Spotify real no Windows 3/3, com OpenAL nulo.
   - Android com Spotify real, aparelho físico, Bluetooth, acústica e perfis de CPU/GC: **não
     executados**.
10. **Status global:** **IMPLEMENTADO_COM_VALIDACAO_PENDENTE**. A correção foi comprovada no núcleo,
    na AVD e no Windows real (OpenAL nulo). O aceite ponta a ponta no telefone continua pendente.

## 2. Linha do tempo das mudanças

| Ponto | Commit | O que interessa aqui |
|---|---|---|
| 0.17.4 | `7b7b125` | Produtor empurrava o PCM direto no mixer, na velocidade do motor; writer não bloqueante |
| 0.17.5 | `f557285` / merge `5521d56` | Fila SPSC de 256, writer único, `trackLock`, eventos tipados, arm com `++mixEpoch` |
| 0.17.6 | `af2df8e` / merge `d6739e9` | Remove o descarte indefinido `transportChanging` |
| 0.17.7 | `84bf3e3` / merge `7422df0` | Captura 6 s mais cedo (não resolve D1: a vazão continuava no tempo real) |
| Auditoria | este branch | Correções D1–D5 e menores, testes adversariais |

## 3. Matriz

Legenda:
- `OK`: IMPLEMENTADO_E_COMPROVADO.
- `NV`: IMPLEMENTADO_NAO_VALIDADO.
- `PAR`: PARCIAL.
- `INC`: INCORRETO.
- `BLQ`: BLOQUEADO_POR_AMBIENTE.

As linhas são do código depois da correção.

### TH-006 (Android)

| ID | Status antes → depois | Evidência | Observação |
|---|---|---|---|
| T006-01 SPSC | PAR → OK | `TransitionInputQueue.java:31` (lock de produtores), `AudioOutput.java:297` (único consumidor) | Produtores reais: thread do sink (`onPcm`, `boundary`, `onAudioPause`). Uma reinicialização do motor pode sobrepor dois sinks por um instante. Com a fila antiga, o teste de dois produtores perdeu ou reordenou itens nas 3 de 3 rodadas; agora passa. F-01: 1M itens com marcadores e 4 gerações, sem perda. |
| T006-02 Contrapressão | PAR → OK | `AudioOutput.java:240` | Interrupção não gira e devolve o sinal (F-03). Cancelamento com fila cheia termina a espera em <500 ms (F-02). Recusa por cancelamento agora devolve `false` e o pacote sai pelo caminho direto, em vez de sumir. Se o writer morrer, a fila é invalidada e esvaziada (`:352`), e o motor não fica preso. |
| T006-03 Sem polling caro | INC → OK | `AudioOutput.java:297` | **D1.** O laço de entrega trata até 64 entradas por ciclo, enquanto o mixer aceitar. `Thread.yield` só sobra num caso transitório. `mixLock` não envolve `AudioTrack.write`. |
| T006-04 AudioTrack/locks | NV → NV | `AudioOutput.java:149`, grafo abaixo | O grafo de locks não tem ciclo. O SIGSEGV de `releaseBuffer` da 0.17.5 não foi reproduzido. Os 100–1.000 ciclos open/route/release não foram executados. |
| T006-05 Epoch | INC → OK | `AudioOutput.java:154` e `:213` | **D2:** arm não muda a geração. O epoch é conferido dentro do `trackLock`, e um bloco antigo não passa depois de `transportDiscontinuity()`. |
| T006-06 Alocação/desempenho | NV → BLQ | — | Continuam um `Vec<f32>` e um `float[]` JNI por pacote (Rust), e `Dsp.render` retorna um array novo. Não houve perfil de alocação, GC, CPU nem bateria (sem Perfetto nem aparelho físico). |
| T006-07 Rotas/formato | PAR → PAR | `AudioOutput.java:72` | A troca de rota continua cancelando a transição, e o áudio guardado no mixer (cerca de 2,5 s) é perdido nesse caso. Rotas e 44,1↔48 kHz não foram testados. |
| T006-08 Evidências | PAR → OK (AVD/JVM) | `AudioSelfTest.realisticTransitions`, `PlaybackSafetyChecks.transitionQueueAudit` | Testes que observam tempo, ordem e conteúdo, não strings. |

### TH-009

| ID | Status antes → depois | Evidência | Observação |
|---|---|---|---|
| T009-01 Origem dos eventos | PAR → OK | `player.rs:1719` (Stop), `:2321` (Seek), `:2395` (Load); `spirc.rs:1413/1854` | **D5.** O fim natural da fila agora é `NaturalStop`, sem evento: Android em `player.rs:126`, Windows em `prepare-librespot-windows.py`. Preload e NaturalLoad não emitem evento. |
| T009-02 Ordem PCM→marcador | OK | `NativePcmPipe.cpp:79`, `PcmPipeCases.hpp` | Marcador fragmentado, ordem e posição: testes existentes passam. |
| T009-03 Windows sem descarte por log | INC → OK | `AudioEngine.cpp:599` | **D4.** Só o pedido do Player esvazia o pipe; o evento tipado limpa a saída, o DSP e o mixer. Logs: `LibrespotProcess.cpp` usa `command=*` só para expectativa e pausa, nunca para flush (`grep flushSpotify`: só `Player::next`). |
| T009-04 Android seek | OK | `NativeBridge.java:104`, `AudioOutput.java:378` | Não há gate indefinido (0.17.6). Seek sem marcador e evento atrasado continuam na suíte AVD e passam. |
| T009-05 Natural ≠ manual | PAR → OK | idem T009-01; `TransitionMixer.cpp:552` | `ResumeLoad > 1 s` continua como a mesma ocorrência. Região vazia agora termina como `next-not-prepared`. |
| T009-06 Pausa/stop/externo | PAR → PAR | `AudioOutput.java:267` | Uma pausa no meio do write não perde mais o resto do bloco no writer. O caminho direto (sem transição) continua como antes. Connect remoto, rede, sessão e token não foram testados. |
| T009-07 Volume/DSP | OK (Windows) | `AudioEngine.cpp` (flush zera `volumeSegments`/`volumeCompensation`) | Android: `dsp.reset` no evento. Pop/clique não medido. |
| T009-08 Contrato comum | OK | tabela abaixo | — |

### Pistas AUD do plano

| ID | Resultado |
|---|---|
| AUD-01 | Confirmado como monitor só no produtor. A saída nunca espera `space`. Mantido, com lock de produtores. |
| AUD-02 | O laço não gira, mas o pacote sumia no cancelamento. **Corrigido** (`return accepted`). |
| AUD-03 | Fora de D1 o caminho é praticamente inalcançável; mantido. |
| AUD-04 | Não medido (BLQ). Cópia só em bloco parcial; o fallback copia só quando há offset. |
| AUD-05 | Sem ciclo de locks. Uma pausa no meio do write devolvia um write curto e perdia o resto: **corrigido**. |
| AUD-06 | **Corrigido**: epoch conferido dentro do `trackLock`. |
| AUD-07 | Códigos 1/2/3 vêm de `handle_command(Load)`, `handle_command_seek` e `handle_player_stop`. O fim natural da fila emitia 3: **D5**. |
| AUD-08 | Nenhum log chega a flush. O `NaturalStop` precisou entrar no vigia (`command=NaturalStop` não contém `command=Stop`). |
| AUD-09 | **D4**: o pipe era esvaziado também no evento tipado. |
| AUD-10 | Seek com marcador ausente ou atrasado continua sem silêncio (suíte AVD). Spotify real no Android: BLQ. |
| AUD-11 | As 47 esperas e 1.024 ms da 0.17.6 são a soma da contrapressão normal do produtor, não falhas. Agora o máximo individual é medido: cerca de 7,1 s numa única espera. É o motor parado enquanto o mixer não aceita mais nada, como já acontecia na 0.17.4 com `wait(50)`. |

### Contrato (T009-08), conforme o código

| Operação | Emissor | Descarta PCM antigo | Transição possível | Evento | Geração |
|---|---|---|---|---|---|
| Próxima natural | Spirc `EndOfTrack` → `load_natural` | Não | Sim | `TrackBoundary` | mesma |
| Retomada da mesma faixa | recuperação de buffer → `load_resume` | Não | Não (`resumeCapture`) | `TrackBoundary` com posição > 1 s | mesma |
| Seek aplicado | `handle_command_seek` | Sim, no evento | Não | `UserSeek` | `mixEpoch` +1 (cancel) |
| Pulo/seleção | `PlayerCommand::Load` | Sim, no evento | Não | `UserLoad` | `mixEpoch` +1 |
| Stop do usuário | `PlayerCommand::Stop` | Sim | Não | `UserStop` | `mixEpoch` +1 |
| Fim natural da fila | `NaturalStop` (novo) | Não; o fim lido toca | A transição armada drena | nenhum | mesma |
| Troca de rota (Android) | `refreshRoute` | Cancela a transição | Não | — | `mixEpoch` +1 |
| Reinício do motor | nova geração da sessão | Sim | Não | geração JNI/pipe nova | nova |

### Grafo de locks (Android)

- `this` → `trackLock`
- `this` → `mixLock`
- `trackLock` não chama nenhum outro lock.
- `mixLock` → `space`/`work`, só no `notify` dentro de `release()`.
- O produtor só usa `producer` e `space`/`work`.
- `output()` nunca roda com `mixLock` mantido.

Não há aresta de volta, portanto não há ciclo.

## 4. Raiz e interleaving dos defeitos principais

- **D1:**
  - **Interleaving:** a cada volta, o writer fazia `peek → push(1 pacote) → pull(1024) → write
    bloqueante (~23 ms)`.
  - **Efeito:** a entrada do mixer ficava em cerca de 1024 quadros por 1024 tocados, e a fila de
    256 pacotes enchia.
  - **Quando B chega:** o marcador de B só chegava ao mixer quando tudo o que estava na fila tinha
    tocado. Nessa hora `tailPlayed` já era igual ao `boundaryFrame`, então `placeRegion` dava
    `overlap = 0` e `pull()` fazia `finishMixing()` sem efeito e sem fechar o trace.
  - **Teste antes:** trace `[1,1,0,0,44100,176400,176400,0,0,0,396488,…]` (fase Armed, região 0).
- **D2:** depois de uma mescla, o mixer funciona como linha de atraso e a fila fica cheia. No
  segundo `armTransition`, o `++mixEpoch` fazia o writer descartar todas as entradas, que eram A
  contínua.
- **D3:** `transitionLatencyMs()` só lia `latencyFrames` do mixer.
- **D4:** `trackBoundary()` → `flushSpotify()` → `m_flush` → `pipe->discard()`. O descarte limpa
  `m_pending` e lê até 128 KiB do pipe, que nesse ponto já são a nova posição. **Teste antes:**
  115.473 de 132.300 quadros lidos.
- **D5:** `handle_next_kind(natural)` → sem próxima → `handle_stop()` → `player.stop()` →
  `UserStop` → Android `Transitions.cancel()` + `flush`, e Windows `output.clear()` + pausa pelo
  log `command=Stop`.

## 5. Alterações por arquivo

- `android/.../TransitionInputQueue.java`:
  - lock de produtores (o consumidor nunca o toma);
  - `tail`/`head` com escrita volátil;
  - a API continua a mesma.
- `android/.../AudioOutput.java`:
  - fila de 16 entradas e contador `queuedFrames` incluído no atraso;
  - arm sem novo epoch;
  - laço de entrega no writer;
  - `output(..., epoch)` com a geração conferida dentro do `trackLock`;
  - `writeOwned` retoma depois de pausa;
  - a fila continua sendo o caminho enquanto o writer tem pacotes, para manter a ordem;
  - falha do writer invalida e esvazia a fila;
  - métricas `transitionQueueMaxWaitMs`, `transitionQueuedFrames` e `transitionWriterFailures`.
- `src/playback/TransitionMixer.cpp`: região vazia fecha o trace com `NextNotPrepared` (Windows e
  Android).
- `src/audio/AudioEngine.cpp`: `eventFlush` local; `pipe->discard()` só em `commandFlush`.
- `src/LibrespotProcess.cpp`: `command=NaturalStop` encerra a expectativa de PCM sem pausar o motor
  de áudio.
- `android/native/vendor/librespot-{playback,connect}` e `packaging/prepare-librespot-windows.py`:
  - `PlayerCommand::NaturalStop` e `Player::stop_natural()`;
  - `handle_player_stop_quiet`;
  - `handle_stop_kind(natural)` no fim da fila.
- Testes:
  - `AudioSelfTest.realisticTransitions` (extra `transitionRealSuite`);
  - `PlaybackSafetyChecks.transitionQueueAudit`;
  - `TransitionChecks` (captura atrasada);
  - `AudioTests --seek-event` e `spotifySeekEventKeepsNewAudioTest`;
  - `RecoveryTests` modo `natural-end`;
  - verificações de atraso da suíte AVD antiga, que agora descontam a fila.

**Reverter:** cada item é independente. D1, D2 e D3 ficam em `AudioOutput.java` e
`TransitionInputQueue.java`; D4 em `AudioEngine.cpp`; D5 no vendor, no script e em
`LibrespotProcess.cpp`; o trace em `TransitionMixer.cpp`. Reverter D1 sem D2 mantém a perda de
áudio no segundo arm.

## 6. Resultados F-01..F-22

| ID | Ambiente | Antes | Depois | Nota |
|---|---|---|---|---|
| F-01 | JVM | n/d (novo) | PASS | 1M itens, marcador a cada 97, 4 gerações, produtor e consumidor com jitter |
| F-02 | JVM | n/d | PASS | Fila cheia + cancelamento: espera terminada em <500 ms |
| F-03 | JVM | n/d | PASS | Interrupção não aceita, não gira e preserva o sinal |
| F-04 | AVD | FAIL (D1) | PASS | Na suíte realista, writer sem região vazia |
| F-05 | — | NOT_RUN | NOT_RUN | AudioTrack com flush/release/rota concorrentes: sem roteiro automatizado nesta entrega |
| F-06 | — | NOT_RUN | NOT_RUN | `ERROR_DEAD_OBJECT` com transição ativa |
| F-07 | — | NOT_RUN | NOT_RUN | 44,1↔48 kHz e 100 reaberturas |
| F-08 | físico | BLQ | BLQ | 30 min com transições OFF/ON |
| F-09 | Windows | PASS | PASS | Marcador fragmentado (`pipeTransportEventTest`) |
| F-10 | Windows | PASS (código) | PASS | Nenhum log chega a flush; vigia com `natural-end` (antes FAIL, depois PASS) |
| F-11 | C++ / AVD / Windows real | PASS (C++) / FAIL (AVD realista) | PASS | 6 efeitos com 440/660 Hz; AVD 2/2 com A+B; Windows real 3/3 |
| F-12 | C++ / AVD | PASS | PASS | Retomada no meio = mesma ocorrência |
| F-13 | Windows | FAIL (D4) | PASS | Seek tipado: 0,38 s perdidos antes, 0 depois |
| F-14 | AVD | PASS | PASS | Seek sem marcador continua tocando (suíte antiga) |
| F-15 | AVD | PASS | PASS | Cancelar com a transição armada volta ao caminho direto (durante a mescla: não exercitado) |
| F-16 | código | NV | NV | Geração JNI/pipe ignora callbacks antigos; não exercitado com reinício real |
| F-17 | — | NOT_RUN | NOT_RUN | Pausa/retomada com transição: só revisão de código (`writeOwned`); o smoke de pausa exige Spotify logado na AVD |
| F-18 | AVD / Windows real | PASS | PASS | Ocorrência pela sequência de marcadores (`trackSequence`, suíte AVD e teste real) |
| F-19 | — | BLQ | BLQ | Troca de rota durante a mescla |
| F-20 | físico | BLQ | BLQ | Spotify real, fones/BT, gravação |
| F-21 | — | NOT_RUN | NOT_RUN | Stress randomizado de comandos |
| F-22 | Windows CTest | PASS | PASS | `audio_integration` com arquivos locais |

## 7. Números (AVD Tonal_Local_20261015, AudioTrack real, 44,1 kHz, PCM sintético)

Suíte realista: pacotes de 1024 quadros, três faixas de 18 s, duas transições de 4/4 s.

| Métrica | Antes (`ca9afbb`) | Depois |
|---|---|---|
| Transição 1 | fase Armed, região 0, sem efeito | concluída, 4.000 ms, B guardada 2.507 ms, 0 B faltando |
| Transição 2 | não chegou a armar (30 s presa na 1) | concluída, 4.000 ms, B 2.507 ms, 0 B faltando |
| Atraso no app × informado | 6.004 × 0 ms | 2.877 × 2.877 ms |
| Quadros perdidos/duplicados | — | 0 |
| Underruns durante as transições | — | 0 (1 no fim do fluxo, com o produtor parado) |
| Fila: máximo / maior espera do produtor | 256 / não medido | 16 / ~7,1 s (contrapressão) |
| Falhas do writer / fallbacks diretos | — | 0 / 0 |

Suíte antiga (`transitionSuite`, pacotes de 2048): PASS. Pré-rolagem de 2.484–2.507 ms sem a fila;
atraso total informado de 3.218–3.225 ms com a fila; 0 underruns; 0 fallbacks.

Windows (Spotify real, aparelho de diagnóstico, OpenAL nulo):

| Efeito | Saída/entrada | Região | B guardada | Falta B | Underruns |
|---|---|---|---|---|---|
| Fade | 6/6 s | 6.000 ms | 2.507 ms | 0 | 0 |
| Eco | 6/9 s | 9.000 ms | 2.504 ms | 0 | 0 |
| Freio de vinil | 9/4 s | 9.000 ms | 2.511 ms | 0 | 0 |

CPU, PSS, GC, alocações/s e p99 do writer: **NÃO MEDIDOS**.

## 8. Não executado e roteiro no aparelho

**Não executado:**
- Android com Spotify real: a AVD não tem login, e o telefone não estava conectado.
- Aparelho físico, Bluetooth/carro, captura acústica, CPU/bateria, Perfetto.
- F-05, F-06, F-07, F-19 e F-21.
- Fim natural da fila com o motor real nas duas plataformas: o caminho só foi compilado e
  verificado no vigia sintético.

**Roteiro com o telefone em depuração USB:**
1. Instalar o APK por cima.
2. Ligar transições 6/6 s e tocar um álbum.
3. Coletar `audio-snapshot.json` (debug) ou o status "Última transição" em três trocas seguidas.
4. Conferir a ausência de pulo de A na segunda troca.
5. Fazer um seek e um pulo durante a mescla e no fim do álbum.

## 9. Gates

- **Gate 1:**
  - PASS no Windows real (OpenAL nulo), na AVD e no C++.
  - PENDENTE no Android real.
- **Gate 2:**
  - PASS na JVM/AVD para fila, epoch e falha do writer.
  - PENDENTE nos ciclos de rota/AudioTrack (F-05..F-07).
- **Gate 3:** PENDENTE, sem medição de CPU, GC e p99.
- **Gate 4:**
  - CTest 12/12;
  - JVM 20.084 checagens;
  - AVD com as duas suítes;
  - Windows Spotify real 3/3, com OpenAL nulo;
  - físico, BT e acústica pendentes.

## 10. Reprodução

- **CTest:** `out/build/release` com `ctest --test-dir out/build/release --output-on-failure`.
- **Só o seek tipado:** `TonalAudioTests.exe --seek-event`.
- **Fila Java:** `javac`/`java` como em `.github/workflows/ci.yml`, passo PlaybackSafetyChecks.
- **AVD:** `assembleBenchmark`, depois
  `am start -n app.tonal.android.benchmark/app.tonal.android.MainActivity --ez transitionRealSuite true`.
  O resultado sai em `files/transition-real-checks.json` no armazenamento externo do app. A suíte
  antiga usa o extra `transitionSuite`.
- **Windows real:** `TonalSpotifyRecoveryTests --live-session --verify-natural-transitions`, com
  `ALSOFT_DRIVERS=null`. Ele pula se a conta já estiver tocando.
- **Motor Windows:** `prepare-librespot-windows.py` seguido de `cargo build` (ver
  `packaging/build-local.ps1`).
- **Android nativo:** `packaging/build-android-local.ps1 -NativeOnly` e
  `packaging/build-muse-android.ps1`.
