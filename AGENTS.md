# Instruções para trabalhar neste repositório

Leia [contexto.md](contexto.md), [README.md](README.md) e os documentos da área
afetada antes de alterar o produto. Estado documental: 21/10/2026, produto 0.13.1.
As instruções diretas do usuário prevalecem; este arquivo não cria uma nova
exigência de confirmação para ações já autorizadas.

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
  troca ao vivo via `App.theme`. Sem animação contínua ou shader para temas.
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
- Capturas: [guia](docs/GUIA_VISUAL.md) e `docs/screenshots/manifest.json`.
  Identificar versão e dados sintéticos; não fabricar telas ou números para marketing.

## Ambiente e backup

- Workspace Windows: `D:/PROJETOS/Tonal-Player-Matheus`, PowerShell.
- Qt local: `out/tools/Qt/6.8.3/msvc2022_64`; Python:
  `out/tools/python/Scripts/python.exe`; build Windows: preset `release`.
- Android: JDK 17, Gradle 8.14.3, SDK 36, NDK 27.0.12077973. Scripts em
  `packaging/build-android-local.ps1`; ARM64 para POCO, universal inclui x86_64.
- AVD exclusiva Tonal_Local_20261015, porta 5560; não tocar em AVD de outro
  projeto. Evitar build pesado/compressão e emulador simultâneos em host de 16 GB.
- Drive privado autorizado (pasta 0.13: 1BR5bH6HQxCGUKrB0YAAMXTzp0ZQ0JBiI):
  https://drive.google.com/drive/folders/15bxn_XcZianWzzp9E5GdYI8deVqm1fy5.
  Reutilizar a organização e verificar tamanho, hash quando disponível e privacidade.
- Backup de fontes via `git archive`, histórico via `git bundle --all`, ambos
  verificados. Dados PC via `packaging/backup-personal-windows.py`; telefone
  físico exige exportação própria. Não confundir backup com sincronização.
- Manter evidência dos binários 0.11.1 associada ao merge de produto
  `54babcf4adcda7ceb8a25dd9648527cdbe1f13fd`; commits documentais posteriores
  não tornam esses binários recompilados ou novamente benchmarkados.
