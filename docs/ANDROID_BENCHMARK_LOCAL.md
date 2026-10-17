# Comparação Android local — 15/10/2026

Este relatório preserva os testes da **0.1.0-local**. A **0.1.1-local** corrigiu a
conexão e passou no teste de streaming; veja [ANDROID_SPOTIFY_FIX.md](ANDROID_SPOTIFY_FIX.md).
O áudio nativo e as medições próprias de streaming da **0.3.0-local** estão em
[ANDROID_AUDIO_LOCAL.md](ANDROID_AUDIO_LOCAL.md). Os números históricos abaixo
não medem o conjunto de recursos atual nem demonstram vantagem de streaming
contra o Spotify oficial.

A prévia do Tonal consumiu **89.6% menos RAM PSS aberta**,
no cenário sem login e sem reprodução medido abaixo. Essa conclusão se limita à
prévia e a esse estado. CPU e memória em segundo plano estão detalhadas na tabela;
o Android pode comprimir/deslocar páginas para swap ao congelar o processo.
Os dados de CPU variam entre amostras e não demonstram uma vantagem geral durante
streaming. Não foi feita uma comparação de bateria.

## Resultados comparáveis

Medianas de 12 amostras por app/estado, reunindo duas rodadas em ordem invertida.

| Cenário | App | RAM PSS (MiB) | Swap informado (MiB) | CPU mediana (%) | CPU média (%) |
|---|---|---:|---:|---:|---:|
| Aberto, sem reprodução | Tonal | 16.5 | 0.0 | 0.00 | 0.08 |
| Aberto, sem reprodução | Spotify oficial | 158.9 | 0.0 | 0.14 | 2.79 |
| Em segundo plano, sem reprodução | Tonal | 16.5 | 0.0 | 0.00 | 0.08 |
| Em segundo plano, sem reprodução | Spotify oficial | 71.6 | 29.2 | 0.32 | 2.40 |

CPU de 100% significa um núcleo totalmente ocupado. A média é ponderada pelo tempo
dos intervalos e inclui os picos; a mediana descreve a amostra central. O emulador tem quatro vCPUs;
para percentual da capacidade total, divida os valores por quatro. Um resultado
0,00% significa que a mediana não acumulou ticks no intervalo, e não custo físico
absolutamente nulo. RAM é PSS, em MiB, não memória privada do Gerenciador de Tarefas.
Swap é apresentado à parte (SwapPSS quando disponível; TOTAL SWAP KB nos demais
snapshots, com o tipo registrado por amostra): páginas deslocadas para swap/zram deixam de contar
da mesma forma na memória residente. Somar PSS e SwapPSS estima o conjunto de
páginas, não o tamanho físico comprimido da zram. As faixas e todas as amostras
estão em `android-benchmark-summary.json`.

## Custo do áudio local no Tonal

São testes funcionais e de custo do Tonal, sem equivalente Spotify reproduzindo.
Não use esta tabela para calcular uma economia percentual contra streaming oficial.
Seis amostras por cenário.

| Cenário Tonal | RAM PSS (MiB) | CPU (% de um núcleo) |
|---|---:|---:|
| Vorbis estéreo, efeitos desligados | 21.1 | 1.90 |
| Vorbis estéreo + EQ, graves e compressor | 21.0 | 2.52 |

Fonte: ruído rosa sintético estéreo Vorbis, 44,1 kHz, nominal 320 kbps, 60 segundos.
Efeitos: EQ de dez bandas, graves +3 dB e compressor. Saída AudioTrack float.
O autoteste DSP verifica bypass idêntico, resposta de +3 dB em 1 kHz e saída finita
com teto de amostra. O teste funcional confirmou frames PCM crescentes, uma faixa
ativa de 44,1 kHz no AudioFlinger, contagem estável enquanto pausado e retomada da
contagem após play e após voltar do segundo plano. O smoke final teve zero
underruns e nenhum crash; resultado em `out/android/audio-smoke/result.json`.
A pausa/retomada usa VolumeShaper de 30 ms;
ausência de cliques audíveis não foi validada auditivamente neste emulador sem som.

## Método e ambiente

- AVD exclusiva `Tonal_Local_20261015`; Android 16/API 36, x86_64, quatro vCPUs,
  4096 MB de RAM, partição de dados 12 GB; Android Emulator 37.2.12, aceleração GPU
  host em Intel HD Graphics 530. Host Windows, quatro processadores lógicos e 16 GB.
- Tonal 0.1.0-local, build release assinado localmente, sem depuração. Interface
  Android nativa Java, Rust/librespot JNI, sem Qt ou WebView nesta prévia.
- Spotify oficial 9.1.90.2270, código 147078674. APK original obtido por espelho e
  assinatura SHA-256 confrontada com o certificado publicado pelo próprio Spotify:
  `6505b181933344f93893d586e399b94616183f04349cb572a9e81a3335e28ffd`.
  Evidência: `out/android/comparison/spotify-origin.json` e
  https://open.spotify.com/.well-known/assetlinks.json .
- Dados dos dois apps limpos antes da rodada final. Sem conta autenticada, sem
  reprodução, permissões de notificação concedidas, rede habilitada. A tela Tonal
  está no início e a oficial na apresentação/login; em segundo plano ambos recebem
  Home. O app concorrente e o APK de instrumentação ficam encerrados.
- Dez segundos de estabilização; seis amostras separadas por três segundos nos
  testes sem reprodução. Local/efeitos: dez segundos e seis intervalos de quatro
  segundos, para terminar antes do fim da amostra de áudio. O intervalo efetivo,
  incluindo consultas ADB, é gravado em cada linha.
- CPU: delta de utime+stime em `/proc/PID/stat`, dividido por CLK_TCK e pelo tempo
  monotônico. Soma dos processos nomeados do pacote. A janela após abertura pode
  incluir tarefas iniciais esporádicas; não é uma medição estacionária por vários
  minutos. PSS: soma de TOTAL PSS em
  `dumpsys meminfo --local -s -S`, sem chamar/acordar o processo congelado e
  registrando SwapPSS à parte. Custo de serviços compartilhados não é atribuído
  integralmente ao app. Builds, compiladores e daemon Gradle estavam parados.
- Uma primeira rodada encontrou atualização visual desnecessária no Tonal.
  A versão final evita redefinir textos iguais, reduz o timer em repouso e suspende
  atualização visual/polling em segundo plano. Os resultados anteriores estão
  separados em `out/android/measurements` e não entram nesta tabela.

## Limites e conclusão prática

Há uma vantagem clara de RAM para esta interface e este conjunto reduzido de
recursos. O Spotify oficial tem muito mais funcionalidades. A redução não deve
ser projetada automaticamente para uma versão Tonal com todos esses recursos,
para uso autenticado, nem para reprodução da mesma música.

A sessão Premium do Windows foi reutilizada somente no pacote de testes. Web API
e cache estavam disponíveis, mas o motor Android recusou a reprodução: primeiro
houve falhas de credenciais/identidade e a última tentativa terminou em HTTP 400,
sem frames PCM. O login oficial foi tentado com os códigos fornecidos, sem sucesso;
o cadastro gratuito alternativo não foi concluído nem criou uma conta. Conforme
o foco definido pelo usuário, a comparação final dispensou login. Não houve
streaming comparável, portanto nenhuma economia de CPU/RAM em streaming foi provada.

O APK entregue é útil para testar interface, arquivos locais, DSP e consumo no
POCO X7 Pro. O streaming permanece experimental e não validado. HRTF/IAMF e o grafo
FFmpeg do Windows não estão implementados nesta prévia. O telefone é ARM64; o teste
foi x86_64 no emulador, sem medir bateria, temperatura ou o aparelho físico.

APK: `out/android/Tonal-Android-0.1.0-local.apk`, 19.44 MiB.
SHA-256: `508dade15f47a7a8a16b27f1f73980f4d900ae10ec5acb6422cf5f767497ef28`.
ABIs ARM64 e x86_64; alinhamento ELF e ZIP de 16 KB, assinatura e ausência dos tokens
de teste verificados. Licenças incluídas. Build e lint concluídos; lint com zero
erros e 13 avisos de localização/compatibilidade da configuração de backup antiga.

Para instalar/testar e reproduzir os builds: [ANDROID_LOCAL.md](ANDROID_LOCAL.md).
Coleta: `packaging/benchmark-android-local.ps1` e `tests/android/measure.py`.
Dados brutos: `out/android/measurements-final`; resumo preservado neste diretório.
Tudo local, sem commit, push, PR ou publicação no GitHub.
