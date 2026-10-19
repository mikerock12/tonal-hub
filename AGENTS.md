# Instruções para trabalhar neste repositório

Leia [contexto.md](contexto.md), [README.md](README.md) e os documentos da área
afetada antes de alterar o produto. Estado documental: 19/10/2026, produto 0.11.1.
As instruções diretas do usuário prevalecem; este arquivo não cria uma nova
exigência de confirmação para ações já autorizadas.

## Autoria, Git e privacidade

- Preservar **Matheus Nunes (natheus)**, autor do Tonal original:
  https://github.com/natheus/tonal-player, MIT e CREDITS.md.
- Fork privado: https://github.com/mikerock12/tonal-hub. Push somente para
  `private`, após conferir `git remote -v`. **Não fazer push para `origin`**,
  que aponta ao repositório original do irmão.
- Branches novas: `codex/`. Conferir estado local e preservar trabalho existente.
- O usuário autorizou commit/push, PR/CI/merge e backup no Google Drive para as
  entregas solicitadas. Conferir CI antes de merge; não inventar aprovação.
- Nunca versionar/uploadar tokens, senhas, códigos de login, Vault, keystores,
  `.env`, dumps pessoais, bancos SQLite ou logs não revisados no GitHub.
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
  Rejeitar <1,1. Temas só em cache local com escore ≥0,35 nas análises existentes.
- Peso = afinidade ×8 se artista inédito ×2 se faixa inédita; ordenar
  `log(U)/peso` decrescente. Novidade usa histórico deste aparelho, eventos >0 ms.
- Nova semente a cada preparação; sem promessa de sempre diferir. Não é sorteio
  uniforme do catálogo, análise acústica ou leitura de todas as letras novas.
- Shake: Fisher–Yates visual; Aleatório: prioridades independentes de reprodução;
  respeitar primeira faixa explicitamente escolhida. Preservar prévia antes de aplicar.

## Evidências, testes e capturas

- [docs/BENCHMARKS.md](docs/BENCHMARKS.md) consolida as medições. **Não há
  benchmark de CPU/RAM 0.11.1**. Windows 0.8: 122,55 vs 540,53 MiB USS tocando;
  Android 0.8: 16,99 vs 154,78 MiB PSS **sem login**, sem streaming comparável
  Spotify. Nunca misturar versões, USS/PSS ou cenários autenticados distintos.
- CPU 100% significa um núcleo. Distinguir benchmark, teste funcional, emulador
  e aparelho físico. Animações consomem recursos e não devem ser chamadas gratuitas.
- Validação 0.11.1 em `docs/discovery-playback-0111-tests.json`: dez CTest,
  QML 8/6/3, Android host 16 e emulador 21; houve timeout na suíte ampla inicial.
- Rodar verificações proporcionais à mudança; docs não exigem rebuild de APK/EXE.
  Fluxo CI existente em `.github/workflows/ci.yml`; não desativar testes para ficar verde.
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
- Drive privado autorizado:
  https://drive.google.com/drive/folders/1sNjL35fdHzoLY8QcqtBX2XF4YQJbeKlL.
  Reutilizar a organização e verificar tamanho, hash quando disponível e privacidade.
- Backup de fontes via `git archive`, histórico via `git bundle --all`, ambos
  verificados. Dados PC via `packaging/backup-personal-windows.py`; telefone
  físico exige exportação própria. Não confundir backup com sincronização.
- Manter evidência dos binários 0.11.1 associada ao merge de produto
  `54babcf4adcda7ceb8a25dd9648527cdbe1f13fd`; commits documentais posteriores
  não tornam esses binários recompilados ou novamente benchmarkados.
