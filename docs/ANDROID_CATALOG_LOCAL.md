# Tonal Android 0.2.0 — catálogo Spotify local

O erro **Spotify HTTP 400: Invalid limit** vinha de `search?limit=20`.
A consulta de busca agora usa **10 resultados por tipo**, como a versão desktop.
O limite antigo foi reproduzido contra a API real; o novo retorna HTTP 200.

## Interface e navegação

- Página inicial com saudação, atalhos de playlists e músicas curtidas, capas de
  álbuns recentes, álbuns salvos e artistas frequentes.
- Busca por **músicas, artistas, álbuns e playlists**, com filtros, capas, duração,
  estado vazio, carregamento, tentativa novamente e páginas adicionais.
- Página do artista com músicas e discografia separada em álbuns, singles/EPs e
  coletâneas. As músicas vêm da busca por artista, filtradas pelo ID do artista;
  não dependem do endpoint antigo de top tracks.
- Página de álbum/playlist com faixas e reprodução por contexto, inclusive uma
  faixa escolhida em página adicional. Resultados e curtidas usam fila de URIs.
- Biblioteca com playlists, álbuns salvos, artistas seguidos e músicas curtidas.
- Faixas indisponíveis ficam atenuadas e identificadas. Filas de busca/curtidas
  excluem itens marcados como não reproduzíveis pela API; contextos mantêm a ordem
  original do álbum/playlist, inclusive quando há músicas repetidas.
- Mini-player permanente; controles ampliados ao tocar no título; posição,
  anterior/próxima, pausa, aleatório e repetição. Configurações de áudio no topo.
- Voltar mantém o caminho artista/álbum/busca. Android 13+ usa
  `OnBackInvokedCallback`; Android 9–12 mantém o callback compatível.

A interface segue a linguagem visual do desktop, adaptada ao toque e à largura do
telefone. Continua sendo uma implementação Android Java nativa com Rust/JNI e
AudioTrack. Não usa WebView ou o executável Qt do Windows. Edição de playlists,
downloads, letras, HRTF e IAMF continuam fora deste APK.

## Autorização e limites

As sessões existentes são preservadas pela atualização. As permissões da
biblioteca agora incluem histórico, artistas seguidos e artistas frequentes.
Sessões antigas sem esses escopos podem exigir **Conta → Atualizar autorização da
biblioteca**. Áudio e biblioteca continuam com autorizações separadas.

Links de paginação são aceitos somente em `https://api.spotify.com/v1/`.
Capas são carregadas sem token, com limite de download e cache de bitmap de 6 MiB.
Respostas antigas são descartadas ao mudar de tela. Atualizações de player param
quando a atividade fica em segundo plano; o serviço continua reproduzindo.

O Spotify ainda pode restringir conteúdo por conta, região ou permissões do
aplicativo. Esses limites são tratados como erro visível, sem resultados falsos.

## Reprodução dos testes locais

```powershell
.\packaging\build-android-local.ps1
# Compile também assembleBenchmark pelo Gradle configurado neste computador e
# instale android/app/build/outputs/apk/benchmark/app-benchmark.apk na AVD local.
# O APK benchmark é separado e aceita somente a semente local de teste.
.\packaging\test-android-session-local.ps1 -Import
python tests/android/catalog_api_local.py
python tests/android/catalog_smoke.py
python tests/android/catalog_navigation_smoke.py --output out/android/catalog-navigation-0.2.0
python tests/android/audio_smoke.py --output out/android/audio-smoke-0.2.0
python tests/android/verify_apk.py --apk out/android/Tonal-Android-0.2.0-local.apk `
  --output out/android/apk-validation-0.2.0.json
```

O teste de catálogo utiliza uma conta Premium autorizada e faz as ações na
interface pelo ADB. A semente fica apenas no sandbox do pacote benchmark e no
diretório local ignorado `out/android/session-private`; não entra no APK entregue.

O APK `out/android/Tonal-Android-0.2.0-local.apk` contém `arm64-v8a` (POCO X7 Pro)
e `x86_64` (emulador), Android 9+, mesmo identificador e certificado local da
versão 0.1.1. O empacotamento mantém alinhamento ELF e ZIP de 16 KiB.
As medições de CPU/RAM da 0.1.0 permanecem históricas e não medem esta interface
com catálogo autenticado. Qualidade audível e consumo no POCO exigem o aparelho.

Código, testes e APK permanecem locais, sem commit, push ou PR.

## Evidências

- Contrato real da API: `out/android/catalog-api-0.2.0.json`.
- Navegação de catálogo: `out/android/catalog-smoke-0.2.0/result.json` e capturas
  das telas, incluindo estado vazio, artista, álbum e as quatro buscas.
- Player e biblioteca: `out/android/catalog-navigation-0.2.0/result.json`.
- Empacotamento e ausência de credenciais: `out/android/apk-validation-0.2.0.json`.
- Assinatura: `out/android/apk-signature-0.2.0.txt`.

As rodadas iniciais incluíram desconexões do motor e falhas de sincronização da
automação. A AVD foi fixada em Wi-Fi; o player agora descarta respostas de posição
anteriores à seleção e pausa/mostra desconexão quando a sessão termina. Tocar em
uma música permite reconectar. A interface não mostra “reproduzindo” após perder
a sessão. A matriz final está em `docs/android-catalog-tests.json`.

A rodada final passou nas seis suítes: contrato real da API (22 verificações),
catálogo (19), playlists/player (6), tela de 360 × 640 dp (4), streaming com
reinício e áudio local/DSP. O seek, aleatório e repetição foram confirmados pelo
estado real do Spotify. O áudio local e o streaming continuaram em segundo plano;
o contador PCM permaneceu estável durante a pausa e avançou ao retomar.
O `lintRelease` terminou com zero erros e 15 avisos, sobretudo internacionalização
dos textos. A assinatura de atualização e os dois ABIs passaram na validação.

O estado do player é associado ao ID do próprio motor; outro dispositivo também
chamado “Tonal Android” não substitui o que aparece nesta instância.
Diagnóstico sanitizado do motor é registrado somente pelo APK benchmark.
