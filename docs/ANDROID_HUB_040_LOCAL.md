# Tonal Hub Android 0.4.0 — implementação local

Atualização do APK 0.3.0, mantendo `app.tonal.android` e a assinatura local de teste. Instale por cima da versão anterior para preservar a sessão e os perfis do Tonalify. Não desinstale antes de atualizar.

## Conteúdo

- Launcher Tonal Hub em processo próprio, sem inicializar áudio, catálogo ou credenciais Spotify.
- Tonalify mantém o catálogo Spotify e os arquivos locais, com acesso ao IndieTonal a partir da consulta digitada.
- IndieTonal em processo próprio: Audius, discografia MusicBrainz, Live Music Archive, Jamendo com client ID próprio e biblioteca OpenSubsonic HTTPS.
- A alternância pausa o player anterior. A consulta literal é o único dado passado entre os catálogos. Não se transfere sessão, token, histórico ou resultados do Spotify.
- Busca normalizada por palavras, com artista exato à frente de remixes e nomes parciais. O score do MusicBrainz desempata homônimos. A ordem das faixas de uma edição não é reclassificada.
- Filtros com contagem, paginação, detalhes de artista/álbum/edição/faixas, erros por fonte e nova tentativa. MusicBrainz é somente metadados, sem botão de reprodução.
- Live Music Archive pesquisa `collection:etree AND mediatype:etree`, abre somente arquivos MP3 públicos e distingue artista exato de tributo, menção ou resultado relacionado. Coleções não são apresentadas como shows. O acervo não inclui todas as bandas.
- MusicBrainz: intervalo mínimo de 1,1 s, até duas novas tentativas com backoff e Retry-After, cache público em memória até 8 MiB, fresco por 15 minutos e fallback identificado por até 24 h.
- Conexões próprias do IndieTonal, com senha da biblioteca cifrada pelo Android Keystore. Trocar servidor/conta não reutiliza silenciosamente a senha anterior nem permite reproduzir resultados da conexão antiga.

## Áudio e estabilidade

O IndieTonal decodifica pelo MediaExtractor/MediaCodec do Android para PCM float, passando pelo mesmo DSP/AudioTrack já usado no Tonalify: equalizador, presets, graves, compressor, proteção de picos de amostra, HRTF OpenAL Soft e perfis por saída. IAMF continua disponível para arquivos compatíveis abertos no Tonalify; um stream estéreo não contém áudio IAMF.

Streams independentes usam reserva privada em disco: blocos de 128 KiB, leitura progressiva e prefetch de até 8 MiB, com cache de até 32 MiB por faixa e pouca memória adicional. O servidor precisa fornecer o arquivo público. Uma queda de rede não descarta os blocos já recebidos. Não há aumento de resolução ou codec Bluetooth por processamento do app.

O motor Spotify mantém os patches anteriores de cache cifrado de 256 MiB, leitura antecipada e reconexão de controle sem interromper PCM já disponível. A versão nova acrescenta:

- até três recuperações automáticas em cinco minutos;
- watchdog de 30 s sem avanço PCM, somente com intenção de tocar e internet validada;
- espera por internet sem consumir o orçamento de reinícios;
- retomada atômica com faixa real, fila/contexto e posição, sem consumir a intenção antes do sucesso;
- pausa, troca de faixa, arquivo local e troca de player cancelam a retomada antiga;
- callbacks associados à geração da sessão, sem aceitar mensagens de motores antigos;
- panics Rust recuperáveis contidos na thread nativa, sem `panic=abort`;
- limites totais de tentativa para Web API/refresh e descarte de callbacks após fechar a tela;
- logs privados rotativos e sanitizados, sem tokens nem URLs completas.

A confirmação de conexão no Android vem de callback JNI explícito; ele não dependia do log INFO que causava o bloqueio no Windows.

## Compilação e verificação

```powershell
./packaging/build-android-local.ps1 -SkipAudio
# Depois de validar o universal:
./packaging/build-android-local.ps1 -SkipNative -SkipAudio -PhoneOnly
```

O universal inclui ARM64 e x86_64; o APK `arm64-v8a` é o indicado para o POCO X7 Pro. Ambos têm os mesmos recursos. As bibliotecas usam alinhamento de 16 KiB e incluem licenças e pacote de fontes correspondente.

Testes automatizados ficam exclusivamente na variante `app.tonal.android.benchmark`, no AVD `Tonal_Local_20261015`. Relatórios desta versão: `out/android/updates-0.4.0/`, `out/android/audio-features-0.4.0/`, validações dos APKs e `docs/android-hub-040-tests.json`.

Os resultados efetivamente executados são registrados no relatório final. Emulador não comprova audibilidade, latência física Bluetooth, estabilidade no carro ou consumo no POCO. Jamendo e OpenSubsonic exigem conexões próprias para validação ponta a ponta. Esta atualização não é um novo benchmark de vantagem contra o Spotify oficial.

Nenhum commit, push ou PR foi feito. Toda a implementação e os artefatos são locais.

## Resultado da validação de 16/10/2026

- Builds release universal, release ARM64 e benchmark concluídos. APKs verificados: assinatura local compatível com 0.3.0, bibliotecas alinhadas a 16 KiB, 698 arquivos de licença e ausência de credenciais de teste empacotadas.
- 15 verificações de lógica e serviço/JNI: retomada com fila/posição, troca de seleção, pausa, limite real de reconexões sem conta, ranking, homônimos, HTTPS e sanitização de diagnósticos.
- Busca real: Veruca Salt em primeiro lugar; artista → All Hail Me → edição → três faixas somente de metadados; Nirvana dos EUA à frente do homônimo.
- Show real de King Gizzard & The Lizard Wizard: PCM no DSP compartilhado, pausa/retomada e zero underruns na amostra registrada. Com Wi-Fi e dados do AVD desligados durante a janela de teste, houve mais 322.560 frames reproduzidos a partir da reserva.
- Alternar para Tonalify pausou o IndieTonal. Parar pelo botão do app removeu o diretório da reserva da faixa. O prefetch também é suspenso durante pausa.
- 16 verificações nativas de áudio passaram: bypass/EQ/graves/compressor/limiter/rampa/HRTF, WAV 44,1/96 kHz, FLAC, MP3, AAC, ReplayGain, 5.1/7.1, IAMF estéreo/multicanal, normalização, arquivo inválido, cancelamento e isolamento de perfis.
- Atualização do release 0.3.0 → 0.4.0 instalada com sucesso no AVD, preservando UID, data da instalação original e inode do diretório de dados. Hub e busca pública também passaram no APK release.

Estas verificações não repetiram streaming Spotify autenticado nem a comparação com o app oficial. Não houve teste físico de Bluetooth, POCO ou carro. Jamendo/OpenSubsonic não foram validados com credenciais próprias nesta rodada. As medições antigas de CPU/RAM continuam identificadas pela versão em seus relatórios.
