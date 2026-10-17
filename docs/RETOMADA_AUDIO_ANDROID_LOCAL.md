# Concluído localmente — 15/10/2026

Implementação Android 0.3.0-local concluída e testes finais aprovados.
Não houve commit, push, PR nem publicação. Trabalho anterior do desktop preservado.

APK POCO X7 Pro: out/android/Tonal-Android-0.3.0-arm64-v8a.apk.
Universal: out/android/Tonal-Android-0.3.0-local.apk.
Fontes: out/android/Tonal-Android-0.3.0-audio-sources.tar.gz; conferir SHA256 em
out/android/audio-sources-0.3.0.json antes de distribuir.
Guia: docs/ANDROID_AUDIO_LOCAL.md; resultados/hashes: docs/android-audio-tests.json.

FFmpeg DSP/decoders, OpenAL Soft HRTF contínua, IAMF/OAR binaural, ReplayGain/R128,
presets do PC, perfis por saída, buffers adaptativos, recuperação AudioTrack e fades.
Spotify com ganho digital unitário, normalização Basic constante por faixa,
cache privado criptografado limitado a 256 MiB e solicitação de 60 s à frente.
Ranges HTTP parciais preservados; sessão de controle reconecta preservando
Player/decoder/fila/cache; esgotamento retoma a mesma faixa no PCM real.
Patches Android: vendor/librespot-core, audio, playback e connect.

Pausa: VolumeShaper 30 ms com curva 0→1, REVERSE na pausa e PLAY na retomada,
HandlerThread de controle separado da UI. Experimento replace/join removido.
Piso: maior entre 80 ms e mínimo recomendado AudioTrack + 40 ms; Equilibrado
acrescenta 20 ms, Estabilidade 60 ms, Menor usa piso. Nesta AVD: 151/191/131 ms.
Seis pausas nos três perfis passaram sem novos underruns. Streaming final:
3,97–6,05% de um núcleo, 67,1–74,5 MiB PSS, zero underruns nas janelas medidas.

Passaram: catálogo completo, busca/álbum/artista/paginação/fila/next/seek,
biblioteca, 12 fixtures/codecs/HRTF/IAMF/cancelamento, painel pequeno, streaming,
recuperação de saída, background e áudio local. Rede: 25 s offline sem novos
underruns; pausa offline preservada após reconectar; buffer esgotado na rede
Wi-Fi limitada, mesma faixa retomada perto de 38 s. A rota celular da AVD não
entregou o áudio inicial do CDN e não valida handover Wi-Fi/5G real.
Sem comprovação auditiva, Bluetooth físico, bateria ou comparação equivalente
de streaming contra o Spotify oficial. Validar no POCO/receptores reais.

Credenciais temporárias: seed do host removida e SOMENTE dados do pacote
app.tonal.android.benchmark limpos. Cofre/cache Windows originais preservados.
SOMENTE AVD Tonal_Local_20261015 encerrada; nenhuma Converter_* alterada.

Bibliotecas FFmpeg/C++ já compiladas para ambas ABIs; não repetir configure longo.
CMake correto 3.31.10 em out/tools/cmake331; NDK 27.0.12077973. Build Java/assets:
packaging/build-android-local.ps1 -SkipNative -SkipAudio; -PhoneOnly gera ARM64.
Verificar universal antes de ARM64: verify_apk compara intermediários release.
Fixtures: adb push + run-as cp + SHA256; stdin binário Windows trunca em Ctrl-Z.
