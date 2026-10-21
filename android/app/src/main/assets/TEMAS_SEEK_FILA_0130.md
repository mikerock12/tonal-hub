# Temas, seek fluido e fila sem travar — 0.13.0

## Temas

Seis temas, com os mesmos nomes no PC e no celular. Cada um muda cores de
fundo, cartões, texto, destaque e também a **barra de progresso e o marcador
(thumb)** dela: espessura, degradê do preenchimento e formato do marcador.

| Tema | Visual | Barra e marcador |
|---|---|---|
| Tonal Azul | Preto e azul, o visual original (padrão) | Branca, azul ao passar o mouse; círculo só no foco |
| Aurora | Verde-água sobre noite polar | Degradê verde→ciano; marcador em anel |
| Pôr do sol | Laranja e coral quentes | Degradê coral→dourado, barra mais grossa; marcador em pílula |
| Neon | Roxo profundo, rosa e ciano | Degradê rosa→ciano; círculo com halo |
| Oceano | Azul-marinho e ciano | Degradê ciano→azul; marcador em losango |
| Grafite suave | Cinza confortável com âmbar | Barra e marcador maiores, pensados para o toque |

**Onde escolher:** no PC, menu da conta (avatar) → **Temas**; no Android,
**Conta → Temas**. A troca vale na hora, sem reiniciar e sem interromper a
música. No celular, a tela é reconstruída no lugar: motor, fila aplicada e
descobertas aceitas continuam como estavam.

Os temas ficam num único catálogo, `resources/themes.json`, copiado para
`android/app/src/main/assets/themes.json`. Um teste confere se as duas cópias
são idênticas, se todo tema tem todas as cores e se o contraste de texto atinge
o nível AA (4,5:1; texto principal 7:1 sobre o painel). O tema Tonal Azul
mantém no Android as cores originais do celular.

Alcance: no PC, o Tonalify inteiro e o painel da música; Tonal Hub e IndieTonal
no PC mantêm o visual atual. No Android, Tonalify, Hub e IndieTonal usam o tema.
Os 11 cenários e estilos das letras continuam independentes do tema.
A escolha fica em `appearance/theme` (PC) e `files/theme.txt` (Android).

Desempenho: degradês e formatos são desenhados uma vez; o halo do Neon é um
retângulo translúcido, sem shader. Não há animação contínua nova. Não houve
nova coleta de CPU/RAM nesta entrega.

## Aplicar fila (Descoberta 2+1, Shake, Aleatório) sem travar

**Causa.** “Aplicar e reproduzir fila” recarregava a faixa que já estava
tocando. No Android, o comando `restore|posição|faixa|fila` fazia o librespot
parar, buscar e posicionar de novo a música atual, com buffer inicial de 2 s,
e o relógio da tela congelava até a reprodução voltar. No PC, a fila era
reenviada com a posição de até meio segundo antes do clique, e o ajuste do
librespot fazia o áudio voltar um trecho.

**Correção no Android.** Novo comando `requeue|…`. O librespot embarcado
recebeu a opção `keep_playing_track`: quando a faixa atual da nova fila é a
mesma que está tocando, só a ordem seguinte muda. Não há parada, nova busca,
seek ou buffer da faixa audível; se a próxima já tinha sido pré-carregada, a
nova próxima é pré-carregada no lugar. O relógio da tela segue sem congelar.
Se o motor tiver caído, a recuperação continua usando `restore` com a nova
ordem, como antes.

**Correção no PC.** O motor do PC é o librespot oficial controlado pela Web
API, que não permite trocar a fila sem reenviar a faixa atual. O Tonalify agora
envia a posição de **agora** somada à latência medida dos comandos (média
móvel, 80–1500 ms), para o ajuste cair onde o áudio já estará. O decoder é
reaproveitado e o buffer não é descartado. Pode restar um ajuste mínimo,
do tamanho do erro da estimativa de rede.

Contrato da Descoberta 2+1 inalterado: mesma prévia, cadência, limites e
preservação das descobertas aceitas.

## Seek fluido

**Causa no PC.** A barra da página da música enviava um pedido à Web API a
cada pixel arrastado, e o valor do player puxava o marcador de volta durante o
arraste. Cada seek também esvaziava o buffer antes de o Spotify aplicar o
comando: silêncio, trecho antigo e só então a nova posição. Um GET atrasado
podia devolver a posição anterior e fazer a barra pular para trás.

**Correção no PC.** A barra mostra só a posição arrastada e envia **um**
seek ao soltar (ou um por passo do teclado). Pedidos seguidos são
coalescidos (60 ms, vale o último). O buffer antigo só é descartado quando o
librespot executa o seek. Por 3 s, respostas remotas distantes mais de 1,5 s
da posição pedida são ignoradas.

**Causa no Android.** O motor faz o seek localmente, mas o relógio da tela só
era atualizado pelo próximo poll da Web API, que ainda podia trazer a posição
antiga: ao soltar, a barra voltava e depois saltava.

**Correção no Android.** O relógio adota a posição pedida no mesmo instante e
passa a seguir os eventos `Seeked`/`PositionChanged` do próprio motor. Polls
desatualizados são ignorados pelos mesmos 3 s/1,5 s. O tempo exibido acompanha
o dedo durante o arraste.

## Validação

| Verificação | Resultado |
|---|---|
| Catálogo de temas (`tests/themes/check_themes.py`) | 6 temas, 172 checagens: tokens, formatos, contraste AA, cópia Android idêntica |
| `ThemeCatalog` C++ (CTest `theme_catalog`) | 86 checagens: catálogo real, fallback de id e persistência |
| Seek e fila no PC (`spotify_recovery`) | Seek coalescido, GET atrasado ignorado, seek pendente não vaza para a próxima faixa, posição atual + latência ao reaplicar |
| CTest Windows / portátil | 10/10 e 6/6 |
| QML fila/temas (`tests/mix/run_ui.py`) | 12/12, incluindo seek só ao soltar, troca de tema na barra e seletor |
| QML letras e playlist | 6/6 e 3/3 |
| Política de reprodução Java (`PlaybackSafetyChecks`) | 21 verificações, incluindo `requeue` e recuperação |
| Rust Android (`cargo build`, ARM64 e x86_64) | Compilado com o librespot ajustado |
| Java Android (release, benchmark, comparison, automation) | Compilado |
| Temas no Android (`tests/android/themes_smoke.py`, AVD) | 6 temas trocados com áudio local tocando: PCM contínuo, 0 underruns novos, título preservado, sem exceção |
| Fila/Descoberta Android (`tests/mix/android_discovery_ui.py`) | Aprovado: 24 verificações JNI, descobertas preservadas, prévia sem aplicar |
| Inicialização do Tonal.exe real | 12 s estável, sem iniciar reprodução |

O teste novo do catálogo encontrou um defeito antes da entrega: o construtor
escrevia no id padrão antes de ele existir (ordem de inicialização). Corrigido.
Uma rodada do smoke Android foi interrompida por um ANR do app Mensagens da
própria AVD, e outra contou o fim natural do ensaio de 60 s; o roteiro passou
a reiniciar o ensaio e as rodadas finais foram aprovadas.

Registro detalhado: [temas-seek-fila-0130-tests.json](temas-seek-fila-0130-tests.json).

Limites desta validação: a troca de fila e o seek **não** foram medidos com
streaming Spotify real nesta sessão, porque isso tomaria a reprodução da conta
do usuário em outro aparelho sem autorização para isso. O comportamento com
áudio real deve ser confirmado no POCO X7 Pro e no PC. Não houve teste de
Bluetooth físico nem novo benchmark.

Base original: Matheus Nunes (natheus), [Tonal](https://github.com/natheus/tonal-player), MIT.
