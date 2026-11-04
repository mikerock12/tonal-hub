# Discografia Spotify — correção de 04/11/2026

## Diagnóstico

A captura Android enviada pelo usuário mostra HTTP 429 nas três categorias de
artista: Álbuns, Singles e EPs e Coletâneas. Isso identifica uma limitação de
consultas da Web API. Não comprova queda geral do Spotify, nem permite descobrir
qual era o prazo Retry-After daquela resposta, pois a captura não contém o cabeçalho.
No Windows, ArtistPage ocultava toda a discografia quando nenhum pedido tinha
sucesso. A captura Windows sozinha não confirma qual status o servidor retornou.

O Android não recuperava respostas 429 no get() do catálogo. O Windows repetia
cada pedido de forma independente e cortava Retry-After em 60 segundos; o
importador Android também cortava a espera. Consultas das outras telas continuavam
saindo durante o bloqueio. Visitas repetidas ao artista consultavam novamente as
três categorias, sem cache.

## Comportamento corrigido

- Windows mantém Discografia e as três categorias acessíveis, com estados de
  carregamento, vazio e erro, além de Tentar novamente. Busca Álbuns inicialmente;
  as outras categorias são consultadas ao selecionar sua aba. Carregar mais pede
  uma página por ação, preserva o link após erro e os cartões já carregados.
- Android carrega as três seções em sequência, com intervalo de 1,25 segundo,
  mantendo controles de nova tentativa e paginação. Navegar cancela entregas,
  repetições e categorias ainda não iniciadas daquela página.
- Cada cliente conserva um cooldown comum à Web API, sem reduzir prazos longos.
  Outras respostas bem-sucedidas não apagam o bloqueio. Pedidos novos recebem
  o prazo restante localmente, sem consultar o Spotify durante a espera.
- Esperas de até 30 segundos admitem tentativas automáticas limitadas. Esperas
  maiores voltam à interface para tentativa manual, com mensagem em português.
  Controles Windows aguardam esperas curtas e preservam sua guarda de intenção;
  um volume superado não volta a ser enviado. O cooldown não dá play/pause,
  não altera o motor e não mexe no callback PCM.
- Cache somente em memória de páginas públicas de discografia: 10 minutos,
  até 32 páginas, chave por URL completa (artista, categoria, mercado e página).
  Páginas grandes não são guardadas (Windows: 128 KiB de resposta; Android:
  65.536 caracteres). Limpa os cartões ao mudar conta/logout; conserva o cooldown
  durante a vida desse cliente. Não grava tokens, letras ou biblioteca no cache.
  Dados já em cache continuam disponíveis durante uma limitação.

A correção evita insistência prematura e telas vazias silenciosas. Não remove
um bloqueio ativo no serviço, não garante catálogo imediatamente disponível e
não contorna a cota. Cooldown/cache não persistem depois de encerrar o processo.
Nenhuma release, instalação em aparelho físico ou teste Spotify autenticado é
comprovado apenas pela compilação e pelos testes sintéticos desta alteração.

## Referências

- [Spotify: rate limits e Retry-After](https://developer.spotify.com/documentation/web-api/concepts/rate-limits)
- [Spotify: discografia e include_groups](https://developer.spotify.com/documentation/web-api/reference/get-an-artists-albums)
- [Relato de desenvolvedor: 429 em artistas/álbuns, busca disponível e esperas de 13–18 horas](https://community.spotify.com/forum/en/discussion/7432857/very-long-retry-after-values-on-web-api-429).
  É um relato público, não confirmação oficial da causa do caso do usuário.

## Validação

Resultados locais e limitações em [catalog-rate-limits-tests.json](catalog-rate-limits-tests.json):
29 QML, 13 verificações Java, três variantes Java compiladas, CTest 2/2 (20,90 s),
build Windows com IAMF e APK ARM64. Tentativas anteriores estão preservadas no relatório.

Candidatos locais e hashes: [catalog-rate-limits-artifacts.json](catalog-rate-limits-artifacts.json).
O APK conserva versionCode 30 e o certificado 0.15.10, com alinhamento 16 KiB.
Seu nome inclui discografia-teste; não substitui a identificação da release já
publicada. Windows deve executar dentro da pasta de build com suas DLLs/plugins.
Não houve instalação automática, publicação ou alteração dos pacotes entregues.
A validação não usa sessão ou credenciais Spotify. Nenhum aplicativo ou ferramenta
adicional foi instalado/baixado; foram usados Qt, MSVC, Python, JDK e Gradle já
existentes. O Gradle foi executado offline.
