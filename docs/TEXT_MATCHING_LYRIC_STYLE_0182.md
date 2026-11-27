# Text to Tonalify e estilo das letras — 0.18.2 (27/11/2026)

Windows 0.18.2 e Android 0.18.2 (versionCode 52). Este documento é o relatório pedido pela
especificação `TONALIFY_Text_to_Tonalify_Auditoria_Correcao_Estrutural.md`: o que foi corrigido,
o que não pôde ser corrigido, por quê, e como foi testado. Também descreve o novo diálogo
**Fontes e cores das letras**.

## 1. Text to Tonalify

### 1.1 Causa raiz (comportamento antes)

Fluxo anterior: `texto → parse → até 4 buscas textuais → rank() → choice=0 + selected=highConfidence`.
Baseline gravado com o `rank()` da 0.18.1 nos 15 casos dos prints (matriz em 1.6):

- **Falsos positivos de apresentação.** O primeiro resultado do ranking era sempre pré-escolhido
  (`choice=0`) e rotulado "Revisar versão", mesmo quando era de outro artista (Bobby Brown/Whitney
  Houston, Balcony Sunrise, Buckcherry, Chandelier, Screaming Trees). A interface tratava homônimos
  como "versões" da música pedida; um toque na caixa os incluía na playlist.
- **Falsos negativos.** Título e artista pesavam igual num índice de palavras (Jaccard), sem tratar
  singular/plural (`Nightmares`/`Nightmare`) nem créditos entre parênteses (`Age of Consent (New Order)`).
  Qualquer penalidade (live, remix…) zerava a confiança.
- **Recuperação.** Nenhuma busca dirigida ao artista: as consultas eram textuais e a última era só
  pelo título, que traz homônimos. Uma lista de faixas homônimas era tratada como resultado final.
- **Erros técnicos.** Qualquer status diferente de 200 parava toda a busca e o texto de erro ia para
  o status da linha; não havia distinção entre erro e ausência.
- **Estado/UI.** O filtro "Só não encontradas" listava qualquer linha desmarcada; contadores
  derivavam só de `selected`; escolhas manuais não eram distinguíveis de automáticas.
- **Mercado.** Sem `market`; faixas com `is_playable=false` eram descartadas em silêncio.

### 1.2 Modelo tipado (C++ compartilhado, `src/playlist/TextPlaylistCore.*`)

- `Song` (referência pedida, linha original preservada), `Title` (núcleo normalizado + qualificadores
  estruturados + versão), `Candidate` (IDs/URIs, artistas e IDs na ordem do Spotify, álbum, tipo,
  data, duração, `playable`, `restriction`, `linkedFrom`, proveniência), `Evaluation` e `Decision`
  (classe, `safe`/`autoSelect`, motivo legível), `Lookup` (busca progressiva).
- Classes: `EXACT_MATCH`, `EQUIVALENT_MATCH`, `ALTERNATE_VERSION`, `COVER_OR_OTHER_ARTIST`,
  `WRONG_ARTIST`, `WRONG_SONG`, `AMBIGUOUS`, `NOT_FOUND`, `UNAVAILABLE_IN_MARKET`, `LOOKUP_ERROR`,
  `SEARCH_INCOMPLETE` e `PENDING`. Os códigos são persistidos na auditoria e mapeados para rótulos.
- Identidade: linhas da revisão têm `requestId` estável; candidatos são deduplicados por URI; o
  índice visual nunca é identidade.

### 1.3 Normalização e decisão

- Apóstrofos retos/tipográficos não quebram palavras (`Baby’s` = `Baby's` = `Babys`); `&` = `and`;
  acentos, caixa, hífens e pontuação normalizados só na cópia usada para comparar. Números,
  negações e partes (`Part 2`, `Reprise`) são preservados; parênteses no início fazem parte do nome.
- Qualificadores entre parênteses, colchetes ou após ` - ` viram campos: ao vivo, acústica, demo,
  remix, instrumental, karaokê/tributo, cover, regravação, edição para rádio, alternativa; remaster,
  deluxe, mono/estéreo, ano, `feat.` e créditos desconhecidos (ex.: `(New Order)`) são equivalentes.
- **Barreira de artista antes do título:** o artista pedido precisa ser o intérprete principal
  (`artists[0]`), confirmado pelo ID da busca de artista quando disponível. Mesmo nome com outro ID =
  homônimo (`AMBIGUOUS`, nunca automático); dois artistas homônimos com a música = `AMBIGUOUS`;
  participação (`artists[k>0]`) não substitui o intérprete; nome parecido exige confirmação.
  Sem artista na lista, nada é selecionado automaticamente.
- Título: igual (`EXACT`), igual após qualificadores equivalentes (`EQUIVALENT`, automático),
  singular/plural de uma palavra com artista confirmado (`EQUIVALENT`, automático), grafia parecida
  ou prefixo de título longo (`EQUIVALENT`, **confirmar**), outra música do artista (`WRONG_SONG`).
- Versão diferente da pedida (ao vivo, remix…) = `ALTERNATE_VERSION` (revisão); estúdio é preferido.
  `is_playable=false` = `UNAVAILABLE_IN_MARKET` com o motivo `restrictions.reason`.
- Ordenação: resultados do artista pedido primeiro, por classe e evidência; outros artistas por último.
  Se o melhor não é do artista pedido, a decisão é `NOT_FOUND` **sem candidato pré-escolhido** e o
  motivo informa quantos resultados de outros artistas foram separados.
- Limiares (Jaccard 0,5/0,75, distância de edição 1–2) foram fixados pelos fixtures e pelo teste real;
  não são escores calibrados de confiança e não são exibidos como porcentagem.

### 1.4 Busca progressiva (`tonal::playlist::Lookup`)

Máquina de estados pura (sem rede, relógio ou threads); Windows (`TextPlaylistImport`) e Android
(`TextPlaylistWizard` via JNI) só executam os pedidos e devolvem os dados:

1. `/search type=track` com `track:"…" artist:"…"` (limite 10, `market=from_token`);
2. `/search type=artist` com `artist:"…"`: IDs dos artistas com o nome exato (cache por importação —
   setlists com o mesmo artista fazem uma busca de artista só);
3. buscas textuais: artista + título, título sem qualificadores, `and`/`&`, singular/plural;
4. discografia de até 2 IDs confirmados: `/artists/{id}/albums` (singles e álbuns, limite 10) e
   `/albums/{id}/tracks` (limite 50) de até 6 lançamentos, primeiro os de nome igual à música;
   faixas de álbum sem relação com o título não entram na revisão;
5. busca só pelo título, apenas se nada do artista apareceu (homônimos ficam separados).

Para assim que a decisão for segura e o artista estiver confirmado (normalmente 1–2 pedidos).
Orçamento de 18 pedidos por referência; esgotado sem resposta = `SEARCH_INCOMPLETE`.
Falha de rede/tempo/5xx: o mesmo pedido é repetido 2 vezes com espera de 1,5 s e 4 s + jitter;
depois vira `LOOKUP_ERROR` (nunca `NOT_FOUND`), mantendo os resultados parciais. 404 de um álbum ou
400 de um filtro só pulam aquele passo. 429 continua no controle já existente de `Retry-After`
(Windows `SpotifyApi`, Android `SpotifyClient`); se persistir, 401/403/429/rede pausam a importação
e as referências restantes continuam pendentes para "Buscar pendentes".

### 1.5 Interface e estado (Windows e Android)

- Seções: Encontradas e selecionadas, Equivalentes para confirmar, Versões alternativas, Só artista
  errado, Não encontradas, Indisponíveis na região, Erro / busca incompleta, Ignoradas, Aguardando.
  "Só pendências" = tudo que não está selecionado nem ignorado. Contadores vêm dos mesmos campos.
- Cada cartão mostra o motivo da decisão. "Gravações" lista primeiro o artista pedido, com classe e
  motivo; outros artistas aparecem sob "Resultados não correspondentes" e pedem confirmação explícita.
- Ignorar, Tentar de novo (só aquela referência) e Editar busca têm semântica própria. Escolha ou
  ignorar manual vira **UserOverride**: vale durante a busca e é preservada por "Buscar pendentes" e
  por respostas tardias, até Tentar de novo ou Editar busca. "Buscar pendentes" não refaz
  referências selecionadas.
- Ao salvar, cada faixa grava em `importAudit` a referência pedida, a URI escolhida, o código da
  decisão, o motivo e se foi manual (até 500 entradas, saneadas no backup, sem tokens nem letras).
  Repetições intencionais continuam preservadas; deduplicar continua opcional.

### 1.6 Matriz antes/depois dos 15 casos dos prints

Fixtures (resultados exibidos nos prints, `tests/playlist/CoreCases.hpp`) e leitura real
(`TonalSpotifyRecoveryTests --text-lookup-readonly`, sessão Spotify existente deste PC, só GET):

| Referência | Antes (0.18.1) | Depois — fixture | Depois — catálogo real |
|---|---|---|---|
| Remember Sports — Get Bummed Out | selecionada | EXACT, selecionada | EXACT, selecionada |
| Dr. Dog — Heart It Races | revisar (live pré-escolhida) | ALTERNATE, revisar | ALTERNATE: só existe a versão ao vivo após conferir a discografia |
| The Wonder Years — Passing Through a Screen Door | selecionada | EXACT, selecionada | EXACT, selecionada |
| Thin Lips — Non-Monogamy Nightmares | **revisar (falso negativo)** | EQUIVALENT (plural), selecionada | EQUIVALENT, selecionada |
| Cayetana — Age of Consent | **revisar (falso negativo)** | EQUIVALENT (crédito), selecionada | EQUIVALENT, selecionada |
| Kurt Vile — Baby’s Arms | selecionada | EXACT, selecionada | EXACT, selecionada |
| Algernon Cadwallader — Look Down | revisar | EQUIVALENT (prefixo), confirmar | EQUIVALENT, selecionada: título real é `Look Down (Because The Ground…)` |
| The Superweaks — Glutton | **Buckcherry pré-escolhida** | NOT_FOUND, nada pré-escolhido | NOT_FOUND após discografia; homônimos separados |
| Free Energy — Dream City | **Balcony Sunrise pré-escolhida** | NOT_FOUND | NOT_FOUND após discografia |
| Free Energy — Hope Child | **Chandelier pré-escolhida** | NOT_FOUND | NOT_FOUND após discografia |
| The Superweaks — Call Me Away | selecionada | EXACT, selecionada | EXACT, selecionada |
| Free Energy — Dance All Night | selecionada | EXACT, selecionada | EXACT, selecionada |
| Free Energy — All I Know | **Screaming Trees pré-escolhida** | NOT_FOUND | NOT_FOUND após discografia |
| Free Energy — Something In Common | **Bobby Brown pré-escolhida** | NOT_FOUND | NOT_FOUND após discografia |
| Free Energy — Girls Want Rock | selecionada | EXACT, selecionada | EXACT, selecionada |

Sobre o Free Energy: a sonda da discografia na leitura real (com e sem `market`) lista 8
lançamentos — *Love Sign*, *Wild Life*, singles —, sem *Stuck on Nothing*, o álbum que contém Dream
City, Hope Child, All I Know e Something In Common. Para esta conta essas gravações não estão no
catálogo disponível. Um token de usuário só enxerga o catálogo do país da conta, então o app não
afirma que a música não existe no Spotify: o motivo diz "discografia conferida no catálogo disponível
para esta conta (pode estar indisponível na sua região)" e oferece substituição manual.

Métricas nos 15 casos (fixtures): selecionadas automaticamente 6 → 8 (+ Look Down como confirmar);
falsos negativos 2 → 0; seleções automáticas erradas 0 → 0; candidatos de outro artista
pré-escolhidos 5 → 0. Catálogo real: 9/15 selecionadas, todas conferidas na saída (artista, título e
álbum corretos), 0 de outro artista; 102 pedidos GET em 42,9 s (os 6 não resolvidos consultam a
discografia; os encontrados usam 1–3 pedidos). Precisão das seleções automáticas: 9/9. Primeira
execução real teve um HTTP 503 em "Look Down" (antes da repetição com espera) e virou LOOKUP_ERROR,
não NOT_FOUND; motivou a repetição automática.

### 1.7 Spotify Web API — fevereiro de 2026 (Development Mode)

Conferido no guia oficial e no changelog: busca continua com limite máximo 10 (padrão 5);
`/artists/{id}/top-tracks`, `/tracks?ids`, `/albums?ids`, `/artists?ids`, `/markets` e `/browse/*`
foram removidos; `popularity`, `available_markets`, `external_ids` (ISRC) e `linked_from` saíram das
faixas; `country` saiu de `/me`. `/artists/{id}/albums` (limite 10) e `/albums/{id}/tracks` (limite 50)
seguem disponíveis; `is_playable` e `restrictions` seguem documentados. Por isso: nada depende de
popularidade, ISRC, top tracks ou país do `/me`; `market=from_token` (documentado na página de
relinking como o país da conta) foi exercido na leitura real com HTTP 200; `linked_from`, quando vier,
só gera uma nota no motivo. Nenhum scraping ou API paralela é usado.

### 1.8 Limites conhecidos

- Não há dados de composição: "mesma composição" é inferida por artista confirmado + título; prefixos
  longos e grafias parecidas ficam para confirmação, exceto quando o resto é um parêntese.
- Gravações fora do catálogo disponível para a conta não podem ser distinguidas de gravações
  inexistentes (limitação do token de usuário).
- Sem teste com Spotify autenticado no Android (AVD sem login) nem em aparelho físico; sem escrita real
  em playlist nesta versão (a adição continua a mesma da 0.17.5, coberta por fixtures).
- Listas de 300 referências foram medidas só com respostas simuladas (300 em 0,27 s de processamento,
  sem rede); com rede real o tempo é dominado pelos pedidos (~1–2 por referência encontrada).

### 1.9 Validação manual

Windows: Tonalify → Biblioteca → Criar com Text to Tonalify → colar as 15 linhas da tabela →
conferir seções, motivos, "Gravações" (artista pedido primeiro; outros artistas pedem confirmação),
Ignorar/Tentar de novo e "Só pendências". Android: Menu da playlist → Text to Tonalify, mesmo roteiro;
"Seções" no seletor acima da lista. Salvar localmente não escreve na conta.

## 2. Fontes e cores das letras

- **12 fontes**, cada uma listada no menu com o próprio desenho (`Nome · Aa Bb Çç 123`): Sans, Serif,
  Mono, Arredondada, Fina, Média, Negra, Condensada, Casual, Cursiva, Versalete, Máquina de escrever.
  Android usa famílias do sistema (`sans-serif-light/medium/black/condensed`, `casual`, `cursive`,
  `sans-serif-smallcaps`, `serif-monospace`); Windows usa fontes do Windows (Segoe UI por peso,
  Bahnschrift, Comic Sans MS, Segoe Script, Courier New, Georgia, Consolas, Trebuchet MS). Nada é
  embutido nem baixado. "Arredondada" usa `sans-serif-rounded`, que alguns Androids não têm: aí o
  menu mostra o mesmo desenho da Sans, sem fingir diferença.
- **12 efeitos** com prévia aplicada no próprio menu, com as cores atuais: Normal, Contorno, Contorno
  grosso, Brilho, Neon, Sombra, Relevo, 3D, Vazado, Marca-texto, Gradiente, Piscante. Os efeitos são
  estáticos (desenho em camadas reaproveitadas); só Piscante anima, como antes, e só tocando.
- **Cores por trilho arco-íris reto** com 16 cores-base, do branco ao preto
  (branco, vermelho, vermelho-laranja, laranja, âmbar, amarelo, lima, verde, verde-primavera, ciano,
  azul-celeste, azul, violeta, magenta, rosa, preto), ajuste de **0 a 1000** e botões **−/+** nas
  pontas para passo de 1 (segurar repete). 990 cores distintas nas 1001 posições. Vale para texto e
  contorno/brilho/sombra, na letra e na tradução. Cores salvas antes são posicionadas no ponto mais
  próximo do trilho. Bolinhas, campo de código e "Aplicar" saíram.
- Os nomes salvos são os mesmos nas duas plataformas (`tests/lyrics/check_style_lists.py`), então o
  backup restaura fonte e efeito entre Windows e Android. Nomes antigos continuam aceitos.
- No Windows, o brilho deixou de ser deslocado para a esquerda em linhas largas (o `shadowScale`
  crescia a partir do centro do item) e, no renderizador por software (modo de segurança gráfica),
  efeitos de sombra/gradiente viram texto simples em vez de texto invisível.

## 3. Evidências

`docs/release-0182-tests.json` (resultados), `docs/text-matching-live-0182.json` (leitura real,
sem credenciais), capturas em `docs/screenshots/0182/`. Ferramentas: nenhum aplicativo novo instalado
ou baixado.
