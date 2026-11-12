# Edição da fila no Tonalify 0.15.14

Na **Fila**, o resumo conta as ocorrências das próximas músicas, os álbuns e
artistas distintos e a soma das durações conhecidas. A música atual aparece
destacada acima. O sinal ≥ indica que há músicas sem duração no catálogo.
O Spotify pode informar só uma parte da fila. O resumo cobre as faixas
disponíveis na lista exibida.

No menu da música, **Adicionar à fila** insere depois da ocorrência mais
recente adicionada por esse comando que ainda esteja na fila, inclusive
se ela tiver sido movida. Se não houver nenhuma,
insere logo após a faixa atual. As ações **Colocar como próxima da fila** e
**Colocar como última da fila** continuam disponíveis. Exemplo: adicionar B,
C e Z produz Atual → B → C → Z → demais faixas.

Arraste a alça ao lado de uma faixa. As vizinhas abrem espaço à medida que
ela atravessa a lista. Soltar aplica a ordem e cancelar o gesto a restaura.
Em **Editar**, marque as faixas e use **Subir**, **Descer** ou **Excluir**.
Subir ou descer desloca cada bloco marcado uma posição, preservando sua ordem.
**Limpar** pede confirmação e remove todas as próximas. Repetições da mesma
faixa são ocorrências independentes. Os modos de descoberta e embaralhamento
continuam com sua própria prévia.

Desde a 0.15.15, o editor atualiza somente as próximas músicas no motor local.
A edição mantém o decoder, a posição e o estado de reprodução ou pausa.
O motor confere a faixa atual antes de aplicar a ordem. O limite continua em
500 entradas contando a faixa atual.

No Windows, a edição requer o dispositivo Tonalify neste PC e o librespot
incluído no pacote. A reprodução em outros dispositivos Spotify Connect
continua disponível.
# Fila longa na versão 0.15.16

A janela de próximas faixas do Spotify pode conter só 20 itens. Windows e
Android consultam as páginas do álbum ou da playlist e acrescentam a sequência
restante quando a janela identifica uma ocorrência única da faixa atual.
Preservam-se as inserções do início da janela e as ocorrências repetidas.
O resumo considera todas as próximas faixas identificadas. No Android,
**Mostrar mais músicas** expande a lista em lotes de 24.

A edição aceita até 500 entradas incluindo a faixa atual. Se a ordem não puder
ser confirmada, como em uma fila externa embaralhada, o app identifica a fila
como parcial e impede alterações que descartariam faixas.
