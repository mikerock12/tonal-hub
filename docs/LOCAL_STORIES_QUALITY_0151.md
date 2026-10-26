# Leitura de letras — correção 0.15.1 em validação

Estado: **não entregue**. Não instalar os protótipos como se fossem o produto final.

O usuário testou a 0.15.0 no telefone: oito letras produziram somente
“O conjunto reúne letras que contrastam e contrastam.” A análise não passou
na aceitação. Os testes anteriores com descrições sintéticas simples verificaram
execução e isolamento do áudio, mas não demonstraram interpretação útil de um
lote real. O relatório histórico foi corrigido para explicitar esse limite.

## O que está sendo corrigido

- Ler cada trecho separadamente e vincular a resposta à faixa correspondente.
- Mostrar a leitura por música, em vez de apenas uma frase genérica e uma lista de fontes.
- Invalidar os resumos antigos: cache v4 e relatório estruturado com índices
  conferidos contra o lote exato. O esquema antigo não é reaproveitado.
- Recusar respostas repetitivas, eco das instruções e cópia extensa de versos.
  Esses filtros são heurísticos; não provam que a interpretação é verdadeira.
- Manter preparação fora da UI, limites de memória, processo separado,
  cancelamento, prioridade de áudio e espera de pausa no Windows e Android.

## Avaliação de conteúdo

Usam-se oito textos curtos originais em português/inglês, incluindo metáforas
sobre saudade, autonomia, desigualdade, exclusão, identidade, recuperação,
ironia e afeto. A saída deve ser inspecionada quanto ao sentido, não apenas
quanto ao tamanho. Modelos pré-treinados não são chamados de treinados pelo Tonal.

Os protótipos Qwen2.5 0.5B, Qwen3 0.6B e Qwen3.5 0.8B produziram interpretações
erradas, genéricas ou em idioma inadequado em parte desses casos. Não foram
aprovados para entrega. Pesos e saídas de avaliação ficam em out/, fora do Git;
revisões/hashes e evidências finais serão registrados ao escolher a solução.

A execução de oito entradas, o número de tokens ou um CI verde de compilação
não equivalem a uma validação da qualidade semântica. Nenhuma inferência descreve
a saúde ou o estado emocional do usuário.

## Candidato maior autorizado

Em 25/10 o usuário escolheu priorizar qualidade com um modelo de cerca de
1,2 GiB. Candidato Qwen3.5 2B Q4_K_M (quantização Unsloth), pré-treinado,
Apache 2.0: 1.280.835.840 bytes. Revisão e SHA-256 em
resources/local-stories-model.json. A escolha não significa aprovação automática
da qualidade. Resultados experimentais ainda erram nuances e metáforas.

A implementação v4 permite resultado parcial no prazo e Continuação por ação
do usuário, reutilizando leituras aceitas do lote exato. Processamento é pausado
com a reprodução também no Windows, além do Android. Não há download automático
do modelo maior nem retenção dele no player durante uso musical normal.

## Lote real de desenvolvimento — resultado limitado

Foram obtidas quatro letras identificadas por faixa/artista no LRCLIB; a
indisponibilidade HTTP 503 impediu completar o lote original de oito. Os textos
e saídas privadas ficam fora do Git e dos artefatos públicos.

A preparação remove linhas repetidas antes de selecionar até 3.000 bytes UTF-8,
incluindo início/meio/fim quando necessário. Cada passe ainda respeita 768 tokens
de entrada. A interpretação passa por uma segunda consulta ao próprio modelo,
além de filtros de formato. Esse revisor também pode errar: não é validação humana.

Na última rodada de 90 segundos, apenas **1/4** leituras passou pela conferência;
duas foram inconclusivas e uma ficou pendente. A continuação de uma rodada
anterior conservou a leitura válida e concluiu as demais como inconclusivas.
O resultado demonstra cobertura limitada; não comprova qualidade geral para o
repertório. Não foi resolvido simplesmente aumentando o tamanho dos pesos.
O produto mostra cobertura e leituras por faixa, sem preencher o card com frases
genéricas. Continuar análise reutiliza apenas o lote e a revisão exatos.
