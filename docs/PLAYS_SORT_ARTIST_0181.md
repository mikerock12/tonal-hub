# Plays, ordenação da fila e bastidores do artista — 0.18.1

## Plays em álbuns, playlists e na fila

A Web API do Spotify não informa reproduções. Desde a 0.18.0 a página do
artista lia os plays das faixas populares pela sessão nativa
(`queryArtistOverview`). A 0.18.1 acrescenta a operação `albumPlays`, no mesmo
componente (`native/jam-transport`, compartilhado por `TonalJam.exe` e pelo
Android): a consulta persistente `getAlbum` do cliente Spotify devolve o
`playcount` de cada faixa do álbum.

- Até 8 álbuns por pedido, buscados em paralelo, até 150 faixas por álbum,
  corpo limitado a 1 MiB, tempo-limite de 10 s por página. Álbum que falha
  continua desconhecido e não interrompe os outros.
- Página de álbum: um pedido. Playlist e fila: os álbuns distintos das faixas
  carregadas. Cache em memória de 6 h, até 6.000 faixas e 1.500 álbuns.
  Respostas 429/503 pausam novos pedidos por 60 s.
- Essas leituras usam um canal próprio (Android: `catalog_busy`; Windows:
  `CatalogStats` com o próprio processo `TonalJam.exe`), separado do Jam.
- Valor desconhecido nunca vira zero nem estimativa: a linha mostra só o que
  existe e a ordenação coloca o item no fim.
- Formato abreviado em pt-BR: `999`, `2,2K`, `200,5K`, `4,5M`, `1,2B`
  (`CatalogFormat.compact` no Android e `CatalogStats::compact` no Windows).

A consulta persistente pertence ao cliente web do Spotify e pode mudar sem
aviso. A falha mantém o app funcionando sem os números.

## Posição no mundo

`worldRank` é a posição do artista no **ranking global de ouvintes mensais** do
Spotify, que só cobre os 500 primeiros. Fora dele o Spotify devolve 0. A 0.18.0
exibia "#0 no mundo". A 0.18.1 trata 0 como ausente e mostra
"#N no mundo em ouvintes mensais" apenas para posições reais. Leitura real em
26/11/2026: Taylor Swift #7, Nirvana #136.

## Ordenar num botão, também na fila

- Página de playlist/álbum: os chips viraram um botão **Ordenar: …** que abre a
  lista de critérios. Escolher o critério atual de novo inverte a direção.
  Quando a coleção é a que está tocando, a ordem também é aplicada às próximas
  da fila. A música atual continua tocando e a playlist salva não muda.
- Fila (Android e Windows): botão **Ordenar** com título, duração, artista,
  álbum, gênero, mais tocadas (Spotify) e seguidores do artista. A edição usa a
  mesma transação de arrastar ou excluir: a música atual, a fila parcial e o
  limite de 500 entradas seguem as regras da 0.15.14/0.16.0. **Ordem anterior**
  desfaz a ordenação feita nesta janela.
- "Mais tocadas" espera os plays dos álbuns necessários, mostrando o progresso
  em álbuns, e coloca no fim as faixas sem dado público. Gênero e seguidores
  vêm da Web API (`/artists`, 50 por pedido).

## Bastidores do artista

A falha "Informações do artista temporariamente indisponíveis" vinha da busca
do MusicBrainz, que estava recusando pedidos (HTTP 503 `search-shed`).

- Identidade exata primeiro: o link `open.spotify.com/artist/{id}` registrado no
  MusicBrainz (`/ws/2/url?resource=…&inc=artist-rels`). É uma consulta direta,
  não uma busca, e não depende do nome. Sem o link, o app volta para a busca
  por nome/álbum, que continua se abstendo diante de homônimos.
- Fatos: origem (cidade e país), início e fim da banda ou nascimento, estilos.
- Integrantes: relações "member of band" agrupadas por pessoa, com
  instrumentos traduzidos, período e marcação de atuais. Até 8 listados, com os
  atuais e os originais primeiro.
- Idades: datas de nascimento e morte dos 6 primeiros integrantes, uma consulta
  por pessoa no limite comum de 1,25 s do MusicBrainz. A idade só é calculada a
  partir dessas datas ("cerca de" quando só há o ano). Sem data, nenhuma idade
  aparece.
- 503/429 mostram um aviso de sobrecarga e o botão **Tentar novamente**. Esse
  resultado não fica em cache.
- **Público no Spotify** (movido da página do artista): ouvintes mensais,
  seguidores, posição real no mundo e as cidades com mais **ouvintes**.

## Seguir / Seguindo

O botão virou uma pílula na cor de destaque: **+ Seguir** preenchido e
**✓ Seguindo** em contorno, com transição de cor ao confirmar. O estado só muda
depois da resposta do Spotify. No Android, o clique seguinte usa o estado
confirmado, sem nova consulta.

## Validação e limites

- Rust: 3 testes de unidade (plays por álbum, linhas inválidas ignoradas,
  ranking 0 tratado como ausente) e leitura real na sessão do PC: estatísticas
  de 2 artistas e plays das 13 faixas de um álbum.
- Java no host: `CatalogFormatChecks` (formato abreviado e idades). Windows:
  CTest 12/12, com novos casos de integrantes, idades e identidade por link;
  QML 37/37, com ordenação da fila e da playlist.
- Não testado: Spotify autenticado no Android, aparelho físico e mudanças
  futuras na consulta persistente do Spotify. Idades e integrantes dependem da
  qualidade dos dados colaborativos do MusicBrainz.
