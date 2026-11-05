# Coleções e limites por rota — 0.15.12

## Regressão da 0.15.11

A correção anterior generalizou uma resposta 429 da discografia para toda a Web API.
Álbuns e playlists passavam a falhar localmente por horas, embora essas rotas ainda
fossem permitidas pelo Spotify. A página Windows escondia o motivo em um toast
“Não consegui abrir…”, sem recuperação na própria página.

Em 04/11/2026, consulta autenticada de leitura confirmou: busca, biblioteca de
playlists, detalhe de álbum e detalhe de playlist responderam HTTP 200; discografia
respondeu HTTP 429 e Retry-After 17.848 segundos. Nenhum comando de reprodução,
nome/ID de playlist, corpo de resposta ou credencial foi gravado no relatório.

## Correção

- Espera por método e família de endpoint, nas duas plataformas. IDs, categorias,
  paginação e mercado não permitem repetir prematuramente a mesma rota bloqueada.
  Um limite em artistas não inventa um bloqueio para álbuns, playlists, busca ou player.
- O prazo completo do servidor continua respeitado. Uma resposta de sucesso em
  outra rota não apaga o limite. Registros expirados são removidos; até 64 famílias
  próprias mais um bucket de excesso, sem descartar esperas ativas.
- O cache público limitado da discografia é preservado. Não há troca de conta,
  IP, credencial ou protocolo para contornar o limite, nem promessa de remover
  uma restrição real do serviço. Uma rota que também retornar 429 terá sua própria espera.
- Windows exibe erro persistente, causa HTTP e Tentar novamente na página. Falha
  ao carregar mais mantém faixas/link e cancela a preparação de mix incompleta;
  a reprodução e a fila audível continuam independentes da navegação.

## Validação inicial

Cliente C++ de produção com a espera da discografia reconstituída: busca, biblioteca,
álbum/faixas e playlist/itens HTTP 200; consulta da discografia recusada localmente,
zero pedidos desse endpoint ao servidor. É leitura autenticada real, sem streaming
ou comandos de áudio. Não comprova execução no telefone físico.

QML 31/31 (inclui falha/recuperação de álbum e playlist e paginação com mix pendente),
20 verificações Java e suíte C++ de transporte aprovada. Relatórios sanitizados em
[collections-routing-live-01512.json](collections-routing-live-01512.json).

A espera real da discografia continua dependendo do Spotify. Os testes não
representam reprodução autenticada, acústica, benchmark novo ou aparelho Android físico.

## Entrega publicada

[v0.15.12](https://github.com/mikerock12/tonal-hub/releases/tag/v0.15.12) após PR40 e CI aprovado. CTest 11/11, QML31,
Java20, três variantes Android, portátil e assinaturas conferidos. Portal PR52
publicado e produção verificada. [Pacotes](artifacts-01512.json) e
[relatório final](release-01512-tests.json). Fontes compiladas no commit aa6d87a;
registros documentais posteriores não são novo build.
