# Changelog

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
