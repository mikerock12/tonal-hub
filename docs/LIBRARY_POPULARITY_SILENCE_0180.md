# Biblioteca, popularidade, resumo local e silêncio — 0.18.0

## Seguir e favoritar

- Tonalify consulta e altera `/me/library` e `/me/library/contains`: artistas,
  álbuns e playlists seguem a conta Spotify. Android também solicita
  `user-follow-modify`; uma autorização antiga pode precisar ser renovada.
- IndieTonal mantém seguidos de artistas e favoritos de álbuns/playlists apenas
  no aparelho. `collectionFavorites` aceita até 500 registros validados e entra
  nos backups completos, sem credenciais. Não há sincronização entre serviços.

## Popularidade real e seus limites

A Web API fornece seguidores de artista e, quando presentes, de playlist e o
índice `popularity` (0–100). Ela não fornece plays nem ouvintes mensais. Para
esses dados e cidades, a leitura autenticada `queryArtistOverview` usa a sessão
nativa existente: `followers`, `monthlyListeners`, `worldRank`, `topCities` e
`playcount` das faixas populares. As cidades mostram **ouvintes**, não
seguidores regionais. A consulta pública do cliente Spotify pode mudar; falha
mantém os dados da Web API e não inventa números. Não há escrita nesse caminho.

Audius fornece `follower_count`, `play_count` e `favorite_count` quando
presentes. Contagens locais de escutas continuam identificadas como deste
aparelho. O Spotify não informa total de favoritos de álbuns pela Web API;
esses itens ficam no fim da ordenação em vez de receber zero inventado.

## Ordenação temporária

Windows e Android ordenam a lista exibida por: ordem original, título, duração,
artista, álbum, gênero, plays/escutas, favoritos do álbum e seguidores do
artista. Outro toque na opção inverte a direção. Plays globais são usados
quando disponíveis; caso contrário, o rótulo diferencia as escutas locais.
Valores desconhecidos ficam por último e a ordem original é estável. A playlist
salva no Spotify nunca é reordenada; reproduzir usa a ordem temporária exibida.

## Resumo sem modelo

“As histórias das suas músicas” foi substituído por **Gêneros e clima do seu
repertório**. Até oito faixas do período são contadas por gênero e pelas pistas
de clima já registradas na biblioteca. Não há interpretação de letra, rede,
LLM, download ou dependência de pausa. Pesos Qwen, checkpoints e resumos do
leitor antigo são apagados na inicialização da biblioteca nova. TonalMuse Tiny
permanece um classificador separado; não há diagnóstico de humor ou saúde.

## Tirar silêncio

O botão em Transições funciona sozinho ou com qualquer efeito. O núcleo
`TransitionMixer` remove apenas quadros estéreo em que **os dois canais são
exatamente 0,0**, no fim de A e começo de B. Volume baixo não é silêncio e não é
cortado. Sem transição, A e B são concatenados sem crossfade; com transição, a
mescla termina no último quadro não nulo de A. Pulos, seeks e seleções mantêm
o cancelamento existente. O trace registra `trimmedA` e `trimmedB`.

## Evidência

- Rust: testes do endpoint e da normalização pública aprovados.
- C++ `TransitionChecks`: efeitos existentes, corte independente, corte com
  crossfade, zeros reais e amostras de amplitude 0,0002 aprovados.
- Android: quatro variantes Java compiladas.
- Demais testes, builds e publicação são registrados em
  `docs/release-0180-tests.json` e `docs/artifacts-0180.json`.

Não há alegação de teste acústico, Bluetooth, aparelho físico ou benchmark.
