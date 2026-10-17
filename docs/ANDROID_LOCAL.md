# Tonal Android — prévia local

Esta é uma implementação Android experimental, com interface Java nativa, motor
librespot/Rust por JNI e saída AudioTrack. Não é o executável Qt do Windows
reempacotado. O código e os artefatos permanecem locais, sem commit ou publicação.

Medições da versão 0.1.0: [ANDROID_BENCHMARK_LOCAL.md](ANDROID_BENCHMARK_LOCAL.md).
Correção da conexão na 0.1.1: [ANDROID_SPOTIFY_FIX.md](ANDROID_SPOTIFY_FIX.md).
Catálogo e interface móvel na 0.2.0: [ANDROID_CATALOG_LOCAL.md](ANDROID_CATALOG_LOCAL.md).
Áudio C++, HRTF/IAMF, Bluetooth e buffer na 0.3.0: [ANDROID_AUDIO_LOCAL.md](ANDROID_AUDIO_LOCAL.md).
Hub, IndieTonal, busca relevante, shows e recuperação na 0.4.0: [ANDROID_HUB_040_LOCAL.md](ANDROID_HUB_040_LOCAL.md).

## Instalar no POCO X7 Pro

Copie `out/android/Tonal-Android-0.4.0-arm64-v8a.apk` para o celular e abra o arquivo.
Instale por cima da versão anterior para preservar a autorização da biblioteca.
Autorize a instalação pelo gerenciador de arquivos quando o Android solicitar.
Esse APK contém `arm64-v8a` para o telefone. O universal `Tonal-Android-0.4.0-local.apk`
também contém `x86_64` para o emulador. Ambos requerem Android
9/API 28 ou mais recente. A assinatura é a chave Android de desenvolvimento local.

Abra **Tonalify** no hub. Use **Buscar** para encontrar músicas, artistas, álbuns e playlists; toque no
artista para abrir sua discografia e no álbum para abrir suas músicas.
**Biblioteca** reúne playlists, álbuns salvos, artistas seguidos e músicas curtidas.
As listas oferecem **Carregar mais**. Toque no mini-player para abrir os controles.

Para testar sem conta: abra **Conta** (ícone no canto superior direito) e escolha
**Testar áudio por 60 segundos** ou **Abrir arquivo de áudio**. O sinal de teste é
ruído rosa sintético de baixo nível. Em **Áudio**, há EQ, presets, graves,
compressor opcional, normalização local, teto de picos, HRTF para fones,
perfis por saída e opções de buffer. IAMF binaural funciona com arquivos
IAMF compatíveis. Os contadores PCM/DSP ficam nos modos de diagnóstico.

O streaming Spotify requer Premium. Ao tocar numa música, o Tonal conecta o motor
automaticamente e, se necessário, abre a autorização de reprodução no navegador.
Use a mesma conta Premium e volte ao Tonal. Em **Conta**, **Conectar motor Spotify**
permite conexão manual; **Refazer autorização do áudio** repete somente essa etapa.
Se seções da biblioteca pedirem novas permissões, use **Conta → Atualizar autorização
da biblioteca**. O login existente é preservado na atualização do APK.

São duas autorizações separadas, como no Windows: biblioteca Web API via PKCE e
callback `http://127.0.0.1:8898/callback`, com seu Client ID; reprodução via OAuth
do próprio motor, com callback `http://127.0.0.1:5588/login`, configurado
automaticamente. Seus tokens da biblioteca não são usados para autenticar áudio.
As credenciais antigas do motor são substituídas por um cache separado.

## Escopo da prévia

- Saída estéreo float AudioTrack; Spotify por librespot/Symphonia e arquivos
  locais por FFmpeg. Entrada multicanal e renderização OpenAL Soft ou IAMF/OAR.
- EQ de dez bandas com presets do PC, graves e compressor FFmpeg opcional;
  ReplayGain/R128 local e HRTF real para fones, inclusive Bluetooth.
- Cache criptografado de áudio em disco limitado a 256 MiB, solicitação de
  aproximadamente 60 segundos à frente e recuperação da conexão de controle
  preservando o áudio já baixado. Ganho digital unitário e normalização
  Spotify constante por faixa.
- Bypass padrão preserva os floats; efeitos incluem teto conservador de amostra.
- Transição de volume de 30 ms ao pausar/retomar, serviço de reprodução,
  MediaSession, foco de áudio e pausa ao desconectar fones.
- Atualizações da interface suspensas em segundo plano; textos iguais não
  provocam novo layout, e o indicador em repouso usa intervalo de cinco segundos.
- Busca por músicas, artistas, álbuns e playlists, páginas de artista/discografia,
  álbuns, playlists, músicas curtidas e biblioteca, com paginação via Web API.
- Fila de resultados, reprodução de contexto de álbum/playlist, avanço, retorno,
  posição, modo aleatório e repetição. Interface nativa com capas e mini-player.
- Token OAuth criptografado com chave não exportável do AndroidKeyStore;
  credenciais do motor no sandbox privado, backups desabilitados.

Letras, edição da biblioteca, downloads offline garantidos de playlists e Android Auto não
fazem parte desta versão. O teto é de amostras, sem garantia de true peak. O
emulador não permite concluir autonomia de bateria, qualidade sonora ou o consumo
exato no POCO. Esses pontos exigem medições no aparelho.

## Compilar localmente

Pré-requisitos usados: JDK 17, Gradle 8.14.3, Android SDK 36/build-tools 36,
NDK 27.0.12077973, Rust com alvos `aarch64-linux-android` e
`x86_64-linux-android`. AGP 8.13.2 e dependências Rust estão fixados no projeto.
O script usa os caminhos instalados neste computador.

```powershell
.\packaging\build-android-local.ps1
# Reempacotar depois de alterações somente Java/assets:
.\packaging\build-android-local.ps1 -SkipNative -SkipAudio
# APK menor para POCO X7 Pro:
.\packaging\build-android-local.ps1 -SkipNative -SkipAudio -PhoneOnly
```

O build exige as bibliotecas JNI e dependências C++ por ABI e usa alinhamento ELF de 16 KB. A licença
do projeto, avisos e licenças das dependências estão em `assets/licenses` do APK.
Symphonia está sem modificações; sua fonte está disponível no repositório upstream
referenciado nos avisos. Os patches Android de librespot ficam em
`android/native/vendor`. Consulte o guia de áudio para compilar FFmpeg e OpenAL.

## Testes e medições

`tests/android/measure.py` usa contadores de CPU de `/proc/PID/stat` e PSS de
`dumpsys meminfo --local -s -S`, somando os processos nomeados do pacote e
registrando swap separadamente, sem acordar processos congelados. CPU de 100% significa
um núcleo totalmente ocupado; não é o percentual do computador Windows. PSS
divide páginas compartilhadas entre os processos e não equivale ao número do
Gerenciador de Tarefas do Windows.

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
python tests/android/measure.py --adb $adb --package app.tonal.android `
  --scenario idle --label tonal-idle --output out/android/measurements/tonal-idle
# Acrescente --background para medir após Home.
# Cenários local/effects iniciam a amostra sintética de 60 segundos.
# Use warmup 10, samples 6, interval 4 para a amostra não terminar durante a coleta.
```

O APK `benchmark` tem pacote separado, depuração e instrumentação. O helper
`packaging/test-android-session-local.ps1 -Import` transfere a sessão Windows
somente para seu sandbox privado, por stdin; a atividade consome e apaga a semente.
O APK release distribuído não aceita essa importação, não contém a sessão e não
contém a instrumentação. Não distribua `out/android/session-private`, a AVD, dumps
de login ou o APK benchmark como se fossem a versão final.

O Spotify de comparação é um APK original, cuja assinatura foi confrontada com
o certificado publicado pelo Spotify em `open.spotify.com/.well-known/assetlinks.json`.
Versão, hashes e evidência estão em `out/android/comparison/spotify-origin.json`.
Não redistribua o APK oficial junto com o Tonal.

A AVD criada para estes testes chama-se `Tonal_Local_20261015`. Para reabri-la
manualmente com interface:

```powershell
& "$env:LOCALAPPDATA\Android\Sdk\emulator\emulator.exe" -avd Tonal_Local_20261015 -gpu host
```
