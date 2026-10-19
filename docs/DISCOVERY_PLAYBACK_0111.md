# Tonal Hub 0.11.1 — Descoberta no PC e reprodução no Android

## Descoberta 2+1 no Windows

A página da música agora aguarda a resposta da fila antes de preparar uma nova ordem. As faixas preservam artista, álbum e ano para a seleção de sugestões. Quando o Spotify Connect entrega uma fila vazia para um álbum ou playlist, o player consulta o contexto exato e recupera as próximas músicas, com limite de 500 faixas e dez páginas. Uma resposta antiga não pode substituir a fila de uma nova seleção.

Na página da música, abra a Descoberta, confira a prévia e use **Aplicar e reproduzir fila**. No álbum ou playlist, ative a Descoberta e use Reproduzir. A seleção salva permanece na ordem original. A faixa externa aparece após cada duas faixas da seleção enquanto houver sugestões compatíveis.

Uma fila com apenas uma música não permite a proporção 2+1: a tela agora explica como iniciar por um álbum ou playlist. Também informa falhas de consulta e ausência de estilos, em vez de tratar todas as situações como falta de sugestões. O catálogo pode omitir os gêneros do artista; estilos já identificados nas outras faixas da seleção servem como referências adicionais. Não se atribui um gênero inventado à música.

O trabalho é assíncrono, com limite de 15 segundos, até 12 consultas de pesquisa e 120 candidatos. Não interrompe o áudio durante a preparação. As consultas de validação com a sessão existente não alteram a reprodução nem playlists remotas.

## Android: impedir reprodução inesperada

Pausar ou parar cancela a recuperação pendente, incluindo a espera pela internet. A saída do motor Spotify recusa áudio quando não existe intenção de reprodução. Eventos atrasados não podem autorizar uma nova reprodução. Reconexões automáticas preservam a revisão da seleção e só retomam dentro de dois minutos da interrupção.

Uma seleção ainda aguardando conexão é cancelada quando a tela do app sai de primeiro plano. Música que já estava tocando continua normalmente com a tela bloqueada. Comandos de ensaio de áudio ficam restritos ao APK de depuração.

Em **Áudio**, a opção **Permitir retomar por Bluetooth ou tela bloqueada** fica desligada inicialmente. Isso impede PLAY externo após uma pausa com o aparelho bloqueado. Para retomar intencionalmente pelo carro, fone ou tela bloqueada, habilite a opção. Pausar continua funcionando em qualquer configuração. O IndieTonal também ignora o término atrasado de uma faixa após uma pausa e bloqueia retomadas externas com tela bloqueada conforme essa preferência.

Essas proteções cobrem caminhos encontrados no código. O telefone físico e o Bluetooth do carro não estavam disponíveis para reproduzir exatamente as duas ocorrências relatadas; não se atribui uma causa única a elas sem os logs daquele aparelho.

## Atualização

Windows: `Tonal-Hub-0.11.1.0-Windows-x64-Setup.exe`.

POCO X7 Pro: `Tonal-Android-0.11.1-arm64-v8a.apk`. APK universal: `Tonal-Android-0.11.1-local.apk`. VersionCode 13; assinatura mantida para atualização sobre a versão anterior. Não é necessário desinstalar.

Projeto original: Matheus Nunes (natheus), <https://github.com/natheus/tonal-player>. Créditos e licença MIT preservados.
