# Tonal Hub 0.11.0 — fila, sorteios e letras

Créditos: Tonal original de Matheus Nunes (natheus), https://github.com/natheus/tonal-player. Fork privado de uso pessoal; créditos e licenças anteriores preservados.

## Como usar

- **Shake** (lista em movimento): cada toque sorteia uma nova ordem visual temporária. Mantenha pressionado para restaurar a ordem original. Não altera a playlist salva no Spotify.
- **Aleatório** (setas cruzadas): muda a sequência de reprodução; usa prioridades aleatórias independentes do Fisher–Yates do Shake. Play do álbum/playlist também sorteia a primeira faixa; tocar numa música específica conserva essa escolha inicial.
- **Descoberta 2+1** (lupa com play, + e setas): sorteia sugestões compatíveis, inseridas na fila após cada duas faixas da seleção. Artistas não presentes no histórico têm maior peso, sem bloquear outros candidatos compatíveis. A duração máxima de preparação continua sendo 6,5 segundos, até 12 consultas e 120 candidatos.
- A página da música tem os três controles e **Fila** abaixo do transporte. Ela mostra a faixa atual e as próximas músicas. Alterações aparecem como **prévia**: confirme **Aplicar e reproduzir fila** para substituir a sequência, retomando a faixa atual na posição atual. A operação pode recarregar brevemente a faixa; preparar a prévia não interfere no áudio.
- Na fila Android, tocar novamente em um modo sorteia de novo. Segure Aleatório/Descoberta para desligá-los. A fila Spotify consultada pelo serviço pode ser parcial; filas explícitas mantidas pelo Tonalify têm até 500 entradas. A tela Android cria inicialmente até 100 linhas.
- Faixas inseridas pelo planejador aparecem identificadas como **Descoberta** na prévia. Se a música mudar antes da confirmação, a aplicação é recusada e é necessário preparar novamente.
- Em letras de tela cheia: opções ocultas → primeiro toque somente revela os controles; opções visíveis → toque numa linha alinha a letra e oculta os controles. Tocar novamente primeiro revela os controles. Rolagem não é tratada como toque.

## Recuperação de letras

O HTTP 503 do print vem do serviço externo LRCLIB. A versão 0.11 mantém o estado ocupado durante a espera das tentativas, impedindo que a tradução automática invalide a recuperação. Faz duas tentativas com espera progressiva e, após falhas temporárias persistentes, tenta a busca por título/artista, com conferência estrita da versão e duração. Não troca silenciosamente para a letra de uma gravação diferente. Uma atualização manual reinicia o ciclo limitado. Android respeita `Retry-After` numérico com limite de espera de 60 segundos; 429 não dispara a busca alternativa imediata. Cache de sessão continua limitado a 30 faixas.

Se o serviço inteiro permanecer indisponível, uma mensagem explica a situação e TXT/LRC continua disponível; nenhum aplicativo pode assegurar disponibilidade de um servidor externo. Letras e tradução continuam usando a mesma lista de linhas.

## Implementação e limites

Ordenação e amostragem permanecem em C++ compartilhado entre PC e JNI Android; sem React, WebView ou novo motor de linguagem. Sorteio ponderado por corrida exponencial substitui a antiga ordenação quase fixa por afinidade. Fontes de entropia são renovadas a cada preparação. O comportamento determinístico por semente fica restrito à reprodução de testes.

Controles de descoberta de catálogo Spotify pertencem ao Tonalify. IndieTonal mantém suas fontes e seus controles próprios. Esta entrega não altera o callback de áudio, DSP, codec Bluetooth ou buffer de PCM, e não representa um benchmark de CPU/RAM nem uma audição física no POCO.

Referências de integração: [LRCLIB](https://lrclib.net/docs), [fila Spotify](https://developer.spotify.com/documentation/web-api/reference/get-queue), [reprodução por URIs e posição](https://developer.spotify.com/documentation/web-api/reference/start-a-users-playback).

## Validação

Consulte `mix-queue-lyrics-0110-tests.json` para os resultados executados. Os testes usam faixas e respostas sintéticas, sem criar playlists reais, enviar textos de teste de letras ou modificar a conta Spotify.
