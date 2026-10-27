# 0.15.3 — faixas puladas, volume, Spotify em segundo plano e Jam

Data: 27/10/2026. Windows 0.15.3 / Android 0.15.3 (versionCode 23).

## 1. Músicas passando sozinhas (PC e Android)

**Causa observada** no diagnóstico do PC (`playback.log`, 27/10/2026 01:49): a
sessão Spotify deixava de responder às chaves de áudio (`Audio key response
timeout`) e o librespot pulava cada faixa com `Skipping to next track, unable to
load track`. A cada ~4 s uma música era descartada e a fila inteira passava.

**Correção**

- PC (`LibrespotProcess::onReadyRead`): só conta um pulo quando houve timeout de
  chave nos 10 s anteriores. Dois pulos assim em até 30 s encerram a cascata:
  o motor reinicia pela recuperação já existente (orçamento de 3 reinícios
  automáticos em 5 min) e a retomada volta à **primeira faixa pulada**, do
  começo, no mesmo contexto/lista (`takeFailedTrack` em `Player::rememberForResume`).
- Android (`lib.rs` + `librespot-core/audio_key.rs`): contador `KEY_TIMEOUTS`
  no núcleo vendorizado; `PlayerEvent::Unavailable` só conta quando o contador
  mudou. Dois em 30 s publicam `Falha Spotify…`, o que reconecta pelo serviço,
  com a primeira faixa pulada como ponto de retomada.
- Faixas realmente indisponíveis (sem timeout de chave) continuam sendo puladas
  pelo librespot como antes, sem reinício nem laço.

Limite: se a conta/rede continuar sem chaves, o orçamento de reinícios termina
em "O motor falhou repetidamente" no PC em vez de varrer a fila. Isso foi
validado com linhas sintéticas do log; não houve reprodução da falha com Spotify real.

## 2. Volume com aumentos bruscos (PC)

- Logo após o motor ficar pronto (reinício automático ou nova sessão), o
  Spotify informa o volume inicial do librespot. Nos primeiros 15 s, um valor do
  aparelho local que difere mais de 10 pontos do escolhido é tratado como
  inicial do motor: o Tonal reenvia uma vez o volume do usuário em vez de adotá-lo.
  Diferenças pequenas e outros aparelhos continuam reconciliados normalmente.
- Volume aceito do aparelho local é persistido (`player/volume`), para o próximo
  início do motor partir do valor atual.
- Mudo: `Player::toggleMute` guarda o volume anterior (`player/volumeBeforeMute`)
  e o restaura mesmo após reabrir o app; antes o valor ficava só na barra QML e
  voltava a 50 quando o mudo vinha de outra sessão.
- Throttle/serialização de 0.14.1 preservados ([VOLUME_WINDOWS_0141.md](VOLUME_WINDOWS_0141.md)).

## 3. Spotify do celular abrindo sozinho (Android)

Sem receptor de botão de mídia registrado na sessão, o Android encaminha um
PLAY de fone, carro ou Bluetooth ao último app que registrou um receptor — o
Spotify — que então inicia em segundo plano junto com o Tonalify.

Agora `PlaybackService` e `IndiePlaybackService` registram `MediaButtonReceiver`
(`setMediaButtonBroadcastReceiver` no Android 13+, `PendingIntent` antes). Com o
player aberto, a sessão recebe as teclas diretamente e segue a política de
intenção (inclusive o bloqueio de retomada com tela bloqueada). Com o player
fechado, a tecla chega ao receptor e é **ignorada**: não toca música nem acorda
outro player. Não foi testado com fone/carro físico; a causa é a explicação mais
provável, não um diagnóstico comprovado no aparelho do usuário.

## 4. Jam

**Por que "Iniciar Jam" parecia não funcionar:** iniciar só cria a sessão. Os
amigos entram pelo convite (link), que antes só podia ser copiado manualmente.
Além disso, no Android os avisos só existiam depois de abrir a janela do Jam.

- Ao iniciar, o Android abre o seletor de compartilhamento do sistema com o link
  (o usuário escolhe para quem; nada é enviado sozinho) e há o botão
  **Compartilhar convite**. O PC copia o convite automaticamente e mostra o link
  selecionável na janela.
- Android: o cliente Jam existe desde a abertura do Tonalify; com o motor pronto
  e o app na frente, procura um Jam ativo a cada 2 min no máximo. PC: ao ficar
  pronto e ao ativar a janela, no máximo a cada 5 min.
- Avisos leves no canto inferior esquerdo (subida + fade, 3,5 s, fila de até 8):
  quem entrou, quem saiu, sessão encerrada, músicas adicionadas (título — artista),
  músicas removidas e mudança de ordem.
- Android recebe ainda o `SessionUpdate` empurrado pelo serviço (dealer
  social-connect, `USER_JOINED`/`USER_LEFT`/`SESSION_DELETED`…), repassado pelo
  Spirc vendorizado como evento `tonal-jam`, com nomes limitados a 160 caracteres.
  O PC continua por consulta a cada 5 s enquanto o Jam está ativo e a janela, ativa.
- Avisos recebidos com o app em segundo plano aguardam (até 8) e aparecem ao
  voltar; não há sobreposição sobre outros apps nem permissão extra.
- Autoria de faixas: a API pública da fila não informa quem adicionou. O aviso
  diz o que mudou, **sem atribuir a um participante**.

Limites: protocolo experimental, não documentado pelo Spotify; sem teste entre
duas contas reais nesta entrega.

## Testes

Ver [release-0153-tests.json](release-0153-tests.json).
