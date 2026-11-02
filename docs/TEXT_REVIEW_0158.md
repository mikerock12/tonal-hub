# Text to Tonalify 0.15.8 — filtro de não selecionadas e revisão com capa

Entrega Windows/Android 0.15.8, 01/11/2026. Corrige a revisão de playlists
criadas por texto e melhora as informações exibidas antes de salvar.

## Problema relatado

Numa criação de playlist com 100 músicas, 89 correspondências foram
selecionadas, mas o filtro **Só não encontradas** mostrava zero: as 11
restantes estavam com o status **Revisar versão** e o filtro só considerava
o status **Não encontrada**. As referências que mais precisavam de revisão
ficavam invisíveis no modo filtrado.

## Correção do filtro

O filtro passa a listar **todas as referências não selecionadas**, nas duas
plataformas (`!selected` em `missingCount`/`reviewIndices` no Java e
`missingCount`/`reviewRows` no QML). Entram no filtro:

- referências com status **Não encontrada**;
- correspondências duvidosas desmarcadas automaticamente (**Revisar versão**);
- qualquer faixa desmarcada manualmente.

A contagem do botão acompanha a mesma regra. Marcar uma música continua
atualizando apenas o cartão dela, sem recriar a lista nem mover a rolagem.

## Revisão e diálogo Versão com capa

Os cartões da etapa Revisar e o diálogo **Versão** agora mostram música,
artista, álbum e miniatura da capa da gravação escolhida:

- Android: cartão com capa de 48 dp, título, artista/status, gravação
  escolhida e álbum, identificados pelas tags `review-check`, `review-status`,
  `review-track`, `review-album` e `review-cover`. O diálogo de escolha usa um
  `ArrayAdapter` próprio com capa de 56 dp. O carregador `Artwork` é fechado
  no `dismiss` do assistente.
- Windows/QML: cartão com miniatura de 56 px e o novo `versionDialog`
  (objectName `versionDialog`) com miniaturas, música, artista e álbum;
  a seleção passa por `chooseReview()`, que preserva a posição rolada.

Sem miniatura disponível, o espaço mostra um marcador estático (♪ / painel).
Nenhuma imagem é buscada fora dos metadados já obtidos na busca Spotify da
própria etapa.

## Validação

- QML do assistente: 3/3, mock com 30 linhas (quatro Não encontrada, uma
  Revisar versão); o filtro conta 5 e a marcação não move a rolagem.
- Catálogo de temas: 243 checagens, 8 temas (regressão).
- CTest Windows: 10/10 (regressão).
- Android: compilação `benchmark` e `release`; smoke sintético na AVD
  dedicada Tonal_Local_20261015 (porta 5560) com o extra de teste
  `textPlaylistUncertain`, que deixa "Música 2" como Revisar versão. O filtro
  **Só não encontradas (2)** listou Shove (Não encontrada) e Música 2
  (Revisar versão), sem Seether; a rolagem e a marcação foram preservadas.

Relatório de testes: `release-0158-tests.json`. Pacotes e hashes:
`artifacts-0158.json`.

## Limites

- Testes com dados sintéticos; sem teste físico, streaming Spotify
  autenticado, escrita real de playlist ou benchmark novo nesta entrega.
- As miniaturas dependem das URLs de imagem retornadas pela busca; sem a
  sessão Spotify, a revisão continua funcional com o marcador estático.
- Houve instabilidade inicial da AVD (ANR do System UI e de apps em segundo
  plano do emulador sob pressão de E/S do hospedeiro), resolvida com cold
  boot; registrada em `release-0158-tests.json`, sem alteração do produto.
