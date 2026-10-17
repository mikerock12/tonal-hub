# Motor de áudio — implementação local

Implementação baseada em `GUIA_IMPLEMENTACAO_AUDIO_CPP_FFMPEG_OPENAL_IAMF.md`.
Nenhum commit, push, PR ou publicação foi feito. O código permanece no working tree local.

## Como testar

Abra `out/build/release/Tonal.exe` e use **Ctrl+E** ou o menu da conta → **Áudio e efeitos…**.
O painel oferece EQ de 10 bandas, presets, graves 0–12 dB, compressor, normalização,
diagnósticos e abertura de arquivos. Duplo clique zera uma banda. O reset geral pede confirmação.

Para usar apenas arquivos locais, sem carregar o login do Spotify:

```powershell
.\out\build\release\Tonal.exe --local-only --audio-settings
# Ou abra um arquivo diretamente:
.\out\build\release\Tonal.exe --local-only --local-file "D:\Musicas\faixa.flac"
```

Também foi criado `out/iniciar-audio-local.cmd`. A barra de reprodução existente controla
play/pause, seek e volume do arquivo aberto. Há um arquivo local por vez; próxima/fila,
shuffle, biblioteca e playlists continuam sendo recursos do Spotify.

No Spotify, os efeitos são aplicados **somente quando a reprodução ocorre no Tonal deste
computador**. Celular e outros dispositivos Connect mantêm seu próprio áudio. O streaming
do Spotify continua estéreo; ele não fornece IAMF para esta integração.

## Arquitetura auditada e preservada

O aplicativo usa Qt Quick/QML, CMake/Ninja e MSVC. `Player` mantém o estado, os comandos,
as playlists e a comunicação com `SpotifyApi`. `LibrespotProcess` originalmente abria
sozinho a saída de áudio; não havia PCM disponível ao C++ do Tonal.

Agora há duas entradas para a mesma cadeia:

```text
Spotify → librespot --backend pipe --format F32 → pipe nativo limitado
                                                     ↓
arquivo → FFmpeg / IAMF+OAR → float32 + ordem de canais → libavfilter → OpenAL Soft
```

- `NativePcmPipe`: pipe anônimo do Windows, com backpressure. O consumo acompanha a fila
  OpenAL; o QProcess/UI não acumula PCM em memória. Logs permanecem em stderr.
- O primeiro OAuth usa o backend original, pois o librespot imprime instruções em stdout.
  Com credenciais salvas, o motor reinicia para usar o pipe PCM.
- `AudioEngine`: recebe comandos da UI e executa leitura, análise e DSP em uma worker.
  A mixagem/saída crítica ocorre nas threads internas do OpenAL. Objetos FFmpeg/OpenAL
  nunca são alterados pela UI. Filas, seek, EOF e mudanças de fonte são coordenados.
- `Player`, `MediaControls`, `Lyrics`, autenticação e navegação Spotify foram preservados.
  O adapter local redireciona os controles existentes enquanto um arquivo local está ativo.
- Se o backend avançado falhar no Spotify, o processo reinicia no backend original,
  aproveitando o mecanismo existente de retomada. Erros locais são apresentados sem
  encerrar o aplicativo. Desconexão de dispositivo provoca tentativa de reabertura.

## DSP e persistência

A cadeia permanece aquecida: ganho de faixa → 10 peaking EQs → bass shelf → compressor
opcional → limitador de picos de amostra. Sliders usam comandos runtime verificados do
FFmpeg, com rampas de 30 ms em sub-blocos de até 256 frames. Eles não recriam o grafo.

O bypass devolve exatamente os floats de entrada quando todos os efeitos e ganhos
estão desligados. Ao usar efeitos, o limitador acrescenta 5 ms de lookahead; a cauda é
drenada no fim. Seu teto é de **pico de amostra**, não uma garantia de true peak.
O volume master é aplicado uma vez: mixer do librespot no Spotify, ganho OpenAL nos
arquivos locais. O medidor mostra PCM **antes** desse volume master.

Arquivos locais priorizam ReplayGain (tags ou side data). Na ausência, `ebur128` mede
LUFS e true peak em uma passagem anterior à reprodução; o resultado é guardado em cache.
O alvo LUFS é usado nessa análise; ReplayGain mantém o ganho declarado pela gravação.
O ganho de análise respeita o headroom medido. Não há `dynaudnorm` disfarçado de normalização.
No Spotify, a normalização usa o próprio librespot, sem uma segunda normalização no DSP;
alterar essa opção reinicia o motor. EQ, graves e compressor não reiniciam o librespot.

Preferências ficam em `QStandardPaths::AppConfigLocation/audio.json`, usando QSaveFile,
schema versionado, validação de finitude e limites. Caches de loudness e renderização IAMF
ficam no CacheLocation do usuário. Os testes usam armazenamento separado.

## Espacialização e IAMF

- **Original:** estéreo direto; sem upmix ou alegação de surround para música estéreo.
- **HRTF:** habilitado por capacidade real, para mono, estéreo (inclusive Spotify), quad,
  5.1 e 7.1. Estéreo usa dois alto-falantes virtuais a ±30°, com os canais preservados;
  isso não cria objetos ou uma mixagem multicanal. Fontes multicanal ficam na posição
  do ouvinte para manter a direção de cada canal. O modo Original mantém estéreo direto.
  A UI informa o estado efetivo de `ALC_SOFT_HRTF`.
  Trocar Original/HRTF usa `alcResetDeviceSOFT`, preservando a fonte, a posição e
  os buffers de áudio; a transição recebe fade-out/fade-in.
- **IAMF:** feature flag `TONAL_ENABLE_IAMF`. A revisão fixada do libiamf já chama OAR;
  não há outro renderizador sobre o PCM final. O arquivo é renderizado uma vez para
  um cache WAV binaural 48 kHz/32 bits, depois reproduzido com seek pelo FFmpeg.
  OpenAL recebe esse PCM com HRTF explicitamente desligado. Não há head tracking.

O suporte validado nesta entrega é **bitstream standalone `.iamf`**, com amostras sintéticas
Opus/SIMPLE estéreo e 5.1. O decoder também foi compilado com PCM, AAC e FLAC, mas estes
codecs dentro de IAMF não têm a mesma cobertura de testes. Não se declara conformidade
geral com todos os perfis de IAMF/Eclipsa. A revisão, o capability string e os testes são
a referência da compatibilidade efetivamente exercitada.

IAMF em MP4 não foi integrado ao adapter: o FFmpeg identifica os grupos, evita reproduzir
um substream isolado como se fosse a mixagem e tenta uma trilha convencional alternativa.
Sem alternativa, informa o formato incompatível. Arquivos comuns não habilitam IAMF.
Layouts personalizados/ambíguos são recusados em vez de embaralhar os canais.

Pausa e retomada usam uma rampa de ganho de 30 ms. A pausa local preserva os buffers e
suspende a fonte após um intervalo adicional de mixer a ganho zero. No Spotify, a saída
fica silenciosa, mas continua consumindo PCM até o librespot pausar, evitando bloquear
o produtor no pipe. Comandos de transporte do librespot são lidos separadamente no stderr
(log debug apenas do módulo player), inclusive para controles por Spotify Connect.
Pausar não faz mais flush. Seek, troca de fonte e fechamento reduzem o ganho antes de
descartar buffers; o início da nova fonte também recebe uma rampa.

## Dependências e build reproduzível

`third_party/versions.lock` fixa o pacote FFmpeg (release datada, SHA256 e DLL hashes),
OpenAL, libiamf, OAR e dependências transitivas. `setup-audio-local.ps1` só baixa/lê os
repositórios externos e modifica o cache local; não envia nada a eles.

```powershell
# Qt 6.8+ MSVC, CMake/Ninja, VS C++ e librespot devem estar configurados.
powershell -NoProfile -ExecutionPolicy Bypass -File .\packaging\setup-audio-local.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\packaging\build-local.ps1 -Test
# Desabilitar a integração IAMF na compilação, mantendo o restante:
powershell -NoProfile -ExecutionPolicy Bypass -File .\packaging\build-local.ps1 -WithoutIamf
```

`out/compilar.cmd` delega ao build local. Notices são copiados para
`out/build/release/licenses/audio`. Há uma checagem de compilação do adapter com IAMF
desabilitado. Leia também `third_party/AUDIO-NOTICES.md`; esta não é uma publicação comercial.

Um snapshot portátil, exclusivamente para testes locais, pode ser montado com
`packaging/package-audio-local.ps1 -Zip`. O destino fica em `out/release-local` e não
executa publicação nem usa a opção `-Publish` do script de releases existente.

## Validação e limites dos resultados

Testes reproduzíveis geram suas próprias senoides/arquivos, sem baixar músicas:

- Bypass float32 bit a bit; resposta de +3 dB em 1 kHz; graves +12 dB com limitador;
  compressor; atualização contínua dos filtros sem perder frames e retorno ao bypass.
- WAV, MP3, FLAC, AAC; 44,1/48/96 kHz; canais 5.1/7.1; ReplayGain; seek e entradas inválidas.
- EBU R128, cache e ganho entre faixas de amplitudes diferentes.
- Fila OpenAL, pausa/flush; HRTF real e sinal cruzado binaural de impulsos estéreo e 5.1;
  bloqueio de segundo HRTF para PCM binaural.
- PCM renderizado via loopback pelo mesmo backend: pausa/retomada sem salto abrupto,
  reversão rápida da rampa, preservação dos buffers e consumo contínuo no mute do Spotify.
- Pipe QProcess/Windows com floats conhecidos, backpressure e separação dos logs.
- IAMF estéreo/5.1 → OAR → WAV; cache/seek e rejeição de entrada inválida.
- Ciclo completo do AudioEngine: alterações ao vivo, pausa, seek, EOF/replay e troca de fonte.
- Smoke test Qt offscreen: painel aberto, dez bandas instanciadas, seleção de HRTF em estéreo e reprodução progredindo,
  sem carregar o login real nem usar dispositivo de som real.

O PCM da amostra 5.1 gerado pelo adapter foi comparado, bit a bit, ao `iamfdec` da mesma
revisão (`-sb -d 32 -l 0 -disable_limiter`). Ambos produziram 144.000 frames estéreo/48 kHz;
SHA256 do PCM: `80ed26f1c5dc6bcdba3e4865169714221e053a4c6c9f2d06231c758ea1860c2d`.

O runner imprime benchmark de DSP para blocos de 512 frames, inclusive p95; valores variam
com carga da máquina. Isso não mede latência fim a fim nem percepção auditiva.

**Validação manual ainda necessária:** revisão visual final (a ferramenta de captura da
janela não recebeu autorização a tempo), audição com seus fones, reprodução Spotify com
a sua conta, hotplug USB/Bluetooth/HDMI e suspensão real. ASan, teste prolongado de memória
e comparação CPU/memória com a versão anterior não foram executados. Não se promete
Dolby Atmos, objetos 7.1.4 em estéreo ou ganho subjetivo para todas as gravações.
