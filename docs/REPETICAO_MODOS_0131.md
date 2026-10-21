# Repetição e modos da fila funcionando — 0.13.1

## O que mudou para quem usa

**PC.** Junto da barra de progresso ficam todos os controles, na mesma ordem
na barra inferior e na página da música:

**Shake · Aleatório · Anterior · Tocar · Próxima · Repetir · Descoberta 2+1**
(e **Fila** na página da música).

- **Shake** sorteia outra ordem das próximas músicas e aplica na hora.
- **Aleatório** liga ou desliga a ordem aleatória da fila atual.
- **Descoberta 2+1** liga (duas da fila, uma descoberta) ou desliga, tirando
  as próximas externas. A busca de sugestões roda em segundo plano.
- **Repetir** alterna: desligada → repetir a fila → repetir só esta música.

Os três modos aplicam direto na fila atual, a partir da música que está
tocando, que continua sem recomeçar. A janela **Fila** mantém a prévia com
**Aplicar e reproduzir fila** para quem quiser conferir antes.

Na página de um álbum ou playlist que está tocando, Shake, Aleatório e
Descoberta também passam a valer na hora; numa coleção que não está tocando,
valem no próximo **Reproduzir**, como antes.

**Android.** A página da música ganha **Repetir**, com os mesmos três estados
(fila e uma faixa, com o “1” no ícone). Os controles ficam organizados:
**Aleatório · Anterior · Tocar · Próxima · Repetir** e, abaixo,
**Shake · Descoberta 2+1 · Fila**. A escolha fica salva no aparelho.

## Por que não funcionava

**Repetição.** O motor librespot zera repetição e aleatório a cada nova
seleção (tocar um álbum, aplicar Shake ou 2+1). O app não reaplicava a
escolha, então a repetição sumia sem aviso. Além disso, uma consulta atrasada
ao Spotify desfazia o ícone, e uma recusa do Spotify (por exemplo, durante a
reprodução automática) era ignorada. No Android, o botão não existia.

Agora o app guarda a repetição escolhida e a reaplica logo depois de cada
nova seleção (no Android, também depois de uma recuperação). Respostas antigas
são ignoradas por alguns segundos e uma recusa desfaz o ícone com uma
mensagem. Como no app oficial, **pular para a próxima** desliga a repetição
de uma faixa e mantém a da fila; o fim natural de uma música repetida a toca
de novo.

**Shake e Descoberta 2+1 no PC.** Os botões da página da música só abriam a
prévia, que exigia “Aplicar”. E, quando a fila era reaplicada a partir da
mesma música, a faixa não mudava, então o painel **Fila** nunca era
atualizado e continuava mostrando a ordem antiga. Agora os botões aplicam
direto, e o painel Fila mostra imediatamente a ordem aplicada, com as
descobertas marcadas, sem depender da resposta do Spotify. Tocar uma seleção
comum (sem modos) encerra o mix.

## Validação

| Verificação | Resultado |
|---|---|
| CTest Windows (`spotify_recovery`) | Repetição persistente, reaplicada após seleção, GET antigo ignorado, recusa revertida, pular mantém fila; fila do mix exibida; botões aplicam direto; cancelamento seguro |
| QML (`tests/mix/run_ui.py`) | 15/15: controles completos na barra e na página da música, Shake/2+1 aplicam direto, repetição em três estados, modos da coleção que toca |
| Regras Java (`PlaybackSafetyChecks`) | 40 verificações, incluindo os três estados, reaplicação após cada seleção e pular |
| Android na AVD (`tests/android/repeat_smoke.py`) | 8 verificações: ordem dos controles, três estados, escolha salva e restaurada após reabrir, áudio local seguindo |
| Regressões Android | Smoke de temas (9) e fila/Descoberta (24 JNI) aprovados com os APKs 0.13.1 |

Registro: [repeticao-modos-0131-tests.json](repeticao-modos-0131-tests.json).

Uma queda de energia interrompeu um build do PC. O Git (`git fsck`), os
fontes e as bibliotecas nativas foram conferidos íntegros, e o PC foi
recompilado do zero antes dos testes e do instalador.

**Não validado nesta sessão:** a mesma sequência com o Spotify real. O teste
ao vivo `TonalSpotifyRecoveryTests --live-session --verify-mix` foi preparado
(dispositivo de diagnóstico isolado, áudio mudo, sem playlists; não roda se
houver reprodução ativa em outro aparelho), mas não foi executado porque a
permissão para usar a sessão real foi negada. Não houve teste no POCO X7 Pro
nem novo benchmark.

Base original: Matheus Nunes (natheus), [Tonal](https://github.com/natheus/tonal-player), MIT.
