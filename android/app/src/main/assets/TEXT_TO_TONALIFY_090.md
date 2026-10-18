# Text to Tonalify — Windows e Android 0.9.0

Abra **Criar playlist → Criar com Text to Tonalify** na Biblioteca do Tonalify.
O mesmo assistente também está disponível na biblioteca pessoal do player.

1. Cole o texto/setlist. Escolha Artista — Música ou Música — Artista, se
   necessário, e informe um artista padrão opcional. Cabeçalhos, numeração e
   timestamps são limpos automaticamente.
2. Revise os resultados. Correspondências fortes ficam selecionadas;
   resultados duvidosos exigem sua escolha. É possível trocar a gravação,
   corrigir artista/título, ignorar faixas e mudar a ordem.
3. Escolha nome, descrição e uma capa. Reprises/repetições são preservadas;
   a remoção de duplicadas é uma opção explícita.
4. Salve no Tonalify. A playlist fica na biblioteca SQLite deste aparelho.
   Depois escolha se deseja uma cópia no Spotify, privada ou pública.

Não há publicação automática apenas por colar, buscar ou salvar localmente.
Uma playlist local com faixas Spotify continua precisando da conta/conexão
adequadas para reproduzir essas faixas; ela não é um download de áudio offline.

## Implementação nativa

As regras de parsing, normalização, consultas e ranking foram portadas do
projeto pessoal [PlaylistSpotify](https://github.com/mikerock12/PlaylistSpotify)
para `src/playlist/TextPlaylistCore.cpp`, C++20 portátil. O mesmo arquivo é
compilado no Windows e no Android, onde é acessado por JNI. Não há React,
WebView, Electron, Node ou servidor JavaScript embutido.

A interface usa o Qt/QML já existente no PC e os Views nativos já existentes
no Android. Os clientes Spotify existentes cuidam do token, renovação e rede;
nenhum token é copiado para o parser, a playlist ou o backup.

O ranking considera artista/título, trata popularidade como opcional e
penaliza covers, karaokês e tributos quando não foram pedidos. Nomes homônimos
e versões duvidosas continuam disponíveis para revisão, sem prometer uma
correspondência perfeita ou trocar silenciosamente uma gravação solicitada.

## Conta, capa e interrupções

O assistente reaproveita a sessão Web API do Tonalify. Os escopos de editar
playlists já eram usados pelo player. Para enviar uma capa, o Spotify pode
pedir nova autorização com `ugc-image-upload`; o botão do próprio assistente
inicia o fluxo OAuth existente, sem reiniciar o motor de áudio por esse motivo.

O cliente usa os endpoints atuais de [criação](https://developer.spotify.com/documentation/web-api/reference/create-playlist),
[itens](https://developer.spotify.com/documentation/web-api/reference/add-items-to-playlist)
e [capa](https://developer.spotify.com/documentation/web-api/reference/upload-custom-playlist-cover).
O envio de músicas é feito em lotes de até 100, na ordem revisada. A imagem
é convertida em JPEG/base64 com payload de até 256 KiB.

O SQLite confirma o registro local antes de cada alteração remota. Se a
resposta de criação se perder, o app pede que você confira a Biblioteca Spotify
antes de permitir uma nova criação. Se a resposta de um lote se perder, o
conteúdo remoto é conferido antes de retomar, sem repetir cegamente as faixas.
Alterações inesperadas na ordem/conteúdo interrompem a retomada.

Falhar apenas a capa não apaga as músicas já copiadas. Após autorizar imagens,
use **Concluir / conferir envio** para terminar a capa. O mesmo botão fica na
playlist pessoal e permite retomar após fechar/reabrir o app. O envio é uma
cópia; não há sincronização bidirecional automática entre as duas bibliotecas.

## Limites e backup

- Até 500 linhas de músicas, 128 KiB de texto e 2.000 bytes por linha.
- Nome até 100 caracteres; descrição até 300.
- Imagem até 16 MiB comprimidos / 16 megapixels, reduzida antes de armazenar.
- A biblioteca pessoal mantém os limites existentes de 100 playlists e
  3.000 músicas; novas importações conservam margem para o backup de 8 MiB.
- Descrição, capa, ordem, repetições e estado do envio entram no backup pessoal.

Os testes de escrita remota usam servidor/catálogo simulados. Eles não criam
playlists de teste na conta real. O teste Android usa um pacote `.benchmark`
separado e um emulador dedicado; os APKs entregues são os de produção locais.
Resultados finais: `text-playlist-090-tests.json`.

O projeto Tonal original é de **Matheus Nunes (natheus)**. Os créditos originais
e os da adaptação PlaylistSpotify estão em `CREDITS.md`.
