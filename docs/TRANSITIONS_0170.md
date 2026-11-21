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

