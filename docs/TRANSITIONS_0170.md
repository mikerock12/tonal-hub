# Transições entre músicas, fila e letra — 0.17.0 / 0.17.1

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
