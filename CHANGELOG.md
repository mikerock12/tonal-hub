# Changelog

## 0.18.3 — 28/11/2026 (Text to Tonalify: discografia completa, rascunhos e mais músicas)

- A busca na discografia lê todas as páginas de lançamentos (álbuns, singles, coletâneas e participações) e as faixas de cada um. "Discografia conferida" só aparece quando a varredura foi completa; corte ou erro viram "busca incompleta".
- "Part II" e "Pt. 2" são o mesmo título. Dois cadastros do Spotify com o mesmo nome e músicas em comum contam como o mesmo artista (caso Scrawl); sem evidência, a escolha fica com você, que pode fixar o cadastro para a lista inteira.
- Rascunhos: ao sair com mudanças, Salvar rascunho e sair / Sair sem salvar / Continuar editando. Continuar, renomear e excluir rascunhos na entrada; recuperação automática se o app fechar.
- Adicionar mais músicas à mesma playlist em revisão (Adicionar ou Substituir pendentes), com prévia de duplicadas, desfazer e busca só das novas.
- Detalhes, antes/depois Ohio e limites: docs/TEXT_DRAFTS_DISCOGRAPHY_0183.md.

## 0.18.2 — 27/11/2026 (Text to Tonalify por artista e estilo das letras)

- Text to Tonalify confirma o artista antes do título: homônimos de outros artistas nunca contam como encontrados nem ficam pré-escolhidos. Cada referência recebe uma classe (encontrada, equivalente, versão alternativa, artista errado, não encontrada, indisponível na região, erro) com o motivo visível.
- Busca progressiva centrada no artista: busca dirigida, ID do artista, variantes do título e discografia do artista antes de desistir. Falhas de rede/5xx são repetidas com espera e nunca viram "não encontrada".
- Singular/plural, créditos e remasters equivalentes são selecionados; versões ao vivo/remix ficam para revisão. Seções, "Só pendências", Ignorar, Tentar de novo e escolhas manuais preservadas; auditoria da escolha salva com a playlist.
- Fontes e cores das letras: 12 fontes e 12 efeitos, cada um mostrado no menu do jeito que fica; cores num trilho arco-íris do branco ao preto, ajuste de 0 a 1000 com − e + nas pontas.
- Detalhes, matriz antes/depois e limites: docs/TEXT_MATCHING_LYRIC_STYLE_0182.md.

## 0.18.1 — 26/11/2026 (plays, ordenação da fila e bastidores)

- Plays abreviados (2,2K, 200,5K, 4,5M) nas páginas de artista, álbum e playlist e na fila. Os números de álbuns e playlists vêm da consulta pública de álbum pela sessão nativa, de 8 em 8 álbuns e fora do canal do Jam.
- A ordenação virou um botão **Ordenar** com lista de critérios, também na fila. Ordenar a coleção que está tocando reordena as próximas. "Mais tocadas" espera os plays e deixa as faixas sem dado no fim.
- Corrige o "#0 no mundo": a posição só aparece quando o artista está entre os 500 com mais ouvintes mensais.
- Bastidores do artista identifica a banda pelo link do Spotify no MusicBrainz e mostra origem, início, integrantes com instrumentos, período e idade (quando as datas estão registradas), além das cidades com mais ouvintes. A busca sobrecarregada do MusicBrainz agora mostra um aviso com **Tentar novamente**.
- Botão **+ Seguir / ✓ Seguindo** estilizado, com mudança de cor ao seguir.
- Detalhes e limites: docs/PLAYS_SORT_ARTIST_0181.md.

## 0.18.0 — 26/11/2026 (biblioteca, estatísticas e silêncio)

- Seguir artistas e favoritar álbuns/playlists no Spotify; coleções locais no IndieTonal incluídas no backup.
- Seguidores, ouvintes mensais, plays e ouvintes por cidade quando disponíveis; contadores reais do Audius.
- Nove ordenações temporárias de playlists, com direção invertível e sem escrita na ordem salva.
- Resumo local de gêneros/clima sem LLM; pesos Qwen, helper, serviço e dependências de inferência removidos.
- Botão para remover zeros digitais no fim da atual e começo da próxima, com ou sem crossfade.
- Detalhes e limites: docs/LIBRARY_POPULARITY_SILENCE_0180.md.

## 0.17.8 — 23/11/2026 (auditoria TH-006/TH-009)

- **Android:** corrige as transições sem efeito. Desde a 0.17.5 a fila entregava ao mixer só um pacote por bloco tocado. Com os pacotes reais do Spotify, a leitura adiantada não passava do tempo real e a mescla ficava vazia. A partir da segunda transição, armar descartava segundos da música atual. O atraso mostrado na tela (posição e letra) agora inclui o áudio guardado na fila.
- **Windows:** o seek e o pulo não perdem mais o começo da nova posição (cerca de 0,4 s).
- **Windows e Android:** no fim natural da fila, o fim já lido da última música (e de uma transição) toca até o fim, em vez de ser descartado como um stop do usuário.
- Testes adversariais que falham na versão anterior: AVD com pacotes reais, motor Windows, mixer, vigia e fila Java. CTest 12/12 e Spotify real no Windows 3/3 (saída nula). Sem Spotify real no Android, aparelho físico ou teste acústico. Relatório em docs/audit/TH006_TH009_AUDIT.md.

## 0.17.7 — 23/11/2026

- **Transições:** amplia em 6 s a antecedência da captura da música atual, dando mais tempo para o Spotify carregar a próxima antes da mescla. A janela anterior podia terminar em `next-not-prepared` e trocar sem efeito.
- A pré-rolagem continua em 2,5 s, sem aumentar o atraso depois da mescla e sem permitir efeito sobre silêncio. Seeks, pulos e seleções continuam cancelando a transição.
- Regressão com carga lenta de B, CTest 12/12, Java 20.080 checks e AVD sem underruns/fallbacks. Spotify real no Windows: 3/3 mesclas com A+B na repetição aprovada; a primeira tentativa registrou um underrun isolado. Sem Spotify real no Android, aparelho físico ou teste acústico.

## 0.17.6 — 23/11/2026

- **Android:** corrige a regressão da 0.17.5 em que avançar pela seekbar podia deixar o áudio mudo e impedir novas transições. O app não descarta mais PCM indefinidamente enquanto espera um marcador de transporte que pode não chegar.
- O evento tipado continua fazendo o flush da saída quando efetivamente aplicado pelo motor, sem bloquear a reprodução. A fila SPSC e a serialização do AudioTrack permanecem.
- Testado na AVD: PCM continua com marcador atrasado, nova transição concluída após seek, zero underruns e regressão do SongPager aprovada. Sem Spotify real no Android ou teste físico nesta correção.

## 0.17.5 — 22/11/2026

- **Android:** corrige o fechamento da página da música quando um gesto era cancelado pela reconstrução do painel e chamava um `SongPager` já substituído.
- **Transições:** fila SPSC limitada para PCM e marcadores, cancelamento por epoch e saída sem esperas temporizadas no monitor do mixer. As operações nativas do AudioTrack são serializadas entre escrita, pausa, flush e fechamento.
- **Windows e Android:** cargas naturais, retomadas, pulos, seeks e paradas têm eventos tipados no fluxo do motor. A limpeza de buffers não depende mais de interpretar linhas `command=Load`/`Seek` no Windows, e PCM antigo não atravessa uma troca de transporte no Android.
- O Windows zera a compensação temporária de volume ao cancelar, trocar de geração ou encerrar uma transição, evitando reutilizar ganho antigo na mescla seguinte.
- **Playlists na fila:** playlists do Spotify e pessoais podem ser adicionadas à fila atual, preservando ordem, ocorrências repetidas, a faixa atual e o limite de 500 entradas.
- **Text to Tonalify:** melhor correspondência de artistas com artigos/colaborações, títulos remasterizados e versões ao vivo/remix; tentativas adicionais quando uma busca só encontra candidatas fracas. Permite acrescentar as músicas revisadas a uma playlist Spotify existente, sempre com POST, sem substituir o conteúdo e com verificação antes de retomar lotes cuja resposta se perdeu.
- Validação sintética Windows/Android e AudioTrack real na AVD; Spotify real no Windows em três transições naturais com A+B e zero underruns. Sem teste físico, acústico ou Spotify real no Android. Detalhes em docs/TRANSITIONS_0170.md, docs/QUEUE_EDITOR_01514.md e docs/TEXT_TO_TONALIFY_090.md.

## 0.17.4 — 21/11/2026

- **Android:** as transições voltam a mesclar a música atual com a próxima. Causa: lido em rajada durante a transição, o fim da música atual era tratado pelo motor como falta de dados. O motor pausava e recarregava o fim da própria música, que entrava na mescla no lugar da próxima. Por isso a atual baixava ou oscilava e a próxima começava depois, sem efeito. Agora o fim chega com a posição exata e segue direto para a próxima.
- A mescla só começa quando o começo da próxima música já chegou. Se ela atrasar, a atual segue sem efeito e a mescla encurta. Sem o mínimo de 1,5 s, a troca é normal, sem efeito, nunca a atual com efeito sobre silêncio.
- "Última transição" só aparece depois que as duas músicas tocaram juntas. A mesma música duas vezes seguidas também tem transição.
- Android: menos trabalho por bloco de áudio na transição, sem alocação de memória por bloco; o mixer acompanha a troca de taxa de amostragem.
- Testado com o Spotify real no Windows: três transições naturais (fade, eco, freio) com as duas músicas na mescla e sem falhas de áudio. Detalhes e limites em docs/TRANSITIONS_0170.md.

## 0.17.3 — 21/11/2026

- Transições de verdade: a música atual e a próxima se misturam no fim, com o efeito. Antes o app estimava errado onde a música atual acabava. O volume dela só baixava ou oscilava, e a próxima entrava depois, sem efeito.
- O motor do Spotify agora marca no próprio áudio onde cada música começa, e a mescla termina exatamente no último instante da música atual.
- Novo controle **Começo da próxima na mescla**, de 3 a 10 s: quanto da próxima música toca por baixo da atual. O antigo tempo de mesclagem virou **Fim da música atual com o efeito**.
- Windows e Android. Detalhes e limites em docs/TRANSITIONS_0170.md.

## 0.17.2 Windows — 20/11/2026

- Se o DirectX do Windows derrubar o app ao abrir a janela, a abertura seguinte usa OpenGL (ou renderização por software, se o OpenGL também cair) e mostra um aviso com o motivo, a placa de vídeo e o renderizador em uso.
- Em Sobre aparece com o que a tela está sendo desenhada e o botão "Tentar DirectX de novo". Depois de reiniciar o Windows o DirectX é tentado de novo sozinho.
- Caso real de 20/11: o `dxgi.dll` estava alterado na memória e derrubava qualquer app Direct3D; reiniciar resolveu. O Warsaw foi descartado como causa. Detalhes em docs/WINDOWS_GRAPHICS_0172.md.
- Android continua na 0.17.1.

## 0.17.1 — 20/11/2026

- Transições sem travar nem pausar: o fim da música atual e o começo da próxima ficam guardados antes da mescla, a saída toca do buffer enquanto o Spotify carrega a próxima, e a mescla começa no ponto certo com as duas prontas.
- Um limitador com antecipação substitui a saturação suave: fora dos picos da soma, as músicas mantêm volume e qualidade originais.
- No Windows a leitura antecipada não deixa mais a saída sem áudio; o volume continua respondendo na hora durante o buffer.
- Detalhes em docs/TRANSITIONS_0170.md.

## 0.17.0 — 20/11/2026

- Transições entre as músicas da fila no Windows e no Android: Fade cruzado, Eco de despedida, Troca de graves, Freio de vinil, Banho de reverb e Loop acelerado, ou Aleatório. Mesclagem de 3 a 10 segundos, com as duas músicas tocando juntas.
- **Colocar como próxima da fila** sempre fica logo após a música atual; **Adicionar à fila** entra depois das próximas escolhidas.
- Letra: atualizar, tradução automática, fontes, tela cheia, sincronia e baixar o ajuste viraram ícones no cabeçalho. Tradução e sincronia ficam azuis quando ligadas e brancas quando desligadas.
- Inclui a correção do atualizador Windows da 0.16.4.
- Detalhes e limites em docs/TRANSITIONS_0170.md.

## 0.15.13 — 07/11/2026

- Descoberta 2+1 volta a encontrar músicas quando o Spotify não informa o gênero do artista, o que hoje acontece com todos os artistas testados. Usa os estilos das outras faixas da seleção e, se preciso, consulta o MusicBrainz sem adivinhar homônimos, guardando o resultado no aparelho.
- A mensagem passa a explicar quando faltam estilos ou quando o Spotify limitou as buscas, em vez de sempre dizer que não há sugestões.
- Android e Windows; no Windows, um limite na consulta do artista não encerra mais a busca.
- Detalhes em docs/DESCOBERTA_2_MAIS_1.md e docs/release-01513-tests.json.

## 0.15.10 — 04/11/2026

- Android: as animações do Natal não param mais ao trocar de música. A proteção de áudio confundia o atraso da música nova na rede com um problema causado pela tela.
- Windows: a seekbar do Natal cabe na barra inferior e na página da música, sem passar da janela.
- Windows: fios e Papai Noel também na capa de **Tocando agora**. Início, álbuns e listas seguem sem animação.
- Detalhes e limites em docs/NATAL_PLAYER_0159.md.

## 0.15.9 — 04/11/2026

- Tema Natal refeito: fios com lâmpadas C9 que passam pela frente e por trás da capa, com sombras coerentes; 65 lâmpadas em quatro padrões de pisca-pisca.
- Seek do Natal: as lâmpadas acendem conforme a estrela passa, com clarão, e apagam ao voltar; estrela maior, facetada e com raios.
- Papai Noel dançando e bebendo cerveja no canto da capa, em loop de 3,38 s, com renas, brinquedos e árvore piscando. Só na página da música e só com a música tocando; início e listas não têm animação.
- Android: corrige sobra de 74 ms da música anterior no início da faixa seguinte, possível causa do apito relatado no carro. Não confirmado no carro; ver limites.
- Relógio único para as decorações e proteção que as interrompe por 30 s se houver falha de buffer de áudio.
- Detalhes, custo medido e limites em docs/NATAL_PLAYER_0159.md e docs/release-0159-tests.json.

## 0.15.8 — 01/11/2026

- Text to Tonalify: o filtro **Só não encontradas** passa a listar todas as referências não selecionadas, incluindo **Revisar versão** e faixas desmarcadas; antes uma playlist de 100 músicas com 89 correspondências mostrava zero.
- Cartões de revisão e o diálogo **Versão** agora mostram música, artista, álbum e miniatura da capa no Windows e no Android.
- Detalhes e limites em docs/TEXT_REVIEW_0158.md e docs/release-0158-tests.json.

## 0.15.7 — 31/10/2026

- Novo tema Natal no Windows/Android: luzes sobre capas, fio de luzinhas no seek, estrela e grinalda; transição play/pause nos dois sentidos.
- Halloween: mesma transição de transporte, teias detalhadas com aranha animada, sangue mais vívido e correção do play invisível nas coleções.
- Cenários Halloween/Bruxas/Monstros/Vampiros redesenhados; novos cenários Natal, Neve e Lareira.
- Coração junto das músicas em álbuns/playlists e layout paisagem com player e letra lado a lado no Android.
- Text to Tonalify: filtro de referências não encontradas e marcação sem mover a rolagem.
- Detalhes e limites em docs/NATAL_PLAYER_0157.md e docs/release-0157-tests.json.

## 0.15.4 — 29/10/2026

- Tema Halloween no Windows/Android: roxo escuro, laranja, teias nas capas e controles em brasa.
- Barra musical com sete escorrimentos, gotas que se desprendem e thumb com gradação/reflexos; até 15 fps, somente visível e tocando. Volume e prévias estáticos.
- Cenários de letras Halloween, Bruxas, Monstros e Vampiros; seletor de tema também no IndieTonal Windows.
- Correção de ícones Canvas após layout tardio e do posicionamento das linhas clicáveis da letra no Windows.
- APKs com a mesma assinatura da 0.15.3, versionCode 24. Execução visual/atualização instalada Android pendentes pela inicialização do emulador; detalhes em docs/release-0154-tests.json.

## 0.15.3 — 27/10/2026

- Interrompe a cascata de músicas puladas quando o Spotify deixa de entregar chaves de áudio (PC e Android): reconecta e retoma pela primeira faixa pulada.
- Volume do PC: o volume inicial do motor após reinício não substitui mais o escolhido; mudo restaura o volume anterior.
- Android: o Tonalify passa a ser o destino dos botões de mídia, para fones/carro não acordarem o app Spotify em segundo plano.
- Jam: avisos leves de entrada, saída, encerramento e mudanças da fila; convite compartilhado ao iniciar; detecção automática de Jam ativo.
- Detalhes e limites em docs/SKIP_VOLUME_JAM_0153.md e docs/release-0153-tests.json.

## 0.15.2 — 26/10/2026

- Retomada privada de preparação, geração e conferência do leitor, inclusive antes da primeira leitura aceita.
- Uma faixa pendente por ação; progresso explícito e continuação, sem repetir leituras já concluídas.
- Backend Android ARM64 DotProd/I8MM/FP16 condicionado às capacidades da CPU, preservando a alternativa genérica.
- Mesmo modelo opcional e limites de áudio/CPU/tempo; interpretação ainda experimental, sem validação no POCO físico.
- Evidências e limites em docs/READER_RESUME_0152.md e docs/release-0152-tests.json.

## 0.15.1 — 25/10/2026

- Atualizar letra nos controles de tela cheia Windows e Android, ignorando o cache corrente.
- Página inicial baseada nas escutas locais, álbuns mais ouvidos e sugestões de discografia.
- Navegação da música simplificada; ações de colocar a faixa como próxima ou última da fila.
- Equalizador ativado ao mover bandas e opção experimental de HRTF para caixas/carro.
- Leitor local v4 por faixa, cobertura explícita e continuação; modelo opcional Qwen3.5 2B.
- Jam experimental com avisos e integração nativa; participação real entre contas ainda pendente.
- Correções de relatórios anteriores: testes sintéticos não comprovam qualidade semântica das letras.

## 0.14.1 Windows — 24/10/2026

- Corrige volume que só mudava após parar de arrastar e saltava com polls antigos.
- Serializa comandos e guarda só o último valor; cancela retries de valores superados.
- Corrige intensidade/eixo da roda do mouse, extremos 0/100 e mostra percentual.
- Android permanece 0.14.0, sem recompilação.

## 0.14.0 — 23/10/2026

- Música, letra e artista em seções verticais completas no PC e no Android.
- Compartilhamento de link Spotify com cartões personalizados; seletor nativo
  Android e exportação PNG/cópia do link no Windows.
- Fatos do MusicBrainz com identificação por nome/álbum, cache limitado,
  fonte explícita e sugestões editoriais relacionadas ao repertório.
- Criar com setlist.fm no Android, com chave criptografada e revisão pelo
  Text to Tonalify antes de salvar ou publicar opcionalmente.
- Créditos do Tonal original de Matheus Nunes (natheus) preservados.


## Tonal Hub pessoal 0.13.3 — 21/10/2026

- Android: trocar o tema não fecha mais o Tonalify. O ícone continua mudando
  na hora; Tonalify e IndieTonal passaram a ter tarefa própria, então só a
  tarefa do Hub em segundo plano é recriada. Voltar no Tonalify ainda leva ao Hub.
- [Detalhes](docs/NOTIFICACAO_ICONE_0132.md#0133-trocar-o-tema-sem-fechar-o-app).

## Tonal Hub pessoal 0.13.2 — 21/10/2026

- Android: notificação e player do sistema com capa do álbum, música, banda,
  álbum, duração e posição real (seek pela barra no Spotify); capa também no
  IndieTonal.
- Ícone seguindo o tema: inicializador Android (um atalho por tema), ícone da
  notificação e ícone da janela/barra de tarefas no PC.
- [Detalhes e validação](docs/NOTIFICACAO_ICONE_0132.md).

## Tonal Hub pessoal 0.13.1 — 21/10/2026

- PC: todos os controles junto da barra de progresso (Shake, Aleatório,
  anterior, tocar, próxima, Repetir, Descoberta 2+1); os modos aplicam direto
  na fila atual e o painel Fila mostra a ordem aplicada. A janela Fila mantém
  prévia e Aplicar.
- Repetição consertada no PC: reaplicada após cada nova seleção (o librespot
  a zerava), sem ser desfeita por consulta atrasada e com aviso em recusa.
- Android: Repetir a fila e repetir uma faixa na página da música, salvo no
  aparelho e reaplicado após cada seleção ou recuperação.
- [Detalhes e validação](docs/REPETICAO_MODOS_0131.md).

## Tonal Hub pessoal 0.13.0 — 21/10/2026

- Seis temas (Tonal Azul, Aurora, Pôr do sol, Neon, Oceano, Grafite suave) no
  PC e no Android, com barra de progresso e marcador próprios; troca ao vivo,
  sem reiniciar nem interromper a música. Catálogo único `resources/themes.json`.
- Aplicar fila (Descoberta 2+1, Shake, Aleatório) sem recarregar a faixa atual
  no Android (`requeue` com `keep_playing_track` no librespot embarcado); no PC,
  posição atual mais latência medida, sem voltar trecho.
- Seek fluido: no PC, um pedido ao soltar, coalescido, sem esvaziar o buffer
  antes da hora e sem GET atrasado puxando a barra; no Android, relógio ancorado
  na posição pedida e nos eventos do motor.
- [Detalhes, causas e validação](docs/TEMAS_SEEK_FILA_0130.md).

## Benchmark 0.12.0 — 19–20/10/2026

- Nova coleta Windows/Android, dois blocos invertidos por app/estado, com
  amostras CSV, resumo JSON, gráfico e rodadas detalhadas.
- Windows minimizado tocando: 110,30 vs 496,83 MiB USS; sem vantagem consistente
  de CPU. Android com Hub medido sem login, sem streaming oficial equivalente.
- Suplemento Tonalify Android autenticado: 72,73 MiB PSS/4,69% de um núcleo,
  zero novos underruns; teste depurável sem Hub, apresentado separadamente.
- [Método, evidências e limites](docs/BENCHMARK_0120.md). Sem alterar o produto
  ou recompilar instaláveis distribuídos; variante de comparação Android atualizada.

## Tonal Hub pessoal 0.12.0 — 19/10/2026

- Descobertas aceitas preservadas ao reorganizar a fila por Shake/Aleatório;
  estado ao reabrir controles e desativação explícita sem cortar a faixa atual.
- Letras das candidatas em segundo plano, análise local TonalMuse, cache derivado
  limitado, cancelamento, opt-out e limites de rede. Sem extração acústica.
- [Funcionamento e validação](docs/DISCOVERY_ENRICHMENT_0120.md).


## Documentação pessoal — 19/10/2026 (produto 0.11.1)

- Contexto persistente em contexto.md e AGENTS.md; especificação da Descoberta
  2+1 com fórmula, histórico local, filtros, limites e diferenças de plataforma.
- README atualizado para 0.11.1, índice dos benchmarks históricos, guia didático
  com 16 capturas reais/testes e origem/versão de cada imagem.
- Publicação pública autorizada pelo titular, com revisão do histórico e
  adição à seção Projetos em destaque do perfil; backup pessoal continua privado.
- Atualização documental: sem alterar binários ou executar um novo benchmark.

## Tonal Hub pessoal 0.11.1 — 18–19/10/2026

- Correções da fila/Descoberta no Windows, preservação de metadados e prévia
  resistente ao refresh da mesma faixa; proteções contra retomada indevida Android.
- Validação e limitações em [DISCOVERY_PLAYBACK_0111.md](docs/DISCOVERY_PLAYBACK_0111.md).

## Entregas pessoais anteriores

- [0.11: fila, novos sorteios e gestos das letras](docs/MIX_QUEUE_LYRICS_0110.md).
- [0.10: modos de reprodução](docs/LISTENING_MIX_0100.md).
- [0.9: Text to Tonalify nativo](docs/TEXT_TO_TONALIFY_090.md).
- [0.8.1: integridade de PCM e recuperação](docs/AUDIO_PCM_081.md).
- [0.8: catálogo, repertório, letras e benchmark](docs/BENCHMARK_080.md).
- [0.7: classificador local e apresentação](docs/TONAL_MUSE_070.md).
- [0.6: página da música](docs/SONG_PAGE_060.md).
- [0.5: biblioteca pessoal](docs/PERSONAL_LIBRARY_050.md).

As seções seguintes preservam notas históricas do desenvolvimento local e do
upstream; declarações como “sem publicação” descrevem apenas aquela etapa.

## Local working tree — Tonal Hub (unreleased)

- Android 0.4.0-local: hub and independent-source player, relevance-ranked public
  catalog and live shows; shared DSP and bounded progressive disk buffer;
  bounded Spotify recovery preserving queue/position, stale-generation guards,
  hard HTTP deadlines and private redacted diagnostics. Same application ID and
  signing key as Android 0.3.0; ARM64 and universal APKs remain local.

- Local 0.1.2: recover from PCM closure, stalled Spotify playback and child
  crashes without blocking the GUI; preserve/cancel resume intent correctly,
  bound token/API request duration and persist bounded sanitized diagnostics.

- Local 0.1.1: rank discovery by artist/title relevance, expose category/live-show
  filters and coverage, retry transient MusicBrainz errors with bounded backoff
  and Retry-After, cache bounded public metadata, and offer page-specific retry.

- Separate Windows hub and IndieTonal player; Spotify player display name is Tonalify.
- Public-source catalog adapters for Audius, MusicBrainz, Live Music Archive,
  Jamendo and OpenSubsonic; credentials and source availability remain explicit.
- Independent discovery suggestions, query handoff, instance reuse and pause on switching.
- Native HTTPS streaming through the existing DSP, bounded asynchronous read buffer,
  limited network/HTTP reconnect and explicit retry after failure.
- Local build, packaging and catalog/UI/IPC tests. See [TONAL_HUB_LOCAL.md](docs/TONAL_HUB_LOCAL.md).
- No Android hub APK, service account creation or GitHub publication in this delivery.

## Local working tree — advanced audio (unreleased)

- Android 0.3.0-local: FFmpeg DSP/decoders, OpenAL Soft HRTF, raw IAMF/OAR binaural,
  ReplayGain/R128, desktop presets, per-output profiles and adaptive MEDIA buffers.
- Android Spotify: unity digital volume, constant track normalization, bounded
  encrypted disk cache, progressive read-ahead and playback-preserving reconnection.

- FFmpeg decoding/DSP, 10-band EQ, bass, optional compression, track normalization and exact bypass.
- Shared OpenAL Soft output, stereo/multichannel HRTF, and a pinned optional IAMF/OAR binaural adapter.
- Smooth pause/resume and source transitions; Spotify pause keeps draining the PCM pipe without flushing queued audio.
- Local-file playback using the existing playback controls; Ctrl+E audio panel and persisted settings.
- Synthetic integration tests, offscreen UI smoke test, dependency lock and local setup/build scripts.
- Scope and validation limits: [AUDIO_LOCAL.md](docs/AUDIO_LOCAL.md). No release or GitHub publication.

## 0.3.0 (October 2026)

- **English interface:** Tonal is now available in English and Brazilian Portuguese. It follows the Windows
  language on first run, and you can switch it in the account menu (Tonal restarts to apply it). Dates and
  durations follow the chosen language too.
- **Setlists:** a new page (button next to Home) that searches a band's shows on setlist.fm and turns the checked
  shows into a private playlist. Shows stay checked when you search another band, so one playlist can mix bands,
  and songs repeated across shows go in once. It needs your own free setlist.fm API key, stored in Windows
  Credential Manager.

## 0.2.0 (October 2026)

- **Synced lyrics:** a microphone button in the player bar. The current line is highlighted, the lyrics follow
  the song and clicking a line seeks to it. Lyrics come from [LRCLIB](https://lrclib.net), and Tonal only sends
  it the track title while the lyrics view is open.
- **Playlists:** create one with the **+** in Your Library, rename it and edit its description (click the
  title), delete it, add tracks to a playlist and remove them from any track's menu, and drag tracks to reorder
  them. On your first edit, Tonal asks you to authorize it again in Spotify, now with access to your private
  playlists.
- **Media keys and the Windows media flyout:** play, pause, next and previous from the keyboard, even with Tonal
  minimized, and the current track with its cover in the Windows media flyout.
- **Better protected login:** the Spotify tokens move from the registry to Windows Credential Manager. This
  happens automatically the first time the new version opens.

If you pinned Tonal to the taskbar, pin it again after updating: the old pin may open as a separate button.

## 0.1.0 (October 2026)

First release: home screen, search, Your Library, playlist, album, Liked Songs and artist pages, a player with
queue, Now Playing and device switching, and an audio engine (librespot) that recovers by itself when the
connection drops.
