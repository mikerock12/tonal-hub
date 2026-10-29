# Contexto do Tonal Hub

Atualizado em **29/10/2026**. Entrega: **Windows 0.15.4 / Android 0.15.4**, com pendências Android abaixo. Leia também [AGENTS.md](AGENTS.md).

## Android 0.15.6 — gota e play/pause em revisão

Após teste do usuário no celular com a 0.15.5, a gota foi redesenhada com a ponta na barra e o corpo arredondado pendendo abaixo, nos mesmos vermelhos do rastro. O controle principal de reprodução recebe aro de brasa animado limitado a 15 fps enquanto visível, com foco e animações do sistema habilitadas. Teste visual sintético e aceite no telefone são distintos. Android avança para versionCode 26; o APK precisa da assinatura original e de instalação por cima antes da publicação. Detalhes: [HALLOWEEN.md](docs/HALLOWEEN.md).

## Android 0.15.5 — correção da barra em preparação

Ver [HALLOWEEN.md](docs/HALLOWEEN.md): posição interpolada mais frequente na página da música, prioridade aos eventos recentes do motor e gota invertida abaixo da barra. O streaming real, a aparência em aparelho e a atualização assinada ainda precisam de validação. O código Android avança para versionCode 25; não substituir o APK instalado com outra chave.

## Halloween 0.15.4

Referência: [HALLOWEEN.md](docs/HALLOWEEN.md). Continuação local Windows do PR22,
base 4678641edfe64c6e2c0178b8c0988b924c8378b0. Novo tema compartilhado, teias,
controles em brasa e quatro cenários de letras. Pedido de 28/10 acrescenta
escorrimentos animados e gota com volume/reflexos. Desenho nativo limitado a
15 fps e apenas durante reprodução com a barra visível; não envia comandos
ao motor. Volume e prévias ficam estáticos. Não é simulação física de fluidos.
Android usa código 24 e mantém o certificado da 0.15.3, conferido nos dois APKs.
Código Windows: 8e96297de2a3268fb14109ef372835546970e605; Android compilado em
72d5c811ca270a3bec50f017c5e9cbe4d3bd4c3b, sem alteração da árvore Android no
commit seguinte. Manifestos: docs/artifacts-0154.json e docs/release-0154-tests.json.
Windows: QML 25/6/3 + Halloween 3, temas 213, inicialização dos três executáveis
empacotados e CRC dos 1.444 arquivos passaram. Houve timeouts intermitentes de
inicialização; manter tentativas e não atribuir causa sem prova.
Android: builds e política de reprodução 47 passaram; assinatura v2 e alinhamento
16 KiB conferidos. Emulador 37.2.12 falhou antes do ADB (launcher 0xC0000135;
engine isolado parou na checagem do hipervisor). Teste visual, instalação por
cima e POCO físico pendentes. Nenhuma outra VM foi encerrada. Apenas uma cópia
local de dependências SDK/CRT existentes foi montada em out/tools para diagnóstico;
nenhum aplicativo/ferramenta nova foi instalado ou baixado nesta tarefa.
CI, release, portal e backup finais registrados no PR22 e recibos privados.

## 0.15.3 — faixas puladas, volume, botões de mídia e Jam

Referência: [SKIP_VOLUME_JAM_0153.md](docs/SKIP_VOLUME_JAM_0153.md). Timeouts de
chave de áudio faziam o librespot pular a fila inteira; agora dois pulos com
timeout em 30 s reiniciam a sessão e retomam pela primeira faixa pulada, do
começo. Faixa realmente indisponível continua só pulada. Volume inicial do motor
nos primeiros 15 s após Ready (>10 pontos de diferença) é substituído uma vez
pelo do usuário; mudo persiste o volume anterior. Android registra
MediaButtonReceiver nas sessões (tecla com player fechado é ignorada, não acorda
o Spotify). Jam: cliente Android criado na abertura, eventos SessionUpdate do
dealer, avisos de entrada/saída/fila, compartilhamento do convite ao iniciar
(PC copia). Sem teste entre contas reais, sem fone/carro físico e sem
reprodução da falha de chaves com Spotify real.

## Leitor 0.15.2 — retomada após timeout (histórico)

O POCO X7 Pro físico do usuário atingiu o limite antes de entregar leituras na
0.15.1. O leitor v5/schema 3 agora prepara uma faixa pendente por ação e salva
o estado também durante preparação, geração e conferência. Continuação funciona
mesmo com zero leituras aceitas, usando o lote exato. Mesmo Qwen3.5 2B, sem novo
download de pesos; atualizar sem desinstalar. Checkpoint privado de até 64 MiB
mais temporário, fora do backup da biblioteca. Uma thread, 90/105 s, separado do
player e somente em pausa escolhida pelo usuário. ARM64 tem backend acelerado
com verificação HWCAP e alternativa genérica. Não prometer ganho físico medido.
Referência: [READER_RESUME_0152.md](docs/READER_RESUME_0152.md).

Retomada validada com pesos reais no desktop e na AVD; não houve teste físico
no POCO. Quatro propostas passaram no filtro do próprio modelo no desktop,
mas a leitura manual ainda encontrou nuances discutíveis. Isso não é acurácia
semântica de 4/4. Jam permanece experimental; alterações da home/fila/áudio
0.15.1 foram entregues e têm limites em
[HOME_QUEUE_AUDIO_0151.md](docs/HOME_QUEUE_AUDIO_0151.md).

## Entrega 0.15.0 — estatísticas e histórias (histórico)

Referência: [STATISTICS_STORIES_0150.md](docs/STATISTICS_STORIES_0150.md).
Atalhos Estatísticas/Backup no cabeçalho global dos players, retirados das abas
internas da música. Seu ritmo: 7/30/90/180/365 dias ou zero para Desde o começo.
SQLite conserva sessões sem expiração de 180 dias; cache recente limitado,
agregação assíncrona e exportação/restauração completas do arquivo histórico.
Rodapé mede arquivo local e último backup exportado, sem confundir com sync.

Qwen2.5 0.5B Q4_K_M (Apache 2.0), 468,6 MiB opcionais, com llama.cpp MIT nativo.
Não foi treinado pelo projeto. Até oito letras do período, cache privado limitado,
resumo dos trechos com fontes explícitas; pode errar e não avalia saúde/humor.
Inferência CPU de uma thread na geração e na preparação, processo separado Windows/Android :stories,
90 s de orçamento e supervisão de 105 s, cancelamento ao fechar. No Android,
a inferência aguarda pausa e é suspensa quando a reprodução retorna; a
preparação permanece em segundo plano. Pesos não
acompanham instaladores e não entram no backup JSON. Não há novo benchmark
Spotify nesta versão. Evidências em docs/statistics-stories-0150-tests.json.

## Projeto, autoria e objetivo

Projeto pessoal de Maicon Nunes, derivado do **Tonal de Matheus Nunes
(natheus)**, seu irmão: https://github.com/natheus/tonal-player.
Preservar créditos e MIT original em [CREDITS.md](CREDITS.md) e [LICENSE](LICENSE).
O trabalho busca um player leve, com navegação musical completa, efeitos de
áudio, letras/tradução, estatísticas locais e descoberta de músicas.

Repositório de trabalho: https://github.com/mikerock12/tonal-hub, com publicação
pública autorizada pelo usuário em 19/10/2026. Branch principal `main`. O remoto
chamado `private` continua sendo o destino autorizado de push, mesmo após a
mudança de visibilidade. O backup e os dados pessoais permanecem privados.
O remoto `origin` aponta ao projeto do irmão: **não publicar alterações nele**.
Branches de trabalho usam `codex/`. Conferir `git status` antes de agir.

## Aplicativos e arquitetura real

- **Tonal Hub:** central leve; abre/alternar players em processos próprios no
  Windows, sem inicializar catálogo ou motor de áudio no hub.
- **Tonalify:** Spotify Web API para catálogo/conta e librespot para reprodução.
  Streaming depende de sessão adequada/Premium. Motor não é o Spotify oficial.
- **IndieTonal:** fontes independentes. Audius e acervos públicos Archive/LMA,
  incluindo a coleção Aadam Jacobs; MusicBrainz fornece metadados, não áudio.
  Jamendo exige credencial ativa; OpenSubsonic exige servidor/conta do usuário.
  Integrações autenticadas sem validação devem continuar identificadas como tal.
- Windows: C++20, Qt 6.8.3/QML; FFmpeg/OpenAL Soft no áudio avançado.
- Android: interface e integração Android nativas Java, núcleo C++ por JNI,
  Rust/librespot já existente e AudioTrack na saída. Não é um app inteiramente
  C++, nem usa React, Electron, Chromium ou WebView para esses recursos.
- Text to Tonalify porta as regras de PlaylistSpotify para C++ compartilhado:
  https://github.com/mikerock12/PlaylistSpotify. Reutiliza OAuth existente.

## Recursos implementados e uso

| Área | Comportamento |
|---|---|
| Catálogo Tonalify | Busca de músicas/artistas/álbuns/playlists; páginas de artista/álbum; biblioteca e buscas recentes |
| Reprodução | Player, fila, prévia, favoritos com coração preenchido azul, Shake, Aleatório e Descoberta 2+1 |
| Text to Tonalify | Colar texto, revisar versões e ordem, editar nome/descrição/capa; salvar local antes da cópia opcional privada/pública no Spotify |
| Biblioteca pessoal | SQLite por aparelho/player, favoritos, playlists, histórico, preferências, parâmetros de apresentação e dados estatísticos |
| Estatísticas | Tempo realmente ouvido, escutas qualificadas, faixas distintas, músicas/álbuns/artistas/estilos/dias; barras e detalhes clicáveis; resumos nas listas |
| Repertório | TonalMuse Tiny local, temas de letras e regras editoriais de gêneros, cobertura e abstenção; sem diagnóstico pessoal |
| Letras | LRCLIB/importação TXT/LRC; acompanhamento opcional, alinhamento manual local, exportar ajuste, tradução pt-BR com idioma automático |
| Apresentação | 11 cenários; fontes/cores/tamanhos independentes da letra/tradução, contorno, brilho, neon, piscante, negrito, itálico e sublinhado |
| Áudio | EQ 10 bandas, presets, graves, compressor, limiter, normalização, HRTF; IAMF somente com fonte compatível; perfis de saída no Android |
| Backup | Exportar/importar biblioteca pessoal; backup consistente SQLite/DSP/preferências no PC; pacote privado no Drive sem credenciais |

Na tela cheia de letras: primeiro toque com controles ocultos apenas os
revela. Com os controles visíveis, tocar numa linha alinha a letra e os oculta.
Rolagem não deve sincronizar. Controles se recolhem automaticamente.
Traduzir envia o texto ao serviço Google Tradutor; só a análise temática é
inteiramente local. Falhas temporárias do LRCLIB têm tentativas limitadas.

TonalMuse Tiny v1: classificador linear multilabel C++, pesos int8 de
32.832 bytes, sem runtime de ML/LLM. Treinado com corpus sintético original
PT/EN/ES; sem precisão estabelecida para letras reais. Sugestões editoriais
não são fatos gerados pelo modelo. Ver [documentação](docs/TONAL_MUSE_070.md).

## Descoberta 2+1: contrato atual

[Especificação completa, fórmula e limites](docs/DESCOBERTA_2_MAIS_1.md).

1. Prepara uma fila temporária: duas originais e uma sugestão externa. Não
   edita a playlist salva e não consulta outra música a cada terceiro play.
2. Busca até seis consultas de gênero, com ano de referência ±8 e sem ano;
   até duas páginas de dez por consulta, até 120 candidatos. Prazo Windows
   15 s; Android 6,5 s. Metadados/gêneros podem ser insuficientes.
3. Exclui faixas e nomes normalizados de artistas da seleção, álbum de origem
   em modo álbum, entradas indisponíveis/locais e filtros textuais karaoke/tribute.
   Sugestões não repetem URI nem nome normalizado de artista na mesma fila.
4. Melhor afinidade contra até 12 originais a partir da escolhida, com retorno
   ao início; não contra especificamente as duas anteriores. Gênero comum:
   `3 + interseção/max(tamanhos)`; ano: `max(0,1-diferença/20)`; tema comum: +0,2.
   Rejeita afinidade <1,1. Temas só contam se já analisados localmente, escore ≥0,35.
5. `peso = afinidade × (artistaInédito?8:1) × (faixaInédita?2:1)`.
   Ordena `log(U)/peso` em ordem decrescente, U estritamente entre zero e um.
   Novos artista/faixa têm multiplicador 16; artista conhecido/faixa nova, 2;
   ambos conhecidos, 1. Não são garantias nem probabilidades finais fixas.
6. Histórico é local, eventos com tempo >0, inclusive parcial; não é o histórico
   Spotify completo nem se sincroniza automaticamente entre aparelhos.
7. Semente renovada a cada preparação, repetições possíveis; candidatos vêm de
   páginas limitadas de busca, sem sorteio uniforme de todo o catálogo. Não há
   fator de popularidade, BPM, timbre, tonalidade ou leitura de toda letra nova.
8. Fila final até 500: reserva espaço para originais, portanto 500 originais
   podem impedir qualquer extra. Falta de candidatos permite seguir só originais.
9. Shake é Fisher–Yates visual; Aleatório usa prioridades independentes para
   reprodução. A música explicitamente escolhida permanece primeira.

Núcleo: `src/playback/ListeningMixCore.cpp`. Catálogo/integração:
`src/playback/ListeningMix.cpp` e `android/.../ListeningMix.java`.

## Correções e validação 0.11.1

No PC, a prévia espera a fila atual; consulta parcial Connect pode recuperar
álbum/playlist exatos; mantém artista/álbum/ano; explica seleção insuficiente,
falta de estilos, erro e timeout. Refresh de metadados da mesma música não
cancela a fila; uma troca real de faixa cancela a prévia antiga.

No Android, intenção explícita de tocar governa PCM e recuperação. Pausa/stop
cancelam retomadas pendentes, callbacks tardios não devem sobrepor pausa e
preparações em segundo plano não iniciam uma seleção. Reprodução já iniciada
continua bloqueada/em segundo plano. Retomada externa após pausa com aparelho
bloqueado fica desabilitada por padrão; opção em Áudio permite Bluetooth/tela
bloqueada. Recuperação limitada, incluindo janela de dois minutos.

Validação registrada em [discovery-playback-0111-tests.json](docs/discovery-playback-0111-tests.json):
10 CTest Windows; QML oito fila/seis letras/três playlist; 16 verificações
Android no host e 21 cenários dirigidos no emulador. O primeiro teste amplo
teve timeout de persistência; não declarar toda a suíte ampla aprovada.
Instalador/upgrade e APKs validados. Sem aparelho físico/car Bluetooth conectado.

Commits de produto concluídos: PRs [7](https://github.com/mikerock12/tonal-hub/pull/7)
e [8](https://github.com/mikerock12/tonal-hub/pull/8), ambos merged com CI verde.
Merge de produto: `54babcf4adcda7ceb8a25dd9648527cdbe1f13fd`. Novos commits
documentais devem ser consultados no Git; não substituir a evidência histórica
dos binários pelo HEAD documental.

## Benchmarks: o que já foi medido

[Índice consolidado](docs/BENCHMARKS.md) e [relatório completo 0.8](docs/BENCHMARK_080.md).

| Execução | Resultado e escopo |
|---|---|
| Android 0.1, 15/10 | Sem login: 16,5 vs 158,9 MiB PSS em primeiro plano; sem comparação autenticada |
| Android 0.3 | Streaming Tonalify: Flat 67,1 MiB/3,97%; EQ+graves 69,1/4,47%; +HRTF 74,5/6,05%; sem equivalente Spotify |
| Android 0.7 | Áudio local 48,4 MiB/4,1%; com apresentação 55,6/15,0%; janelas exploratórias de oito segundos |
| Windows 0.8, 17/10 | Mesma faixa tocando: Tonalify 122,55 MiB USS/0,50%; Spotify 540,53 MiB/2,87%; RAM 77,33% menor nessa amostra |
| Android 0.8, 17/10 | Sem login: 16,99 vs 154,78 MiB PSS; streaming Tonalify separado 72,59 MiB/5,23%; sem streaming Spotify equivalente |
| Windows 0.12, 19–20/10 | Minimizado, mesma faixa: 110,30 vs 496,83 MiB USS; CPU 0,77% vs 0,67%; 77,80% menos RAM nessa amostra, sem vantagem consistente de CPU |
| Android 0.12, 19–20/10 | Sem login, com Hub: 31,49 vs 156,84 MiB PSS na tela inicial; CPU 0,11% vs 1,20%; sem streaming oficial autenticado equivalente |
| Android 0.12, suplemento | Streaming Tonalify de teste sem Hub: 72,73 MiB PSS/4,69%, zero novos underruns; variante depurável, sem equivalente Spotify autenticado |

CPU 100% corresponde a um núcleo. USS e PSS não são intercambiáveis.
Coleta 0.12 em [docs/BENCHMARK_0120.md](docs/BENCHMARK_0120.md), com CSV/JSON e gráficos.
Sem conclusão de vantagem em streaming Android,
bateria, fidelidade superior ou latência/estabilidade Bluetooth física.

## Artefatos e restauração

- Windows: `out/release-local/Tonal-Hub-0.11.1.0-Windows-x64-Setup.exe`.
- POCO X7 Pro ARM64: `out/android/Tonal-Android-0.11.1-arm64-v8a.apk`.
- Universal ARM64+x86_64: `out/android/Tonal-Android-0.11.1-local.apk`.
- Versão Android 0.11.1-local, versionCode 13. Atualizar por cima preserva dados;
  desinstalar pode removê-los. Exportar backup antes. Backup não é sincronização.
- Drive privado: https://drive.google.com/drive/folders/1sNjL35fdHzoLY8QcqtBX2XF4YQJbeKlL.
  Contém instalador/APKs, fontes, histórico Git, fontes de áudio correspondentes,
  dados pessoais disponíveis do PC, evidências e guia de restauração.
- Credenciais, Vault, senhas, OTP, keystores, cache de SDK e mídia externa ficam
  fora. Banco do telefone físico precisa de exportação no próprio telefone.
- Windows SQLite/DSP: restaurar conforme manifesto sob `%LOCALAPPDATA%/Tonal`.
  `preferences.json` documenta valores do Registro; copiar o JSON não os aplica.
- Exportação/importação da biblioteca pelo app é o caminho para troca de aparelho.

## Continuidade e cuidados de implementação

Não bloquear callback de áudio com rede, metadados, inferência, I/O de banco ou
animações. Não mudar volume do carro/global para compensar diferenças. DSP não
controla o codec Bluetooth; Android e receptor o negociam. Buffer/cache temporário
não equivale a downloads offline garantidos. Testar recuperação sem ignorar pausa.

Manter PC e Android coerentes em recursos; atualizar os instaláveis quando houver
mudança de produto. Documentação não exige rebuild. Para entregas autorizadas,
concluir commit/push no fork, PR/CI/merge e backup verificado no Drive. Nunca
declarar CI verde, upload, teste físico ou benchmark sem evidência do resultado.

Guia didático com capturas e origem/versão: [GUIA_VISUAL.md](docs/GUIA_VISUAL.md).
Capturas históricas/testes não devem ser apresentadas como a sessão atual.

Incidente Codex, distinto do app: capacidade do modelo reportada em
https://github.com/openai/codex/issues/54283#issuecomment-6257960326.
Não reenviar nem atribuir automaticamente à cota do usuário.

## Publicação e perfil

Em 19/10/2026, o usuário autorizou tornar este fork público e adicioná-lo aos
projetos em destaque do perfil mikerock12. A revisão dos 39 commits/1.060 blobs
locais anteriores à mudança encontrou apenas seis falsos positivos de teste/código,
sem credenciais confirmadas. Isso é uma revisão heurística com inspeção manual,
não uma garantia absoluta de ausência de segredos. Novos commits devem manter
a separação entre fontes públicas e backup pessoal privado.
O perfil tem um README no repositório mikerock12/mikerock12 com a seção
Projetos em destaque; preservar os projetos existentes ao acrescentar o Tonal Hub.

## Entrega 0.12.0 — Descoberta independente e letras

As descobertas aceitas pertencem à fila; Shake/Aleatório as preservam sem nova
consulta de catálogo. Desligar Descoberta remove as próximas externas na prévia;
Aplicar confirma. A faixa atual permanece. O estado acompanha a fila ao reabrir.
Cache efêmero por seleção permite reaplicar os modos de uma mesma coleção.

DiscoveryLyrics busca letras LRCLIB em segundo plano (12 referências/120 candidatas),
um pedido por vez, intervalo 1,25 s, prazo 6 s, resposta 256 KiB, cache de até
2.000 escores com expiração. Não guarda as letras nem cria escutas; não altera
a fila confirmada. Opt-out na Fila, cancelamento e espera de serviço preservada
entre seleções. TonalMuse usa até 12 mil caracteres. Sem extração acústica
de BPM/timbre/tonalidade. A entrega de produto não teve nova coleta CPU/RAM;
o benchmark posterior de 19–20/10 está documentado acima. Detalhes e testes:
[DISCOVERY_ENRICHMENT_0120.md](docs/DISCOVERY_ENRICHMENT_0120.md).

Validação 0.12.0: 10 CTest Windows, 5 portáteis, QML 9/6/3, Android 21 de serviço
e 24 JNI/cache; teste da fila confirmou Shake/Aleatório/reabertura/desativação.
Upgrade Windows preservou dados e APKs mantêm identidade/assinatura (código 14).
Tentativas interrompidas, limitações e hashes constam no relatório JSON.
Backup privado 0.12: https://drive.google.com/drive/folders/15bxn_XcZianWzzp9E5GdYI8deVqm1fy5.
Pasta privada da atualização de benchmark:
https://drive.google.com/drive/folders/1e3ii--ACPMW4hH97EGIEVxf4pbdc6bF_.

## Entrega 0.13.0 — Temas, seek e fila sem recarregar

Seis temas (Tonal Azul, Aurora, Pôr do sol, Neon, Oceano, Grafite suave) com
os mesmos nomes no PC e no Android, incluindo barra de progresso e marcador.
Catálogo único `resources/themes.json`, copiado para os assets Android e
verificado por `tests/themes/check_themes.py` (cópia idêntica, tokens,
contraste AA). PC: `ThemeCatalog` (C++) exposto como `App.theme`; `Theme.qml`
lê o tema ativo; escolha em `appearance/theme`. Android: `AppTheme` aplica as
cores em `Ui` e reconstrói as views no lugar, sem recriar a Activity nem tocar
no motor/fila; escolha em `files/theme.txt`. Tonal Hub e IndieTonal no PC
mantêm o visual atual; cenários de letras são independentes.

Aplicar fila: o Android usa `requeue|…`; o librespot embarcado ganhou
`keep_playing_track`, que troca só a ordem seguinte quando a faixa atual
continua a mesma. Recuperação continua usando `restore`. O PC (librespot
oficial via Web API) reenvia a faixa com a posição atual mais a latência medida.
Seek no PC: um pedido ao soltar, coalescido em 60 ms, sem flush antecipado;
respostas remotas a mais de 1,5 s da posição pedida são ignoradas por 3 s.
No Android, relógio ancorado no pedido e nos eventos `Seeked/PositionChanged`.

Validação 0.13.0 em [temas-seek-fila-0130-tests.json](docs/temas-seek-fila-0130-tests.json)
e [TEMAS_SEEK_FILA_0130.md](docs/TEMAS_SEEK_FILA_0130.md). Sem streaming real
nem aparelho físico nesta sessão; sem novo benchmark.
Instaláveis: `Tonal-Hub-0.13.0.0-Windows-x64-Setup.exe`,
`Tonal-Android-0.13.0-arm64-v8a.apk` e `-local.apk` (versionCode 15).
Backup privado 0.13: https://drive.google.com/drive/folders/1BR5bH6HQxCGUKrB0YAAMXTzp0ZQ0JBiI.

## Entrega 0.13.1 — Repetição e modos da fila

PC: Shake/Aleatório/Descoberta junto da barra (inferior e página da música)
aplicam direto pela `ListeningMix::applyModes`, a partir da faixa que toca; a
janela Fila mantém prévia + Aplicar. O Player guarda a fila do mix
(`setOrderedQueue`) para o painel Fila e a encerra numa seleção comum
(`selectionReplaced`). Na coleção que toca, os modos também valem na hora.
Repetição: o librespot a zera a cada carga; o Player guarda a escolha e a
reaplica (`reapplyRepeat`), ignora GET antigo por 3 s e reverte com aviso se
o Spotify recusar. Pular desliga só a repetição de uma faixa.
Android: `RepeatMode` + `NativeBridge` reaplicam `repeat:*`/`repeat-track:*`
após cada seleção ou recuperação; botão Repetir na página da música, salvo em
`settings/repeatMode`. Detalhes: [REPETICAO_MODOS_0131.md](docs/REPETICAO_MODOS_0131.md).
Sem execução com Spotify real nesta sessão (permissão negada ao teste ao vivo).
Instaláveis 0.13.1 (versionCode 16) e backup privado:
https://drive.google.com/drive/folders/12L0IbCaTwURRIcOoHaQMLcgVYnP3nMcV.

## Entrega 0.13.2 — Notificação com capa e ícone no tema

PlaybackService publica na MediaSession título, artista, álbum, duração e capa
da música atual (MusicStore.current), com `Artwork.fetch` fora da thread
principal e o mesmo cache de capas; posição real e `onSeekTo` (Spotify).
IndiePlaybackService ganhou a capa. Ícone: activity-alias `HubIcon<Tema>` por
tema (só o escolhido ativo; `AppTheme.applyLauncherIcon`, DONT_KILL_APP),
desenhos gerados por `packaging/generate-theme-icons.py`; notificação colorida
com o destaque; no PC, `App::themedIcon` define o ícone da janela/barra.
Detalhes: [NOTIFICACAO_ICONE_0132.md](docs/NOTIFICACAO_ICONE_0132.md).
Instaláveis 0.13.2 (versionCode 17) e backup privado:
https://drive.google.com/drive/folders/1nW93Tq55vdBJcCv2pGtYjJNUgHvrSl99.

## Entrega 0.13.3 — Tema sem fechar o app

Tonalify e IndieTonal têm `taskAffinity` próprio e são abertos pelo Hub com
`NEW_TASK`; desativar o alias de ícone só recria a tarefa do Hub. Voltar no
Tonalify abre o Hub. O ícone é ajustado só ao escolher o tema, nunca ao abrir
o Hub. Teste: `tests/android/theme_keep_open_smoke.py` (reproduziu na 0.13.2).
Instaláveis 0.13.3 (versionCode 18) e backup privado:
https://drive.google.com/drive/folders/1QgE4Ja6iKKczifxhrirB-Dx9AJ3fTTqZ.

## Entrega 0.14.0 — Música, letra, artista e compartilhar

Página com três seções ajustadas à área disponível e navegação vertical com
encaixe; rolagem interna da letra preservada. Compartilhamento congela a faixa
escolhida, usa link Spotify canônico e cartão 600×1066, cores e três estilos.
Android abre seletor nativo, provider privado/read-only para cache PNG;
Windows copia link e salva PNG. Sem envio automático ou acesso a contatos.

Artista: fatos estruturados MusicBrainz, identificação única ou pelos créditos
do álbum; abstenção para homônimos. Sugestões editoriais por gênero/artista,
identificadas na tela. Consulta sob demanda, cache limitado e throttle comum
MusicBrainz. Não há LLM, inferência de biografia nem trabalho no callback PCM.
Setlist.fm Android usa chave própria criptografada, até cinco páginas de shows,
seleção/reprises/covers e revisão no Text to Tonalify (C++ compartilhado) antes
de salvar local e optar pela cópia Spotify. Windows mantém seu assistente.

Detalhes: [SONG_PAGES_SHARE_SETLISTS_0140.md](docs/SONG_PAGES_SHARE_SETLISTS_0140.md).
Testes e limites: `docs/song-pages-0140-tests.json`. Sem novo benchmark;
sem teste físico. Automação Windows recebeu GetCursorPos/acesso negado;
conferência por componentes Qt renderizados e AVD dedicada, sem chamar isso
validação manual da janela nativa. APKs versionCode 19, mesma identidade.

Validação 0.14: CTest Windows 10; QML 17/6/3; temas 183; política Java 40;
Android 18 verificações nativas e 10 cenários de interface na AVD. MusicBrainz
consultado ao vivo no C++ para Veruca Salt e Nirvana. Backups privados desta
entrega: https://drive.google.com/drive/folders/1iztlSwSWiDcCk5A6Wd1XIBHlJRGzEae4.

Instaláveis finais 0.14.0 verificados: Windows Setup com os três programas
abertos em smoke offscreen usando o runtime do pacote; APKs de produção com
mesma assinatura 0.13.3, versionCode 19, bibliotecas de áudio nativas idênticas.
Upgrade e abertura do APK universal conferidos na AVD. Código dos binários:
`8e2ce30d432b45dc6cb308b56906cddafc18e3bd`; atualizações documentais posteriores
não representam nova compilação. A instalação interativa Windows não foi testada.


## Correção Windows 0.14.1 — Volume

Volume: throttle de 150 ms enquanto arrasta, um PUT por rota em voo e apenas
o último valor pendente. Polls anteriores à intenção/ACK não alteram o volume;
guarda adicional de 3 s após confirmação. Trocar dispositivo, sair da sessão
ou entrar em arquivo local cancela a rota anterior. Retry obsoleto recebe
cancelamento local 499 opt-in e libera o slot; demais chamadas mantêm contrato.
Falha atual é mostrada e libera reconciliação. Persistência agrupada, fora do
callback de áudio. Master do Spotify continua uma única vez no librespot.
Roda: 2 pontos/120 unidades; deltas pequenos acumulados, horizontal/zero ignorados.
Extremos 0/100 e mudo/restaurar preservados. Percentual na barra do PC.
Ver docs/VOLUME_WINDOWS_0141.md e docs/volume-windows-0141-tests.json.
Android sem alteração/rebuild; permanecem os APKs 0.14.0 e suas evidências.

Validação focada: 10 cenários C++ de volume e QML 20/20; três runtimes do
pacote aprovados em offscreen. Primeira suíte CTest 7/10; repetição dos três
falhos aprovou UI, mas recuperação/integração de áudio continuaram com falha
ou timeout enquanto outra AVD/Java rodavam. Causa não estabelecida; não
chamar a suíte ampla de aprovada nem desativar/afrouxar testes. Instalador
0.14.1 gerado; instalação interativa/medição acústica não realizadas.
Backup privado: https://drive.google.com/drive/folders/1X5iQCZHdaGoflbjW2B9H4h_XEAtcSqqb.


Validação complementar 0.14.1 em 24/10: nova suíte CTest completa **10/10**,
zero falhas, 58,04 s, incluindo recuperação de processos, integração e UI de
áudio. A AVD cuja parada foi autorizada já não estava ativa; outra AVD e os
processos Java foram preservados. Sem mudanças de produto ou limites de teste
entre as tentativas; não atribuir causa definitiva às falhas anteriores.
As duas primeiras execuções permanecem no histórico do relatório JSON.
Binários mantêm código `d4c812d9cda8b66a1f40ccecd0c46aaddb3f5192` e hash
original; este complemento documental não recompila EXE/APK nem é benchmark.

## Ambiente Git local em 24/10/2026

A instalação do Git do sistema estava sem o helper HTTPS. Para concluir a
entrega, foi usada uma cópia portátil oficial, sem mudar a instalação global:
`out/tools/git-repair/mingit/cmd/git.exe` (MinGit 2.56.0.3, Git for Windows).
ZIP oficial verificado por SHA-256
`2e5619966e588fde5f8122c5557cad50c0fbafcaed736689f40ec1dfb1d78ea7`.
Operações de rede usam temporariamente o helper do GitHub CLI já autenticado;
nenhuma credencial foi exportada ou salva no workspace. Push continua só para
`private`. Essa ferramenta local não faz parte do instalador do player.


## Ferramentas adicionais — preferência de relatório (25/10/2026)

Informar no relatório final quais aplicativos/ferramentas extras foram instalados
ou baixados no PC e o motivo. Inclui ferramentas portáteis. O Nightly Private
Browsing foi instalado por outro projeto para testes de navegador; sua permanência
foi aceita pelo usuário. Nesta entrega do Tonal, o MinGit portátil foi necessário
porque a instalação Git do sistema não tinha o helper HTTPS. Não mudou o Git global.


## Leitor 0.15.1 em desenvolvimento (25/10/2026)

O usuário rejeitou o resumo vazio da 0.15.0 e autorizou explicitamente priorizar
a qualidade com um modelo opcional de cerca de 1,2 GiB, carregado só durante
pausa e descarregado ao terminar. Candidato: Qwen3.5 2B Q4_K_M, quantização
Unsloth, Apache 2.0, runtime C++ llama.cpp existente. Não houve treino pelo Tonal.
Leituras por faixa, síntese, guardas de formato e cache v4 estão em validação.
Não confundir compilação ou quantidade de letras com qualidade semântica.
A nova solicitação de Jam Spotify com aprovação de participantes e avisos leves
de fila faz parte da sequência de trabalho. A API pública não cobre sessões;
a investigação do protocolo nativo está em docs/JAM_RESEARCH.md.

Jam: o usuário dispensou aprovação se indisponível. Priorizar avisos de entrada
e das músicas adicionadas/alteradas, com pessoa identificada só se houver dado
real. Sala de espera própria/servidor extra não são necessários para esse escopo.

## Validação local 0.15.1 — 25/10/2026

Atualizar letra incluído nos controles de tela cheia Windows/Android, com busca
explícita que ignora o cache. Contraste corrigido no painel Windows. Início usa
álbuns realmente ouvidos; página da música sem Agora/Biblioteca/Letras/X no topo,
casa para o início e Voltar Android conserva a rota anterior. Próxima/última na
fila preservam pausa e ocorrências. EQ ativa ao mover bandas; HRTF para caixas
é opção experimental explícita, sem crosstalk/correção de sala.

Jam experimental implementado em Rust compartilhado + C++/QML e Java. HTTP fora
do loop de comandos Spirc; transferências que retêm a faixa conservam decoder e
pausa. Cache de fila alinha ocorrências, conserva marcadores de Descoberta e
cauda conhecida; não atribui autoria sem dado. Sem teste com amigos reais.

Leitor v4 rejeita a frase genérica da 0.15.0 e mostra por-faixa/cobertura. Qwen3.5
2B pré-treinado autorizado, opcional; roda só durante pausa explícita. No lote
real obtido, 1/4 leituras passou, duas inconclusivas e uma pendente em 90 s.
Não tratar isso como qualidade semântica resolvida ou modelo treinado no Tonal.

CTest Windows 10, complementar 7, QML 23/6, Java três variantes, política 43,
Android 35 cenários instrumentados + 13 UI e suíte digital de áudio aprovada.
Ver docs/release-0151-tests.json e docs/LOCAL_STORIES_QUALITY_0151.md.
Backup privado 0.15.1 preparado em 10OdK5GvZoO_7RinWatT8eHzwS2ITDgh6;
pacotes e hashes em docs/artifacts-0151.json. O CI do produto d2bc7cb passou
no run 40253559819. O estado final do merge e dos checks está no PR #19.
O manifesto privado do backup registra uploads e verificações; não inclui
credenciais, pesos opcionais nem dados do telefone físico.
