# Changelog

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
