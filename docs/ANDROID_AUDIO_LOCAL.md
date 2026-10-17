# Tonal Android 0.3.0-local — áudio completo e Bluetooth

Implementação local de 15/10/2026. Nenhum commit, push, PR ou publicação.

O Android passa a usar os mesmos componentes e parâmetros principais do áudio
de desktop: FFmpeg/libavfilter e OpenAL Soft. A saída física continua sendo um
AudioTrack em MEDIA/MUSIC; o OpenAL trabalha em loopback, gerando PCM
estéreo. Na troca de faixa, a saída anterior pode coexistir durante o fade
de 50 ms, sem outro decoder ativo alimentando-a.

## Recursos

| Controle | Android |
|---|---|
| Bypass mestre | Sim; estéreo finito sem efeitos evita filtros e cópias JNI de PCM |
| EQ de 10 bandas | 31/62/125/250/500/1k/2k/4k/8k/16k Hz, ±12 dB, passos de 0,5 dB |
| Ativar/zerar EQ | Independente dos demais efeitos |
| Presets do PC | Flat, Rock, Pop, Clássica, Eletrônica, Vocal |
| Graves | Low shelf FFmpeg, 110 Hz, Q 0,707, 0–12 dB |
| Compressor | FFmpeg, threshold 0,125; ratio 2; attack 20 ms; release 250 ms |
| Normalização Spotify | Ganho Basic constante por faixa, com proteção por pico; reconexão explícita para aplicar |
| Normalização local | ReplayGain prioritário; análise EBU R128/libebur128 na ausência de tags |
| Alvo local | −24 a −10 LUFS; padrão −16 LUFS |
| Teto de amostras | −6 a −0,1 dBFS; padrão −1 dBFS; limiter lookahead 5 ms |
| HRTF real | OpenAL Soft/SADIE, mono/estéreo/quad/5.1/7.1 para fones |
| IAMF binaural | libiamf/OAR com Opus, FLAC, AAC e PCM; arquivos IAMF brutos compatíveis |
| Arquivos locais | Decodificação FFmpeg para WAV float canônico, inclusive multicanal |
| Perfis por saída | Automático, fones/headphones, carro e caixa de som; EQ/efeitos/buffer salvos por saída |

Ganhos e mistura wet/dry usam transições de 30 ms. O limiter depois da
virtualização protege também os picos gerados pelo HRTF, sem usar hard clipping
como efeito. O bypass não limita o PCM original. Não há garantia de true peak
no receptor após resampling, efeitos externos ou codificação Bluetooth.

IAMF precisa existir no arquivo: não é criado a partir de música estéreo do
Spotify. O arquivo IAMF é renderizado pelo OAR para binaural 48 kHz e marcado
para não passar por uma segunda HRTF. Não é Dolby Atmos, não há head tracking,
e não se promete compatibilidade com todo perfil IAMF ou IAMF dentro de MP4.

Arquivos locais são preparados e analisados fora da UI e da thread de áudio.
A primeira abertura pode demorar enquanto o arquivo é convertido/analisado.
Entrada importada limitada a 512 MiB, PCM preparado a aproximadamente 1,8 GiB;
os caches anteriores são descartados, preservando o arquivo ainda em uso.
O cache usa disco, não mantém a música inteira na RAM. ReplayGain usa referência
aproximada de −18 LUFS para ajustar o alvo; o ganho é limitado pelo pico medido.

## Bluetooth: o que foi otimizado

- Buffers Equilibrado, Estabilidade e Menor buffer. Se há underrun, o buffer
  cresce em passos de aproximadamente 20 ms, até o limite de 250 ms/capacidade.
- Solicitação de dispositivo de saída através da API pública, com diagnóstico
  da **rota realmente usada** durante a reprodução.
- Preferências por identidade de saída (hash de tipo/endereço/nome, sem publicar
  endereço Bluetooth em logs). Quando o sistema omite o endereço, modelos com
  o mesmo nome podem compartilhar um perfil.
- Recuperação de `AudioTrack.ERROR_DEAD_OBJECT`, sem exigir novo login Spotify.
- Pausa/retomada com VolumeShaper de 30 ms, proteção contra callbacks de pausa
  atrasados e escoamento dos filtros/buffer no fim de arquivos. Retomar Spotify
  preserva AudioTrack/filtros e o PCM enfileirado, sem reabrir a saída a cada play.
- Prioridade de áudio nas threads de PCM. Sem modo de chamada/SCO, microfone,
  root, APIs ocultas ou alteração global do áudio do aparelho.
- Espacialização automática do Android desativada para essa saída do app,
  evitando processamento adicional sobre Original/HRTF/IAMF.

Valores solicitados de buffer: Bluetooth 60/140/40 ms; demais saídas
40/80/20 ms (equilibrado/estável/menor). O piso de segurança é o maior entre
80 ms e o mínimo recomendado pelo AudioTrack acrescido de 40 ms, limitado à
capacidade alocada. Essa margem permite concluir o fade de pausa de 30 ms
depois que o decoder para de alimentar PCM. Menor buffer usa esse piso;
Equilibrado acrescenta 20 ms e Estabilidade acrescenta 60 ms. A capacidade
máxima do dispositivo ainda pode igualar perfis. Pausa e fechamento são agendados em uma
thread de controle de áudio, independente da interface; não há polling nela.
O painel mostra o tamanho efetivo. **Buffer do app não é latência total**:
codec, rádio, interferência, firmware e o buffer do receptor acrescentam atraso.
Não há controle privilegiado sobre LDAC/aptX/SBC nem promessa de eliminar falhas
de rádio. Para fones Bluetooth A2DP, selecione explicitamente o perfil Fones
antes de usar HRTF; para carro/caixa o perfil mantém estéreo.

## Volume no carro e continuidade de internet

O motor usa ganho digital unitário (`NoOpVolume`); o volume de mídia continua
sob controle do celular/receptor. Foi removida a segunda atenuação logarítmica
do SoftMixer, cuja configuração inicial era 50%. Não há ganho artificial global
nem alteração automática do volume do carro.

A normalização Spotify mudou de Dynamic para Basic: um ganho constante por
faixa, reduzido quando necessário para respeitar o pico fornecido pelo Spotify.
Novas saídas começam Flat, sem compressor; configurações explicitamente salvas
por saída são preservadas. No carro, use Original e compressor desligado se
desejar preservar a dinâmica. O compressor permanece disponível como efeito.

O downloader solicita aproximadamente 60 segundos à frente da posição atual
(estimativa pelo bitrate), com espera inicial de 2 segundos. É uma meta de
pré-carregamento, não uma garantia de duração quando a internet é lenta.
Cache de arquivos de áudio criptografados, privado do app, com limite de
256 MiB e descarte dos mais antigos; música inteira não fica na RAM. Durante
uma faixa, bytes parciais recebidos são mantidos mesmo se o HTTP cair. As
faixas completas podem ser reaproveitadas nas próximas reproduções enquanto
permanecerem no cache; autenticação/metadados/chaves ainda podem exigir rede.
Isto não implementa downloads offline garantidos de playlists.
Temporários órfãos de processos anteriores são removidos antes da primeira
conexão do motor no novo processo, preservando cache completo e credenciais.

Quedas da sessão de controle reconectam com backoff, preservando Player,
decoder, posição, fila e ranges já baixados. Pause/play funcionam durante
as tentativas. Se a reserva acaba antes do final real da faixa, a faixa não
é pulada: há tentativas de retomada a partir da última posição decodificada,
respeitando a intenção de pausa. Uma reserva esgotada pode causar silêncio
até os dados voltarem; nenhum buffer finito elimina quedas prolongadas.

Patches Android de `librespot-core`, `librespot-audio`, `librespot-playback` e `librespot-connect`
ficam em `android/native/vendor`, sob as licenças originais; Cargo.lock aponta
para essas cópias locais. A proteção de playback restaura o decoder caso uma
busca de posição falhe por falta de rede, permitindo nova tentativa sem estado
inválido/encerramento do processo. Eles não alteram o motor do desktop.

## Pesquisa de componentes

Fontes primárias consultadas em 15/10/2026:

- [OpenAL Soft](https://github.com/kcat/openal-soft): escolhido para reutilizar
  HRTF do PC e gerar binaural antes do transporte Bluetooth. LGPL-2.0-or-later;
  banco SADIE sob Apache-2.0. É DSP, não modifica o rádio Bluetooth.
- [FFmpeg](https://ffmpeg.org/ffmpeg-filters.html): filtros, codecs e formatos
  locais; build mínimo compartilhado, LGPL-3.0-or-later, sem GPL/nonfree.
- [libebur128](https://github.com/jiixyj/libebur128): medição EBU R128 e pico,
  MIT; análise local fora da reprodução.
- [libiamf](https://github.com/AOMediaCodec/libiamf) e
  [OAR](https://github.com/AOMediaCodec/oar): renderização de conteúdo IAMF.
- [Oboe](https://github.com/google/oboe) e
  [AAudio](https://developer.android.com/ndk/guides/audio/aaudio/aaudio): avaliados.
  São bons componentes de áudio de baixa latência, mas não dão a um APK controle
  de codec/buffer interno do receptor Bluetooth. Nesta versão foi mantido o
  AudioTrack existente para preservar MediaSession, foco e transporte já testados.
- [JamesDSP](https://github.com/james34602/JamesDSPManager): alternativa de DSP
  de sistema avaliada; não foi adicionada uma camada de efeitos globais/root.
- [BluetoothA2dp no AOSP](https://android.googlesource.com/platform/packages/modules/Bluetooth/+/refs/heads/main/framework/java/android/bluetooth/BluetoothA2dp.java):
  `setCodecConfigPreference` é API de sistema/oculta e exige
  `BLUETOOTH_PRIVILEGED`. Não é uma capacidade disponível a este APK comum.
- [AudioTrack](https://developer.android.com/reference/android/media/AudioTrack)
  e [Spatializer](https://developer.android.com/media/grow/spatial-audio): APIs
  públicas de buffer, rota, recuperação e prevenção de espacialização duplicada.
- [VolumeShaper](https://developer.android.com/reference/android/media/VolumeShaper):
  envelope de volume para a transição de pausa/retomada.

## Build local

O ambiente instalado neste projeto usa SDK Android 36, NDK 27.0.12077973,
JDK 17, Gradle 8.14.3, Rust com alvos aarch64-linux-android/x86_64-linux-android,
CMake 3.31.10 e Ninja. Fontes/revisões em `third_party/android-audio.lock`;
os codecs/OAR/Eigen/Abseil vêm do cache de fontes já usado pelo desktop.

```powershell
# FFmpeg: MSYS2 apenas dentro de out/tools/android-build/msys64
# Dependências MSYS2: make gcc diffutils (assinaturas pacman verificadas).
$env:MSYSTEM='MSYS'
& ./out/tools/android-build/msys64/usr/bin/bash.exe --noprofile --norc ./packaging/build-android-ffmpeg-local.sh

# Rust + plugin C++ + APK, sem Git remoto
./packaging/build-android-local.ps1
# Para reutilizar o Rust compilado:
./packaging/build-android-local.ps1 -SkipNative
# APK menor para POCO X7 Pro (ARM64), reutilizando bibliotecas compiladas:
./packaging/build-android-local.ps1 -SkipNative -SkipAudio -PhoneOnly
```

As adaptações Android ficam em CMake/arquivos gerados: TLS POSIX para API 28,
shim de timespec_get no OAR, um fill equivalente no FlexArray OpenAL, módulos
C++20 desativados e stub FDK inline em cópia própria. Fontes do PC preservadas.
FFmpeg/OpenAL são `.so` separados, substituíveis/recompiláveis. Licenças no APK
em assets/licenses/audio e fontes correspondentes no pacote local de fontes.
O APK de teste usa assinatura local; não contém sessões/códigos de login.

## Validação

Resultados registrados em `out/android/audio-features-0.3.0`,
`out/android/audio-panel-0.3.0` e `out/android/audio-stability-0.3.0`.
A suíte real de sinal passou: bypass, EQ/graves/compressor/limiter, rampas,
HRTF contínua entre blocos de 257 frames, multicanal e ausência de segunda HRTF
em fonte já binaural. Doze fixtures incluem WAV 44,1/96 kHz, FLAC, MP3, AAC,
ReplayGain, 5.1/7.1 e IAMF estéreo/surround; arquivo inválido e cancelamento
também verificados. Todas as seções do painel foram alcançadas em 360×640 dp.

Streaming Spotify real, sessão Premium autorizada somente no sandbox benchmark,
AVD Android 16/API 36 x86_64, 4 vCPUs/4096 MB, GPU host. Medições sem Cargo,
rustc ou daemon Gradle ativos, interface visível, consultas de posição/snapshot
e o mesmo app de instrumentação nas três fases. Cada janela dura cerca de 26 s:

| Reprodução | CPU (% de um núcleo) | RAM PSS (MiB) | DSP médio por bloco | Underruns na janela |
|---|---:|---:|---:|---:|
| Flat estéreo | 3,97 | 67,1 | 119 µs | 0 |
| EQ Rock e graves | 4,47 | 69,1 | 175 µs | 0 |
| EQ Rock, graves e HRTF | 6,05 | 74,5 | 333 µs | 0 |

Não houve blocos acima de sua duração de PCM nas três janelas finais.
Essas janelas curtas não permitem prometer ausência absoluta de cortes sob toda carga.
Seis ciclos de pausa/retomada passaram, com envelope 0→1 restaurado; recuperação
de saída morta injetada, troca de buffers, bypass e segundo plano passaram.
O teste específico `audio-transitions-0.3.0` também passou: seis ciclos nos três
perfis, sem novos underruns durante pausa/retomada. Nesta AVD, os tamanhos
efetivos foram 6.658/8.422/5.776 frames (Equilibrado/Estabilidade/Menor),
aproximadamente 151/191/131 ms. O valor físico do POCO será diferente e aparece
no diagnóstico.
O Spotify oficial não reproduziu a mesma música na rodada histórica: não há
comparação equivalente que prove economia percentual em streaming.

Rede: `out/android/audio-network-0.3.0/result.json` passou. Durante 25 segundos
com Wi-Fi e dados desligados, PCM continuou crescendo e não houve novos
underruns; pausa offline foi mantida após a volta da rede. Na rodada com Wi-Fi
limitado a 1 Mbit/s e cache completo removido somente do pacote benchmark,
o áudio baixado continuou por aproximadamente 38 segundos após cortar a rede.
O buffer esgotou; depois da volta da rede, houve reconexão de controle e retomada
automática da mesma faixa perto da posição efetivamente decodificada.
O underrun esperado do esgotamento foi registrado, sem crash ou novo login.

A tentativa equivalente pela rota celular da AVD conectou o controle, mas
não entregou o primeiro áudio do CDN dentro do prazo. Essa tentativa não valida
handover Wi-Fi/celular. O resultado aprovado usou a rota Wi-Fi do emulador;
troca real Wi-Fi/5G, volume relativo ao Spotify, codec, estalos audíveis e
latência Bluetooth continuam exigindo o POCO X7 Pro e os receptores reais.

O emulador pode verificar DSP,
decoders, controles, reprodução, buffers e consumo do processo. Ele **não valida**
codec de um fone, qualidade audível, interferência nem atraso físico Bluetooth.
Esses itens exigem o POCO X7 Pro e os dispositivos reais.

## Pacotes e relatório final

Resultados e hashes: [android-audio-tests.json](android-audio-tests.json).
APK para POCO X7 Pro: `out/android/Tonal-Android-0.3.0-arm64-v8a.apk`, 22,33 MiB.
O APK universal `Tonal-Android-0.3.0-local.apk` também inclui x86_64 para emulador.
Ambos tiveram assinatura local, nove bibliotecas por ABI, alinhamento de 16 KB
e ausência das credenciais temporárias verificados; 698 arquivos de licenças.
Lint final: zero erros, 17 avisos; detalhes no relatório gerado pelo Gradle.

Instale por cima da versão anterior para preservar o login. No carro, comece
com perfil Carro, Original/Flat e compressor desligado; compare a normalização
com a mesma configuração no Spotify oficial. O ganho interno agora é unitário.

Fontes correspondentes: `out/android/Tonal-Android-0.3.0-audio-sources.tar.gz`;
hash e tamanho em `out/android/audio-sources-0.3.0.json`. Todos os artefatos
permanecem locais, sem commit, push ou PR.
