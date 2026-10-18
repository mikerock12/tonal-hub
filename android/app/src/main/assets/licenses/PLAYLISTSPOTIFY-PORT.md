# PlaylistSpotify → Text to Tonalify

Origem: https://github.com/mikerock12/PlaylistSpotify
Revisão consultada: `77b15378525cdc210e76eaa4118d52a3e6d30057`.
Autor/proprietário do projeto pessoal: Maicon Nunes (mikerock12).
A integração e a conversão foram solicitadas pelo próprio autor.

As regras de `src/services/parser.ts` e `src/services/spotifyApi.ts` orientam
a adaptação para C++ em `src/playlist/`. A aplicação React e suas dependências
não são distribuídas nos executáveis/APKs. A interface foi adaptada aos
componentes nativos existentes, preservando colagem, revisão e criação.

As regras compartilhadas têm testes nos dois sistemas. A adaptação também
adiciona biblioteca local, capa, checkpoint SQLite e retomada de operações
remotas com resposta incerta. Não se aplica retroativamente uma licença nova
ao repositório de origem, que não declarava um arquivo de licença na revisão
consultada. A autorização de adaptação veio do pedido direto do seu autor.
