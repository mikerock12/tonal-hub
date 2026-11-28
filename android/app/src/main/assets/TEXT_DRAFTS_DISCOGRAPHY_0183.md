# Text to Tonalify V3.1 — discografia completa, cadastros homônimos, rascunhos e mais músicas (0.18.3)

Windows 0.18.3 e Android 0.18.3 (versionCode 53), 28/11/2026. Relatório pedido por
`TONALIFY_V3_1_RASCUNHOS_E_ADICIONAR_MUSICAS.md`: causa raiz por artista, antes/depois, limites da API,
persistência e testes. Complementa [TEXT_MATCHING_LYRIC_STYLE_0182.md](TEXT_MATCHING_LYRIC_STYLE_0182.md);
todas as salvaguardas da 0.18.2 continuam (barreira de artista, versões, erro ≠ não encontrada).

## 1. Diagnóstico com instrumentação (AUD-01)

`TonalSpotifyRecoveryTests --text-lookup-readonly --text-file <lista>` passou a registrar cada pedido real
(rota, filtro, offset, limite, mercado, HTTP, itens, total, próxima página) por referência, sem tokens.
Leitura real com a sessão Spotify existente deste PC (só GET), 18 referências (12 Ohio + 2 Scrawl + regressões):
`docs/text-ohio-live-0183.json` (antes e depois).

| Grupo | Causa raiz comprovada pelo rastreamento |
|---|---|
| The Dopamines (6) | 13 lançamentos, mas a 0.18.2 abria só 6 (limite fixo) e começava pelos singles; os álbuns nunca eram abertos. |
| New Bomb Turks (6) | Só `album` e `single` eram listados: coletâneas e participações ficavam fora. |
| Frase "discografia conferida" | Era escrita mesmo com o corte acima — afirmação sem varredura completa. |
| Scrawl (2) | O Spotify tem **dois cadastros "Scrawl"**. A 0.18.2 comparava só IDs; não comparava discografias. Os nomes extras (Marcy Mays, Sue Harshe, Carolyn O'Leary) são créditos dos integrantes, não outro intérprete. |

## 2. O que mudou no motor (`TextPlaylistCore`, compartilhado por Windows e Android)

- **Discografia completa e paginada:** para até 3 cadastros com o nome pedido (o fixado primeiro): todas as
  páginas de `album,single,compilation` (10 por página, até 300 lançamentos), depois `appears_on` (até 50; a faixa
  só conta se o artista pedido for o intérprete principal), e as faixas de **cada** lançamento (50 por página,
  até 200). Lançamentos com nome igual à música vêm primeiro; a busca para ao achar correspondência segura.
- **Honestidade do estado:** "discografia completa conferida (N lançamentos …)" só quando todas as páginas e
  faixas foram lidas. Corte por limite de consultas ou erro → `SEARCH_INCOMPLETE`/`LOOKUP_ERROR` com o motivo e
  "Tentar de novo" ("Buscar na discografia"), nunca `NOT_FOUND`.
- **Variantes de título:** `Part`/`Pt.`/`pt` e numerais romanos depois de part/vol/chapter/no têm a mesma chave
  (`You're So Vain Part II` = `You're So Vain, Pt. 2`); romanos soltos não mudam (`Rocky II` ≠ `Rocky 2`).
- **Cadastros com o mesmo nome:** classe nova `ARTIST_AMBIGUOUS`. Dois cadastros cujas discografias compartilham
  ≥ 3 títulos são tratados como o mesmo artista (cadastro duplicado), com preferência pelo cadastro com mais
  lançamentos e pela gravação original (álbum, ano mais antigo). Sem títulos em comum, a escolha fica com o
  usuário, com os álbuns citados. O usuário pode **fixar o cadastro** para todas as músicas daquele artista na
  lista (persistido no rascunho, com desfazer pela nova escolha).
- **Cache por importação:** respostas de busca e discografia são reaproveitadas na mesma lista (Windows e
  Android); um artista com 6 músicas tem a discografia lida uma vez.
- **Ritmo:** intervalo entre consultas passou de 120 ms para 350 ms. Repetições de 5xx/rede continuam com
  espera crescente; 429 respeita `Retry-After` e pausa a lista.

## 3. Antes/depois das 14 referências Ohio (catálogo real)

| Referência | Antes (0.18.2) | Depois (0.18.3) |
|---|---|---|
| Scrawl — Charles | AMBIGUOUS ("mais de um artista chamado Scrawl") | EXACT, selecionada; 2 cadastros com 5 músicas em comum = mesmo artista |
| Scrawl — Clock Song | AMBIGUOUS | EXACT, selecionada (Bloodsucker); idem |
| New Bomb Turks — Born Toulouse-Lautrec, Tail Crush, Up for a Downslide, Tattooed Apathetic Boys, Dragstrip Riot | NOT_FOUND após 6 lançamentos (sem coletâneas), "discografia conferida" | NOT_FOUND após **varredura completa de 20 lançamentos** (com coletâneas e participações) |
| New Bomb Turks — Long Gone Sister | NOT_FOUND | LOOKUP_ERROR: limite do Spotify (429) — não virou "não encontrada" |
| The Dopamines — You're So Vain Part II, Useless, You're Ruining My Life, Don't Mosh the Organ, Paid in Full | NOT_FOUND após 6 de 13 lançamentos | NOT_FOUND após **varredura completa de 17 lançamentos** |
| The Dopamines — Kitchen Cleaners | NOT_FOUND | não consultada nessa execução (limite do Spotify; continua pendente) |

Conclusão honesta: com a varredura completa, as 11 músicas de New Bomb Turks/The Dopamines consultadas **não
estão no catálogo disponível para esta conta** (o token de usuário só enxerga o catálogo do país da conta; não
afirmamos que não existam no Spotify). O app não força 200/200 e não aceita homônimos. As duas Scrawl foram
recuperadas. Regressões (Free Energy, Dr. Dog, Buffalo Tom "Soda Jerk" = "Sodajerk") seguem nos fixtures.

**Incidente:** as leituras de diagnóstico desta entrega (≈ 500 pedidos em 4 execuções) levaram o Spotify a
responder 429 com `Retry-After` de 61.150 s (~17 h) para a aplicação. O app tratou como erro (lista pausada,
pendências mantidas). Por isso a última execução não cobriu todas as linhas e o intervalo entre consultas
aumentou. Listas grandes com muitas músicas ausentes custam ~15–30 pedidos por artista na primeira varredura.

## 4. Rascunhos (DRAFT-08)

- Arquivo JSON versionado por rascunho (`schemaVersion` 1) ao lado da biblioteca: Windows
  `…/personal/text-drafts/<draftId>.json` (QSaveFile), Android `files/text-drafts/<draftId>.json`
  (temporário + fsync + renomear). Leitura de volta antes de dizer "salvo". Falha de disco/permissão mostra erro
  e não sai.
- Conteúdo: ids e ordem das referências, texto original, artista/título, lote, decisão e motivo, escolha
  (8 candidatos + o escolhido), seleção manual, ignoradas, substituições, duplicatas, cadastros fixados, lotes
  (`batchId`, data, rótulo, impressão digital), capa, formato, mercado (`from_token`) e
  `publication: not_started` (rascunho é só local). Sem token, credencial ou URL de autenticação.
- Saída: X, Fechar, Voltar do Android, Esc e "Interpretar" de novo perguntam **Salvar rascunho e sair / Sair sem
  salvar / Continuar editando** quando há mudanças. Toque fora e Voltar com a pergunta aberta não descartam.
  Sair sem salvar mantém a versão salva anterior.
- Entrada: "Continuar rascunho" com nome, músicas, selecionadas, pendentes e data; Continuar, Renomear e Excluir
  (com confirmação). Até 20 rascunhos; nenhum é apagado sem pedido. Salvar a playlist no Tonalify conclui e
  remove o rascunho.
- Cópia automática a cada mudança (3 s) só para queda do app/processo: aparece como "Recuperação automática"
  quando é mais nova que o rascunho salvo; não substitui a pergunta de saída.
- Limite conhecido: o rascunho não guarda a conta Spotify; o destino é escolhido de novo em Personalizar.

## 5. Adicionar mais músicas (APPEND-09)

- "Adicionar mais músicas" usa o mesmo leitor de listas e mostra a prévia: reconhecidas, novas, já na lista
  (com a linha) e "esta mesma lista já foi adicionada".
- **Adicionar:** novo lote no fim; **Substituir pendentes:** cada música nova ocupa o lugar de uma pendência
  escolhida (não encontradas, indisponíveis, ignoradas…); a original fica como "Substituída" para auditoria. O
  total ativo se mantém; contadores mostram ativas, substituídas, selecionadas, pendentes e "a salvar".
- Só as linhas novas são buscadas; escolhas, capa e pendências antigas não mudam. "Desfazer última importação"
  remove o lote e devolve as substituídas. Gravação já selecionada em outro lote fica desmarcada como "Já está na
  playlist" (pode marcar mesmo assim). Repetições dentro da mesma lista colada continuam intencionais.

## 6. Testes

- Núcleo (C++, Windows e CI Linux): paginação completa com coletânea, corte → incompleta, Part II/Pt. 2, Scrawl
  (cadastros duplicados, homônimos reais e fixado), chave de duplicata e impressão digital.
- Importador Windows: DRAFT-02 (reabrir em outra instância: ordem, escolha manual, URIs), DRAFT-03, DRAFT-04,
  DRAFT-05, cópia automática, APPEND-01/02/03/04/06/07, duplicata por URI, cadastro fixado aplicado à lista,
  300 referências.
- QML Windows: saída com 3 ações (inclusive Esc), adicionar músicas, rascunho na entrada, sair sem salvar.
- AVD: `tests/android/text_draft_smoke.py` (saída, adicionar, salvar, morte do processo, retomar, sair sem
  salvar, excluir, falha de gravação) + regressões do Text to Tonalify e das letras.
- Sem Spotify autenticado no Android, aparelho físico, escrita real de playlist ou teste de reinício do celular
  (a morte do processo foi simulada com `am kill`).
