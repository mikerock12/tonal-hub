# Tonal: catálogo e reprodução de múltiplas fontes

Pesquisa em 16/10/2026. Trabalho local; sem commit, push ou PR.

## Decisão proposta

É tecnicamente viável criar um catálogo com artistas, gravações, edições de
álbuns e ofertas de várias fontes. A possibilidade de tocar cada oferta depende
do provedor, da conta, da região e da autorização da aplicação. Uma API de
metadados não equivale a uma API de reprodução.

Começar pelo catálogo independente, Audius e bibliotecas próprias OpenSubsonic;
adicionar Jamendo com uma aplicação própria ativa. Usar MusicBrainz para
identidade e discografia. YouTube entra como opção oficial de vídeo/link, com
uma possível experiência de YouTube Music não oficial avaliada separadamente.
A união do Spotify com outros serviços não pode ser apresentada como permitida
pelas condições atuais da API oficial.

## Fontes e viabilidade

| Fonte | Catálogo / reprodução | Custo e acesso | Prioridade |
|---|---|---|---|
| Audius REST | Busca de usuários/artistas, faixas e playlists; streaming para conteúdo autorizado | Leitura de muitos endpoints sem credenciais; plano de API gratuito. Conteúdo pago/restrito exige tratamento próprio | Alta |
| Jamendo v3 | Artistas, álbuns, faixas e URLs de áudio | Uso não comercial gratuito, client ID próprio; licença e download permitido por faixa | Alta, condicionada à credencial ativa |
| MusicBrainz | Artistas, obras, gravações, releases, ISRC e relações | Metadados gratuitos para uso não comercial, sem chave; User-Agent identificável e no máximo uma consulta por segundo | Alta; não fornece áudio |
| OpenSubsonic / Navidrome | Busca, álbuns, artistas, streaming da biblioteca do usuário | Software/protocolo aberto; exige coleção própria e servidor. Hospedagem e arquivos têm seus custos | Alta |
| Bandcamp Subsonic beta | Coleção do usuário e playlists; streaming/download da coleção | Credenciais em Fan Settings. Compras/conteúdo pertencente à coleção não se tornam gratuitos | Alta como extensão OpenSubsonic; precisa teste autenticado |
| Internet Archive / Live Music Archive | Busca, metadados e gravações ao vivo disponíveis | Gratuito; verificar autorização/política da coleção e do artista e restrições por item | Boa fonte complementar de shows, não de todo o catálogo de estúdio |
| YouTube Data API + IFrame | Busca de vídeos/canais/playlists e reprodução de vídeo incorporado | Projeto/chave Google e cotas; não entrega um stream PCM ao motor Tonal | Opcional, integração com capacidade diferente |
| YouTube Music via projetos comunitários | Biblioteca, busca e, com componentes adicionais, reprodução experimental | Não é uma API oficial; autenticação e compatibilidade podem mudar | Laboratório separado |
| Deezer | Consulta pública devolveu faixas e campos de prévia | Não validar cadastro novo nem presumir acesso a SDK/áudio completo | Catálogo/prévias, condicional |
| SoundCloud | Busca e streaming de faixas liberadas; outras são prévias ou bloqueadas | A documentação atual exige Artist Pro para registrar uma aplicação e obter credenciais | Fora do primeiro escopo sem nova assinatura |
| Apple Music / MusicKit | Catálogo e SDK de reprodução | Tokens de desenvolvedor; reprodução integral depende de assinatura Apple Music | Futuro, não atende ao pedido de fontes adicionais gratuitas |
| TIDAL | Catálogo, embeds e SDKs | Documentação pública limita reprodução fora dos embeds a prévias; acesso integral não deve ser presumido com uma assinatura comum | Futuro, depende do nível de acesso |
| Qobuz | API de catálogo e reprodução para integrações autorizadas | App ID/secret emitidos pelo provedor e condições específicas; não contar como streaming gratuito | Futuro |
| Amazon Music | API para integrações selecionadas | Portal informa beta fechada | Não planejar como disponível hoje |
| Last.fm / ListenBrainz | Histórico, relações e descoberta/recomendações, conforme endpoint | São complementos de metadados; não concedem um catálogo de áudio para tocar | Opcional após o catálogo básico |

Audius publica plano gratuito de 10 requisições/s e 500 mil/mês; Jamendo informa
até 35 mil consultas/mês para aplicações não comerciais. São limites documentados
nesta pesquisa, sujeitos a alteração. Nas buscas YouTube, conferir a cota real
do projeto: a documentação atual descreve um limite próprio de 100 chamadas
`search.list` por dia e outras cotas, em vez de assumir números antigos.

Fontes primárias:

- [Audius REST](https://docs.audius.co/api/) e [plano/SDK](https://docs.audius.co/sdk/).
- [Jamendo](https://developer.jamendo.com/v3.0.), [autenticação](https://developer.jamendo.com/v3.0/authentication), [faixas](https://developer.jamendo.com/v3.0/tracks) e [condições](https://devportal.jamendo.com/api_terms_of_use).
- [MusicBrainz API](https://musicbrainz.org/doc/MusicBrainz_API) e [ISRC](https://musicbrainz.org/doc/ISRC).
- [OpenSubsonic search3](https://opensubsonic.netlify.app/docs/endpoints/search3/), [stream](https://opensubsonic.netlify.app/docs/endpoints/stream/) e [Navidrome](https://www.navidrome.org/about/).
- [Bandcamp: anúncio oficial Subsonic, julho/2026](https://blog.bandcamp.com/2026/07/16/discover-improvements-and-subsonic-implementation/). A antiga API administrativa de vendas não é o conector indicado para ouvir a coleção.
- [Internet Archive: recursos de desenvolvimento](https://archivesupport.zendesk.com/hc/en-us/articles/360001495812-Developer-Resources) e [políticas do Live Music Archive](https://archivesupport.zendesk.com/hc/en-us/articles/360016553532-Live-Music-Archive-etree-org).
- [YouTube Data API](https://developers.google.com/youtube/v3/getting-started), [busca](https://developers.google.com/youtube/v3/docs/search/list) e [IFrame](https://developers.google.com/youtube/iframe_api_reference).
- [SoundCloud: requisitos de credenciais](https://developers.soundcloud.com/docs/api/register-app) e [guia de streaming](https://developers.soundcloud.com/docs/api/guide).
- [Deezer: portal](https://developers.deezer.com/) e [condições](https://developers.deezer.com/termsofuse). A documentação detalhada redirecionou a login; não confirmei criação de nova aplicação.
- [MusicKit](https://developer.apple.com/musickit/) e [Apple Music API](https://developer.apple.com/documentation/applemusicapi/).
- [TIDAL: guidelines de reprodução](https://developer.tidal.com/documentation/guidelines/guidelines-design-guidelines). Conteúdo textual acessível pela indexação oficial; o portal dinâmico não foi extraído integralmente.
- [Qobuz: termos de API](https://static.qobuz.com/apps/api/QobuzAPI-TermsofUse.pdf). Documento sem confirmação de onboarding atual; não prometer credenciais.
- [Amazon: estado da API](https://developer.amazon.com/docs/music/landing_home).
- [Last.fm](https://www.last.fm/api) e [ListenBrainz](https://listenbrainz.readthedocs.io/en/latest/users/api/).

## Spotify e YouTube: limites que mudam o desenho

A política Spotify, seção III.5, veda produtos integrados a streams ou conteúdo
de outro serviço. Portanto, o catálogo e a fila unificados incluindo Spotify
precisam de autorização compatível; separar apenas em abas não demonstra
conformidade. Librespot é um cliente comunitário, não uma licença adicional.
O experimento de novas fontes deve ficar independente da integração atual até
resolver essa condição. Não houve alteração do motor Spotify nesta etapa.
[Fonte](https://developer.spotify.com/policy).

O YouTube Premium completo inclui Music Premium, mas isso não concede uma API
pública de áudio ao Tonal. O caminho oficial oferece Data API e player visível;
as políticas vedam separar o áudio e criar reprodução em segundo plano por essa
integração. Não prometer cache offline, equalizador FFmpeg ou tela apagada por
meio do IFrame.
[Assinatura](https://support.google.com/youtube/answer/6305537),
[políticas](https://developers.google.com/youtube/terms/developer-policies).

Há alternativas técnicas comunitárias: `ytmusicapi` emula operações do cliente
web, não é endossado pelo Google e não é, sozinho, um motor de áudio. Music
Assistant tem conector YouTube Music, exige conta paga e documenta funcionamento
por melhor esforço. Isso justifica uma prova de conceito separada, não a promessa
de uma integração oficial estável nem de herdar todos os benefícios Premium.
[ytmusicapi](https://ytmusicapi.readthedocs.io/en/stable/),
[Music Assistant YouTube Music](https://www.music-assistant.io/music-providers/youtube-music/).

Music Assistant também é uma referência útil para arquitetura de identificação
e mapeamento entre fontes. A existência desse projeto não altera os contratos
dos provedores. Não recomendo exigir um servidor doméstico sempre ligado para
o Tonal móvel funcionar; uma conexão opcional com servidor pode existir depois.
[Referência de catálogo](https://www.music-assistant.io/ui/).

## Modelo de catálogo proposto

Manter a procedência de cada campo e oferta; nunca apagar uma opção de plataforma
ao agrupar duplicatas. Hierarquia:

```text
Artista canônico
  Obra / composição
    Gravação (estúdio, ao vivo, remix, cover)
      Edição / master
        Oferta: provedor + ID + conta/país + capacidades + data da verificação
  Grupo de lançamento (álbum / EP / single)
    Edição do lançamento (deluxe, país, ano, bônus, tracklist)
```

O modelo pode agrupar visualmente a mesma gravação e expandir suas versões e
fontes. Música de mesmo nome por artistas diferentes não é duplicata.

Ordem de evidência para sugerir vínculos:

1. Identidade de artista por IDs e relações verificadas, não apenas nome.
2. ISRC e MBID de gravação, confrontando duração, artista e indicação de versão.
   ISRC pode estar ausente ou errado; não identifica sozinho uma edição/master.
3. Sem IDs confiáveis, título normalizado + créditos + duração + álbum/versão
   geram candidatos, não fusão automática. Guardar confiança e justificativa.
4. Ao vivo, remix, cover, instrumental, clean/explicit, sped-up e remasterizações
   ficam preservados. Metadados conflitantes exigem revisão ou ficam separados.
5. Reversão manual de vínculo e fixtures de regressão para falsos positivos.

Lançamentos: usar release-group/release, código de barras quando disponível,
tracklist e edição. Ter a mesma capa/nome não prova que dois álbuns são iguais.

Na interface, oferecer selos `Audius`, `Jamendo`, `Biblioteca`, etc., e estados:
`reprodução completa`, `prévia`, `abrir no serviço`, `exige assinatura`,
`bloqueado na região`, `somente metadados`, `desconhecido`.
Usar "encontrado apenas em X nas fontes verificadas"; não declarar exclusividade
global com base em uma busca parcial ou em uma falha de rede. A disponibilidade
é verificada por região/conta, com horário e cobertura/paginação conhecidos.

## Integração no código existente

Hoje o desktop usa `src/SpotifyApi.*`, `src/Player.*` e chamadas Spotify diretas
em `qml/pages/SearchPage.qml`. Android usa `SpotifyClient.java` e
`CatalogScreen.java`. Não basta acrescentar um endpoint à busca existente.

Proposta de separação:

- `CatalogProvider`: busca paginada, artista, lançamentos, faixas, mapeamento dos
  IDs originais e política de cache/atribuição por fonte.
- `IdentityResolver`: núcleo C++ portátil compartilhado entre Qt e Android/JNI,
  sem Qt, rede ou dependência Python. A escolha é de projeto, não requisito de API.
- `PlaybackProvider`: resolve uma oferta para reprodução nativa, SDK, embed ou
  abertura externa. Nunca entrega uma prévia como se fosse a faixa integral.
- `Capabilities`: seek, duração conhecida, PCM acessível, DSP, buffer, download,
  segundo plano, autenticação, mercado e tipo de reprodução. Verificar por faixa.
- `SourceSelector`: respeita escolha do usuário, conta, versão e permissões;
  mudanças de provedor transparentes só após correspondência confiável. Não
  prometer retomar exatamente a mesma posição entre masters distintos.
- SQLite local: entidades e vínculos consultáveis offline; credenciais no cofre
  por serviço; URLs temporárias resolvidas ao tocar, sem tokens em relatórios.

Busca incremental com cancelamento, debounce, limites por provedor e resultados
parciais; uma API fora do ar não bloqueia as outras. Respeitar 429/Retry-After,
usar backoff e cache conforme as regras específicas, sem indexar discografias
inteiras em cada abertura. Capas limitadas em memória e carregadas sob demanda.

Nos provedores que autorizam acesso ao áudio, manter decoder → DSP C++ → saída
existente; um único dispositivo de saída ativo. Fazer buffer limitado em bytes
e tempo com política própria por serviço. Download permanente é uma capacidade
separada de prebuffer transitório. Não transportar as permissões/cache do Spotify
para outra fonte. SDK/DRM/embed podem impedir o acesso PCM e os efeitos do Tonal.

## Sequência de implementação

1. Contrato de provedor e modelo local; testes de identidade com casos difíceis.
   Fonte de arquivos locais e Audius primeiro, sem alterar a reprodução Spotify.
2. MusicBrainz com consultas espaçadas, busca e página de artista/edições,
   seletor de origem e procedência visível.
3. Jamendo com client ID próprio ativo; testes de acesso, faixas sem álbum,
   qualidade/formato, licenças e direitos de download.
4. OpenSubsonic: autenticação segura, descoberta de capacidades, busca e streaming.
   Validar Navidrome; depois Bandcamp beta com coleção e credenciais do usuário.
   Internet Archive pode entrar como conector complementar para shows autorizados,
   preservando data/local/formação e sem fundi-los com a gravação de estúdio.
5. YouTube oficial como vídeo/link; laboratório separado para YouTube Music
   comunitário. Resolver a permissão Spotify antes de unir seu conteúdo à nova fila.
6. Testes Android/PC: busca cancelada, API lenta, 401/403/429, expiração da URL,
   queda/retorno de rede, pausa, troca de faixa/fonte, Bluetooth físico, CPU/RAM
   em janelas longas. Comparar com a versão 0.3.0 antes de distribuir novo APK.

Primeiro aceite: artista com gravações e edições distintas, ofertas identificadas,
busca de fontes independentes sem congelar a UI, reprodução autorizada de uma
fonte nova passando pelo DSP e sem regressão no buffer/pausa. Protótipo de API
sozinho não cumpre esse aceite.

## Verificações realizadas nesta pesquisa

`packaging/probe-public-music-apis.py` faz uma consulta pública de metadados por
serviço. Não lê contas, não toca mídia, não é dependência de execução do Tonal.
Resultado em `out/research/music-api-probe-2026-10-16.json` e cópia neste diretório.

- Audius: HTTP 200, três resultados; exemplos incluem remix e faixa chamada
  "Nirvana" de outro artista. Demonstra por que buscar nome não prova identidade.
- MusicBrainz: HTTP 200, três candidatos de artista, incluindo homônimos.
- Deezer: HTTP 200, três faixas de Nirvana com campo de prévia; áudio não testado.
- Jamendo: HTTP 200 com erro da própria API, código 11: a aplicação da chave
  pública de exemplo está suspensa. Isso não prova indisponibilidade de aplicações
  próprias; precisamos de uma credencial ativa para validá-las.
- Sem login/testes de áudio nos novos provedores, sem build novo, sem mudança nos
  APKs 0.3.0 ou no comportamento atual do aplicativo.

Reproduzir a verificação na raiz do projeto, com rede disponível:

```powershell
& ./out/tools/python/Scripts/python.exe packaging/probe-public-music-apis.py --output out/research/music-api-probe-2026-10-16.json
```

Uma resposta HTTP válida e campos de stream não comprovam autorização, licença,
decodificação, qualidade ou desempenho de reprodução. São somente evidência de
disponibilidade das consultas desta rodada.
