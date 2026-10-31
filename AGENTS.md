# Instruções para trabalhar neste repositório

Leia [contexto.md](contexto.md), [README.md](README.md) e os documentos da área
afetada antes de alterar o produto. Estado documental: 25/10/2026, builds Windows 0.15.1 / Android 0.15.1.
As instruções diretas do usuário prevalecem; este arquivo não cria uma nova
exigência de confirmação para ações já autorizadas.

Estado atual: 31/10/2026, entrega Windows/Android 0.15.7. As seções anteriores são históricas; o contrato atual do leitor está na seção 0.15.2 e as correções visuais/player mais recentes ao final deste arquivo.

## Autoria, Git e privacidade

- Preservar **Matheus Nunes (natheus)**, autor do Tonal original:
  https://github.com/natheus/tonal-player, MIT e CREDITS.md.
- Fork: https://github.com/mikerock12/tonal-hub; publicação pública autorizada
  pelo usuário em 19/10/2026. O nome do remoto continua `private`. Push somente para
  `private`, após conferir `git remote -v`. **Não fazer push para `origin`**,
  que aponta ao repositório original do irmão.
- Branches novas: `codex/`. Conferir estado local e preservar trabalho existente.
- O usuário autorizou publicação pública do fork, destaque no perfil,
  commit/push, PR/CI/merge e backup privado no Google Drive para as
  entregas solicitadas. Conferir CI antes de merge; não inventar aprovação.
- Nunca versionar/uploadar tokens, senhas, códigos de login, Vault, keystores,
  `.env`, dumps pessoais, bancos SQLite ou logs não revisados no GitHub público.
  Dados pessoais só no backup privado autorizado, excluindo credenciais.
- Não delegar a subagentes sem pedido explícito do usuário/instrução aplicável.

## Arquitetura e desempenho

- Preservar C++20/Qt/QML no Windows e Java nativo + C++ JNI/Rust existentes no
  Android. Novas regras portáveis devem preferir C++ compartilhado; não embutir
  React, Electron, navegador, servidor JavaScript ou LLM para recursos simples.
- Não colocar rede, banco, busca de letras, inferência ou trabalho de UI no
  callback de áudio. Usar limites de fila/cache, cancelamento e estados explícitos.
- Pausa/stop do usuário prevalecem sobre recuperação, callbacks tardios e seleção
  pendente; reproduzir em segundo plano só conforme a intenção autorizada.
- Bluetooth é rota PCM do sistema. Não prometer controle de codec, ausência
  absoluta de cortes, fidelidade superior ou latência física sem medição real.
- TonalMuse é um classificador local pequeno, sem diagnóstico de humor/saúde;
  corpus sintético e escores não calibrados devem continuar identificados.

## Contrato da Descoberta 2+1

A referência completa está em [docs/DESCOBERTA_2_MAIS_1.md](docs/DESCOBERTA_2_MAIS_1.md).
Atualize esse documento, contexto.md e os testes pertinentes se as regras mudarem:

- Duas originais → uma externa, numa fila temporária preparada, sem editar a
  playlist. Até 500 entradas; 500 originais podem impedir extras.
- Até 120 candidatos, seis consultas de gênero, duas páginas de dez por consulta;
  ano de referência ±8 mais busca sem ano; Windows 15 s, Android 6,5 s.
- Excluir URI/artista normalizado da seleção; álbum de origem em modo álbum;
  não repetir artista externo na fila. Filtros textuais karaoke/tribute.
- Melhor afinidade entre até 12 originais de referência; gênero comum
  `3+interseção/max(tamanhos)`, ano `max(0,1-diferença/20)`, +0,2/tema comum.
  Rejeitar <1,1. Temas em cache local com escore ≥0,35; enriquecimento LRCLIB
  assíncrono limitado, cancelável e opcional. Consultar DISCOVERY_ENRICHMENT_0120.md.
- Peso = afinidade ×8 se artista inédito ×2 se faixa inédita; ordenar
  `log(U)/peso` decrescente. Novidade usa histórico deste aparelho, eventos >0 ms.
- Nova semente a cada preparação; sem promessa de sempre diferir. Não é sorteio
  uniforme do catálogo ou análise acústica. Letras são buscadas para o lote
  limitado; resultados só valem para preparações futuras, nunca alteram fila confirmada.
- Shake: Fisher–Yates visual; Aleatório: prioridades independentes de reprodução;
  respeitar primeira faixa explicitamente escolhida. A janela Fila preserva prévia
  antes de aplicar; os botões do player aplicam direto (pedido do usuário, 0.13.1).
- Descobertas aceitas permanecem ao mudar Shake/Aleatório e reabrir controles;
  só desligar Descoberta remove próximas externas. Atual não é interrompida.
- Cache derivado: 2.000 entradas, sem letras completas, 1 pedido/vez, 1,25 s,
  timeout 6 s, corpo 256 KiB, cooldown 429/503 preservado entre seleções.
- Repetição é do app: reaplicar após cada nova seleção (librespot zera); pular
  desliga só a de uma faixa. PC `Player::reapplyRepeat`; Android `RepeatMode`.
- Aplicar fila não recarrega a faixa audível: Android `requeue|…` com
  `keep_playing_track` no librespot embarcado; recuperação segue com `restore`.
  PC reenvia pela Web API com posição atual + latência medida, sem flush.

## Temas e seek (0.13.0)

- Catálogo único `resources/themes.json` = `android/.../assets/themes.json`;
  `tests/themes/check_themes.py` exige cópia idêntica, tokens e contraste AA.
  Novo tema: editar o JSON, copiar ao Android e rodar a verificação.
- Trocar tema não pode recriar a Activity Android nem tocar motor/fila; o PC
  troca ao vivo via `App.theme`. O usuário autorizou em 28/10 a animação da barra
  Halloween: até 15 fps, apenas visível e tocando, sem alterar seek/áudio.
  Restante dos controles do tema sem animação contínua ou shader personalizado.
- Ícone por tema: Android activity-alias `HubIcon<Tema>` (gerar com
  `packaging/generate-theme-icons.py`; ativar o novo antes de desativar o antigo);
  PC `App::themedIcon`. Notificação: metadados/capa da música atual na MediaSession.
  Tonalify/IndieTonal em tarefas próprias (taskAffinity): trocar o alias do Hub não
  pode fechar o player aberto; nunca trocar o ícone ao abrir o Hub.
- Seek: PC envia um pedido ao soltar (coalescido) e só descarta o buffer no
  `command=Seek(` do librespot; ambos ignoram poll remoto antigo (3 s/1,5 s).

## Evidências, testes e capturas

- [docs/BENCHMARKS.md](docs/BENCHMARKS.md) consolida as medições. Nova coleta
  0.12.0 em 19–20/10: Windows minimizado tocando 110,30 vs 496,83 MiB USS,
  CPU 0,77% vs 0,67% de um núcleo; sem vantagem consistente de CPU entre rodadas.
  Android com Hub: 31,49 vs 156,84 MiB PSS **sem login**; sem streaming oficial
  autenticado equivalente. Nunca misturar versões, USS/PSS ou cenários distintos.
  Método e dados em docs/BENCHMARK_0120.md; 0.8 permanece histórico separado.
  Suplemento Tonalify Android de teste sem Hub: 72,73 MiB PSS/4,69%, zero novos
  underruns; variante depurável, sem streaming oficial equivalente para dividir.
- CPU 100% significa um núcleo. Distinguir benchmark, teste funcional, emulador
  e aparelho físico. Animações consomem recursos e não devem ser chamadas gratuitas.
- Validação 0.11.1 em `docs/discovery-playback-0111-tests.json`: dez CTest,
  QML 8/6/3, Android host 16 e emulador 21; houve timeout na suíte ampla inicial.
- Rodar verificações proporcionais à mudança; docs não exigem rebuild de APK/EXE.
  Fluxo CI existente em `.github/workflows/ci.yml`; não desativar testes para ficar verde.
- Validação 0.12.0 em `docs/discovery-enrichment-0120-tests.json`: 10 CTest Windows,
  5 CTest portáteis, QML 9/6/3, 21 cenários Android e 24 verificações JNI/cache,
  além da interface com retenção dos modos. Houve interrupção por ANR do System UI
  da AVD antes da repetição final aprovada; não ocultar essa limitação.
- Validação 0.13.0 em `docs/temas-seek-fila-0130-tests.json`: CTest 10/6,
  QML 12/6/3, temas 172+86 checagens, política Java 21, smoke de temas Android
  com áudio local (0 underruns) e regressão da fila. Sem streaming real Spotify
  nem aparelho físico: não declarar fila/seek medidos com áudio real.
- Validação 0.13.1 em `docs/repeticao-modos-0131-tests.json`: CTest 10, QML
  15/6/3, Java 40, AVD repetição 8 + temas 9 + fila. Teste ao vivo `--verify-mix`
  pronto, não executado (permissão negada): não declarar fila/repetição com Spotify real.
- Validação 0.13.2 em `docs/notificacao-icone-0132-tests.json`: AVD 7 (notificação,
  capa IndieTonal, alias do ícone), temas 183, CTest 10. Sem capa Spotify real.
- Capturas: [guia](docs/GUIA_VISUAL.md) e `docs/screenshots/manifest.json`.
  Identificar versão e dados sintéticos; não fabricar telas ou números para marketing.

## Ambiente e backup

- Preferência do usuário (25/10/2026): o relatório final deve informar qualquer
  aplicativo ou ferramenta adicional instalado/baixado no PC durante a tarefa e
  por que foi necessário, incluindo ferramentas portáteis. Não confundir dependência
  local de build com componente instalado no produto.

- Workspace Windows: `D:/PROJETOS/Tonal-Player-Matheus`, PowerShell.
- Qt local: `out/tools/Qt/6.8.3/msvc2022_64`; Python:
  `out/tools/python/Scripts/python.exe`; build Windows: preset `release`.
- Android: JDK 17, Gradle 8.14.3, SDK 36, NDK 27.0.12077973. Scripts em
  `packaging/build-android-local.ps1`; ARM64 para POCO, universal inclui x86_64.
- AVD exclusiva Tonal_Local_20261015, porta 5560; não tocar em AVD de outro
  projeto. Evitar build pesado/compressão e emulador simultâneos em host de 16 GB.
- Drive privado autorizado (pastas 0.13.0: 1BR5bH6HQxCGUKrB0YAAMXTzp0ZQ0JBiI;
  0.13.1: 12L0IbCaTwURRIcOoHaQMLcgVYnP3nMcV; 0.13.2: 1nW93Tq55vdBJcCv2pGtYjJNUgHvrSl99;
  0.13.3: 1QgE4Ja6iKKczifxhrirB-Dx9AJ3fTTqZ):
  https://drive.google.com/drive/folders/15bxn_XcZianWzzp9E5GdYI8deVqm1fy5.
  Reutilizar a organização e verificar tamanho, hash quando disponível e privacidade.
- Backup de fontes via `git archive`, histórico via `git bundle --all`, ambos
  verificados. Dados PC via `packaging/backup-personal-windows.py`; telefone
  físico exige exportação própria. Não confundir backup com sincronização.
- Manter evidência dos binários 0.11.1 associada ao merge de produto
  `54babcf4adcda7ceb8a25dd9648527cdbe1f13fd`; commits documentais posteriores
  não tornam esses binários recompilados ou novamente benchmarkados.

## Página da música e compartilhamento (0.14.0)

- Referência: docs/SONG_PAGES_SHARE_SETLISTS_0140.md. Três seções completas
  abaixo do cabeçalho; respeitar rolagem interna da letra e encaixe de página.
  Não reiniciar áudio nem a seleção ao navegar, compartilhar ou abrir fatos.
- Compartilhar congela a faixa: URI Spotify de track com 22 caracteres,
  link canônico, PNG 600×1066. Android usa seletor nativo; Windows copia/salva.
  Não selecionar contatos nem enviar mensagens automaticamente.
- Provider Android não exportado, grants só de leitura para PNGs gerados em
  share-cards; nunca expor files/, banco, credenciais ou paths arbitrários.
- Artista: MusicBrainz identificado de forma única ou pelos créditos do álbum,
  cache por artista+álbum até 32 entradas; abster em homônimos. Sugestões por
  regras editoriais identificadas. Throttle MusicBrainz comum de 1,25 s e
  backoff 429/503, cancelamento de entregas e limites de resposta/timeout.
- Setlist.fm Android usa Secrets(namespace setlistfm), sem copiar token/chave
  para parser, playlist ou backup. Buscar/selecionar não escreve no Spotify;
  revisão e salvamento local pelo Text to Tonalify precedem cópia remota opcional.
- Capturas 0.14 usam dados sintéticos; não declarar teste físico, benchmark novo,
  busca autenticada setlist.fm ou envio para terceiros sem evidência.

- Validação 0.14: docs/song-pages-0140-tests.json (CTest 10, QML 17/6/3,
  temas 183, Java 40, Android 18 nativas + 10 UI, MusicBrainz ao vivo no C++).
  Drive privado 0.14: 1iztlSwSWiDcCk5A6Wd1XIBHlJRGzEae4.

## Volume Windows (0.14.1)

- Referência: docs/VOLUME_WINDOWS_0141.md. Throttle 150 ms, um PUT por rota
  em voo + último valor, geração para polls pré-intenção/pré-ACK e guarda 3 s.
- Nunca enviar volume pendente para outro dispositivo. Cancelar ao mudar rota,
  sessão ou entrar em arquivo local. 499 local opt-in libera pedido superado;
  não mudar o comportamento de cancelamento das demais chamadas da API.
- Roda: respeitar delta, acumular frações, ignorar eixo horizontal e zero;
  extremos da barra atingem 0/100. Persistência agrupada, fora do callback PCM.
- Não duplicar ganho: Spotify usa mixer librespot; arquivos locais usam OpenAL.
  Validação simulada de transporte não equivale a medição acústica/Spotify real.
- Android permanece 0.14.0 nesta correção exclusiva do Windows.

- Validação complementar 0.14.1: CTest final 10/10, zero falhas, 58,04 s.
  Preservar as tentativas anteriores no relatório; alvo AVD já ausente, outra
  AVD preservada, sem alteração do produto/limites e sem causalidade provada.
  Binários continuam no código d4c812d9cda8b66a1f40ccecd0c46aaddb3f5192;
  commits documentais de evidência não significam rebuild ou novo benchmark.

## Estatísticas e leitor local (0.15.0)

- Ler docs/STATISTICS_STORIES_0150.md. Estatísticas e backup ficam no cabeçalho
  global; período zero significa Desde o começo, sem fallback para 7 dias.
- Não apagar sessões por idade ou evicção do cache (10.000 sessões recentes).
  SQLite é o arquivo histórico. Exclusão explícita usa historyEpoch; restauração
  usa libraryEpoch. C++ deve comparar epochs com value(), sem inserir null por
  operator[] mutável. Queries/agrupamento e seleção do lote ficam no worker.
- Android fixa uma transação durante o uso de tabelas TEMP no WAL: conexões do
  pool não compartilham tabelas temporárias. Não ocultar falha como total completo.
- Backup completo inclui o arquivo histórico, mede bytes efetivamente gravados e
  não inclui pesos/credenciais/letras completas. Guardas JSON: 64 MiB, 250.000
  sessões, 100.000 metadados, 100 playlists. Não confundir com retenção por idade.
- O usuário autorizou explicitamente um LLM para interpretar letras e, em 25/10,
  priorizou qualidade com modelo opcional de cerca de 1,2 GiB: Qwen3.5 2B
  Q4_K_M (quantização Unsloth) + llama.cpp, aberto, opcional e nativo. Isso não autoriza LLMs em recursos
  simples ou no motor de áudio. Não substituir o Tiny por marketing de LLM.
- Não treinar nem chamar de treinado pelo Tonal o Qwen pré-treinado. Preservar
  Apache 2.0/MIT, revisões e hashes em resources/local-stories-model.json.
- Só iniciar inferência por ação do leitor, máximo 8 letras, 1 thread CPU tanto na geração quanto na preparação,
  1024 contexto / 768 entrada por passe / 64 saída por faixa + 64 de síntese
  (até 585 tokens no lote), orçamento 90 s e supervisão 105 s.
  Processo separado TonalStories.exe / serviço privado :stories; fechar cancela
  também seleções ainda pendentes. Nunca carregar pesos no player/callback PCM.
- Cache 128 letras limitadas + 20 resumos, privado e descartável. Mostrar fontes,
  lote e período. Não inventar temas para preencher o card, nem inferir saúde.
  Downloads verificam 1.280.835.840 bytes e SHA-256 antes da ativação; usar HTTPS.
- Build: packaging/build-stories-local.ps1 (Windows) ou -Android. Fontes externas
  fixadas e verificadas; pesos baixados pelo app, não embutidos em EXE/APK.

- Prioridade de áudio no leitor 0.15: preparação/cache/rede fora da UI, captura
  pendente agrupada até 128 faixas; inferência CPU 1 thread, processo ocioso PC /
  preparação Android BACKGROUND (+10); inferência só durante pausa, nice +2,
  sem forçar perfil de I/O ocioso
  durante mmap (Bionic setpriority no JNI, antes dos pesos; erro impede inferência). O serviço
  continua BIND_NOT_FOREGROUND. Download e LRCLIB deste leitor aguardam pausa.
  Exigir pesos + 512 MiB disponíveis antes de carregar; encerrar só inferência
  sob pressão (<256 MiB; lowMemory no Android). Aguardar/cancelar nunca toca fila.
  Prioridade segue intenção de reproduzir, não simples presença de frames;
  buffering não é pausa para liberar downloads.
  Guardas não autorizam prometer impacto absoluto zero ou encerrar outros apps.


## Leitor 0.15.1 e Jam em desenvolvimento (25/10/2026)

- A frase “O conjunto reúne letras que contrastam e contrastam.” da 0.15.0
  falhou na aceitação física do usuário. Testes sintéticos de execução não
  comprovam interpretação real. Preservar a limitação nos relatórios históricos.
- Leitor v4: leitura por faixa, síntese das leituras aceitas, rejeição de formato
  genérico/repetitivo/cópia e relatório tipado com cobertura e índices conferidos.
  Os filtros são heurísticos, não garantem correção semântica. Não expor escores
  não calibrados como confiança de interpretação.
- Inferência também no Windows só em pausa, por escolha explícita do usuário.
  Retomar reprodução encerra só o processo do leitor, mantendo a solicitação
  pendente. Não pausar a música automaticamente para liberar inferência.
- Orçamento de 90 s por ação: preservar leituras parciais válidas e permitir
  Continuar análise com cache do lote exato (modelo/prompt/período/textos/nomes).
  Não chamar pendência de tempo de faixa analisada nem recalcular leituras já válidas.
- Qwen3.5 2B é pré-treinado, não treinado pelo Tonal. Troca de pesos é manual,
  verificada; só remover os pesos 0.15.0 após ativar/verificar os novos.
- Nova solicitação: Jam com amigos no Spotify, moderação de entrada e avisos
  leves no canto inferior esquerdo com subida/fade. Referência: docs/JAM_RESEARCH.md.
  A API pública da fila não informa autor nem controla participantes; preservar
  a distinção entre protocolo experimental, recurso simulado e teste ao vivo.

- Esclarecimento Jam do usuário (25/10): se não houver controle de aprovação,
  basta aviso de quem entrou. Faixas adicionadas/alteradas e autoria são exibidas
  quando o protocolo as identificar; não inventar atribuição a participantes.

## Início, fila e áudio em validação (0.15.1)

- Ler docs/HOME_QUEUE_AUDIO_0151.md. Recência usa escutas >0 ms e ID de álbum;
  consultas ao arquivo histórico ficam no worker. Abrir um álbum não é ouvi-lo.
- Atualização manual da letra ignora apenas o cache corrente. Tela cheia deve
  atualizar o provider da letra exibida e permitir tradução automática novamente.
- Inserir na fila preserva pausa, ocorrências repetidas e descobertas. Edição
  atrasada Android deve conferir a faixa antes de tocar no contexto. Windows
  prepara a edição pausada para o próximo play explícito pelo Tonalify.
- HRTF para caixas/carro é binaural experimental, sem crosstalk cancellation ou
  calibração do ambiente. Não declarar equivalência acústica com fones nem teste
  no rádio físico sem evidência. Bypass mestre prevalece sobre EQ/HRTF.

## Entrega 0.15.1 e limites

- Atualizar letra também na tela cheia: busca explícita ignora cache; preservar
  primeiro toque para revelar controles e serialização de busca/tradução.
- Home deriva álbuns de escutas >0 ms; consultas/agrupamento no worker.
- Próxima/última preservam ocorrências e pausa. Jam reconcilia janela parcial
  por ocorrências; conserva marcadores de Descoberta e cauda conhecida. Acima de
  500, recusar reaplicar modos sem truncar músicas. Não atribuir autoria por inferência.
- Jam permanece experimental: o teste nativo read-only respondeu 404 sem sessão;
  não prova participação entre contas. HRTF caixas/carro não tem crosstalk/calibração.
- Leitor 2B ainda limitado: 1/4 leituras aceitas no lote real, duas inconclusivas
  e uma pendente. Revisor do próprio modelo é heurístico; não declarar problema
  semântico totalmente resolvido. Trechos agora até 3.000 bytes antes do limite
  de tokens; stdin do auxiliar até 64 KiB. Pesos não treinados pelo Tonal.
- Evidências: docs/release-0151-tests.json e docs/artifacts-0151.json. Binaries
  associados ao commit de produto d2bc7cb51b3f6fe022ba881263eab9b2c3691790;
  commits documentais posteriores não significam novo build ou benchmark.
- Backup privado 0.15.1: 10OdK5GvZoO_7RinWatT8eHzwS2ITDgh6. Modelos/fontes
  externas regeneráveis, credenciais, letras completas e telefone físico não
  integram o snapshot privado SQLite deste PC. Nunca confundir com sincronização.

## Leitor 0.15.2 — retomada e backend ARM64

- Ler docs/READER_RESUME_0152.md e docs/release-0152-tests.json. V5/schema 3;
  zero leituras aceitas com pendências é resultado parcial válido, sem frase
  inventada. Cada ação processa uma faixa pendente, até oito por lote.
- Checkpoint vinculado a modelo/prompt/período/textos/nomes exatos: até 64 MiB
  mais temporário, checksum e limites antes do restore, escrita atômica. Privado,
  descartável, fora do backup. Não publicar checkpoint, letras ou rascunhos.
- O estado llama não preserva logits. Próximo token e margem de conferência
  precisam ser salvos junto; não amostrar logits inexistentes após restore.
- Continuam 90 s/105 s, uma thread e inferência somente durante pausa explícita.
  Retomar áudio mata o leitor sem aguardar snapshot; retoma o último já válido.
- ARM64 fast é opcional: HWCAP exige NEON, FP16 escalar/vetorial, DotProd,
  I8MM e atomics. Ausência mantém baseline. Não usar nome do aparelho como guarda.
- Mesmo modelo/hash 0.15.1; nenhum novo treinamento. A conferência pelo próprio
  modelo é heurística. 4 propostas aceitas no teste desktop não são 4 acertos
  semânticos; não declarar validação física ou velocidade medida no POCO X7 Pro.
- Instaláveis identificados em docs/artifacts-0152.json, código de produto
  93ef7455a5c98b3ee8aa66d0ab56c3bcee1b08bf. Commits documentais não são rebuild.
- Backup privado 0.15.2: 1rVGPhTHvN79-CplhB5wJyEe3DeR_CZZl. Preservar verificação
  de tamanho e privacidade; SHA remoto só quando o conector o fornecer.

## Reprodução e Jam 0.15.3

- Ler docs/SKIP_VOLUME_JAM_0153.md. Cascata de pulos só conta com timeout de
  chave de áudio (PC: linha do librespot nos 10 s anteriores; Android:
  `KEY_TIMEOUTS` do core vendorizado). Dois em 30 s reiniciam e retomam pela
  primeira faixa pulada; faixa realmente indisponível não reinicia o motor.
- Volume: só o valor inicial do motor (15 s após Ready, >10 pontos, aparelho
  local) é reafirmado, uma vez. Não bloquear volume vindo de outro aparelho/app.
- MediaButtonReceiver ignora teclas com o player fechado; não iniciar
  reprodução por ele nem contornar a política de intenção/tela bloqueada.
- Jam: avisos só dentro do app (sem overlay), fila de 8, nomes limitados;
  convite por seletor/cópia escolhidos pelo usuário. Não atribuir autoria de
  faixas. Consultas automáticas limitadas (Android 2 min, PC 5 min).
- Validação 0.15.3: docs/release-0153-tests.json e docs/artifacts-0153.json,
  código de produto f5bd00d. Smoke `tests/android/media_button_smoke.py` exige o
  app Spotify na AVD. Commits documentais posteriores não significam rebuild.

## Halloween 0.15.4

- Ler docs/HALLOWEEN.md. Catálogo compartilhado de sete temas; cenários de letras
  independentes Halloween/Bruxas/Monstros/Vampiros. Capturas sintéticas devem ser identificadas.
- BloodSurface QML é compartilhado pelas três barras Windows. Android usa
  BloodTrail/BloodDrop nativos com objetos de desenho reutilizados. Até 15 fps;
  pausa, ocultação, janela sem foco e outro tema encerram o relógio. Android
  respeita ValueAnimator.areAnimatorsEnabled(). Volume/prévias são estáticos.
- Essa animação foi explicitamente solicitada pelo usuário, sem promessa de
  custo zero ou simulação física. Nenhum tick envia comandos de seek/áudio.
- APK versionCode 24; conferir certificado da 0.15.3 e instalação de atualização
  na AVD sem desinstalar. Nunca versionar/copiar/enviar credenciais de assinatura.

- Evidências 0.15.4: docs/release-0154-tests.json e docs/artifacts-0154.json.
  Código Windows 8e96297; Android 72d5c81 com árvore idêntica no commit seguinte.
  Assinatura da 0.15.3 e alinhamento 16 KiB verificados; instalação por cima,
  execução visual Android e POCO físico pendentes. Emulador falhou antes do ADB;
  cópia isolada de dependências existentes em out/tools, sem instalar/baixar apps.
  Capturas públicas usam dados sintéticos. Preservar timeouts de UI e tentativas
  de CI, sem declarar testes físicos, acústicos ou benchmark novo.

## Entrega Halloween 0.15.6 — 30/10/2026

- Android: APKs assinados pelo certificado existente, versionCode 26 e alinhamento 16 KiB conferidos; teste em aparelho relatado pelo usuário. SHA-256 na release. Não alegar que o teste visual valida reprodução autenticada no desktop.
- Windows: código de produto da árvore do merge a133e12; CI Windows 40996835093 compilou com IAMF, executou 10 grupos de testes e instalou, iniciou, reinstalou preservando arquivo de teste e removeu a cópia isolada. Também há testes QML sintéticos; teste acústico/hardware físico não foi feito.
- O FFmpeg exato de versions.lock deixou de estar disponível no endereço original. Uma cópia privada fornecida pelo usuário foi verificada pelo SHA-256 fixado. Para um build futuro, disponibilizar o pacote original em out/audio-deps/ffmpeg-pinned.zip; não substituir por outro build sem revisão.
- O workflow de empacotamento era específico desta release, dependia de uma transferência temporária e foi removido após a conclusão para não bloquear PRs futuros. A release e o resultado do CI permanecem registrados no GitHub.

## Natal e player 0.15.7 — 31/10/2026

- Ler docs/NATAL_PLAYER_0157.md. Oito temas compartilhados; Natal tem luzes sobre capas, seek com fio/lâmpadas/estrela e grinalda. Capa/seek/aros ficam limitados a cerca de 15 fps quando visíveis; respeitar foco, ocultação e animações do sistema. Nenhum tick emite seek/áudio.
- Play/pause usa transição nos dois sentidos e segue o estado real do player. Halloween compartilha o comportamento; o botão das coleções deve manter contraste entre ícone e fundo.
- Teias/aranha e sangue são vetoriais estilizados, não fotografia nem simulação física. A aranha desce, tece e sobe apenas em capas grandes visíveis. Há 18 cenários de letras; Halloween/Bruxas/Monstros/Vampiros e Natal/Neve/Lareira permanecem independentes do tema.
- Corações junto das faixas atualizam biblioteca local/Spotify sem reconstruir a coleção. Em paisagem Android, player à esquerda e letra à direita; preservar cabeçalho, página ao retornar, motor e fila. Modos secundários podem usar rolagem horizontal.
- Text to Tonalify filtra apenas status Não encontrada e atualiza somente o cartão marcado, sem mover a rolagem. Busca/revisão não escreve na conta.
- Evidências locais: temas 243, QML 25/6/3, CTest final 10/10, componentes nativos e fluxos sintéticos na AVD. Preservar a falha intermitente de Halloween e a suíte CTest inicial 4/10 com repetições posteriores. Não declarar teste físico, Spotify autenticado, escrita real de playlist, acústica ou benchmark novo.
