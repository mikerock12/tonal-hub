# Contexto do Tonal Hub

Atualizado em **20/10/2026**. Estado do produto: **0.12.0**; benchmark posterior sem alterar binários distribuídos. Leia também [AGENTS.md](AGENTS.md).

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
