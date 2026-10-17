# IndieTonal 0.1.1 — busca e shows

Verificado localmente em 16/10/2026. Sem commit, push ou PR.

## Problemas corrigidos

- A ordem das respostas HTTP definia a ordem dos resultados: perfis do Audius
  podiam aparecer antes da identidade exata da banda no MusicBrainz. Agora a
  busca classifica por nome/título/artista, preservando a fonte e as versões.
  A pontuação retornada pelo MusicBrainz desempata homônimos, como Nirvana.
- HTTP 503 encerrava imediatamente a navegação. Agora o MusicBrainz tem espera,
  até duas novas tentativas, cache público limitado e repetição manual da página.
- A busca de shows incluía páginas de coleção. Agora exige gravação `etree` e
  identifica resultados relacionados/tributos/menções. Há filtros com contagem.
- O nome do artista podia ficar vazio ao navegar por álbuns: agora é preservado.

## Bandas solicitadas

Consulta: `collection:etree AND mediatype:etree AND (creator:"BANDA" OR title:"BANDA")`.
Isso cobre o Live Music Archive, não todo o Internet Archive. Contagens podem
mudar. Correspondência exata de metadados não prova identidade/autenticidade.

| Banda | Resultado desta consulta |
|---|---|
| Nirvana | 6 gravações relacionadas; nenhuma com nome exato do artista. Incluem tributos, não foram tratadas como shows da banda original. |
| Alice In Chains | 0 |
| Metallica | 0 |
| Soundgarden | 0 |
| Green Day | 0 |
| Ramones | 0 |
| The Distillers | 0 |
| Veruca Salt | 0 |
| Dinosaur Jr. | 0 |
| King Gizzard & The Lizard Wizard | 494 gravações; 20 primeiras com artista exato nos metadados |
| Goose | 943 gravações; 20 primeiras com artista exato nos metadados |

Foram inspecionados dois itens de King Gizzard (12 e 27 arquivos MP3 públicos)
e dois de Goose (22 e 20), sem restrição indicada nesses metadados.
Um trecho de King Gizzard passou pelo decodificador e DSP nativos por 1.952 ms,
com zero underruns e amostras inválidas. Saída de teste OpenAL nula: não é uma
avaliação auditiva, Bluetooth ou ensaio longo de estabilidade.

MusicBrainz real: Veruca Salt → All Hail Me → edição de 1995 → três faixas.
Isso é discografia, não áudio de streaming fornecido pelo MusicBrainz.

Os dados brutos, consultas e resultados estão em:

- `out/research/live-bands-probe.json`
- `out/research/indie-veruca-tests.json`
- `out/research/indie-live-rock-tests.json`
- `out/research/hub-ctest.xml`
- `docs/tonal-hub-tests.json` (consolidado do pacote)

O acervo atual é insuficiente para oferecer shows originais das bandas
solicitadas nesta consulta. Não se apresenta ausência como erro de conexão nem
tributos como material original. Expandir a cobertura exige avaliar outras
fontes; o filtro de shows não cria disponibilidade que a fonte não retorna.

## Abrir a correção

Feche a instância antiga do IndieTonal e abra:
`out/release-local/Tonal-Hub-0.1.1-local/IndieTonal.exe`.
Para conferir shows disponíveis, busque King Gizzard & The Lizard Wizard e
selecione **Shows ao vivo**. No MusicBrainz, procure Veruca Salt para conferir
a identidade da banda antes de navegar na discografia.

Referências oficiais:

- https://musicbrainz.org/doc/MusicBrainz_API/Rate_Limiting
- https://internetarchive.readthedocs.io/en/stable/metadata.html
- https://archivesupport.zendesk.com/hc/en-us/articles/360016553532-Live-Music-Archive-etree-org
