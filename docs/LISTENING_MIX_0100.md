# Tonalify 0.10.0 — ícones e modos de escuta

O cabeçalho do Tonalify acompanha as páginas pessoais no Android: marca, busca, favoritas, efeitos e conta. A navegação usa ícones com descrição de acessibilidade e dica ao manter pressionado: disco (Agora), biblioteca, gráfico (Estatísticas), linhas (Letras), download (Backup) e X (Fechar). No Windows o painel pessoal deixa o cabeçalho principal visível e acessível.

Nos álbuns/playlists do catálogo e na biblioteca pessoal:

- **Shake** (linhas com setas): embaralha temporariamente a ordem exibida. Um segundo toque restaura a ordem original. Não reorganiza nem grava a playlist no Spotify.
- **Aleatório** (dado): prepara a sequência aleatória de reprodução. Não muda a ordem visual da lista.
- **Descoberta 2+1** (bússola): no Tonalify, intercala uma faixa externa compatível depois de duas da seleção, enquanto houver sugestões disponíveis. Pode ser combinado com o aleatório.

Escolha os modos e pressione **Reproduzir** no álbum ou em uma faixa. Alterar opções não reinicia nem muda a música que já está tocando. Desative as opções e reproduza novamente para voltar à sequência normal. As filas preparadas são temporárias e não alteram suas playlists salvas.

## Afinidade e descoberta

O mesmo C++20 compartilhado entre Windows e Android organiza e pontua as filas. O gênero vem de metadados disponíveis, filtros de busca Spotify ou relações editoriais locais identificadas como tal; não se inventa uma medição acústica. Quando há ano de lançamento, a busca prioriza uma época próxima, com uma segunda consulta mais aberta. Temas das letras só influenciam a comparação quando já foram analisados e guardados localmente para as duas faixas. Não é uma análise do áudio, de BPM, ou de letras ainda não disponíveis.

A descoberta pesquisa diferentes páginas do catálogo por estilo/época, evita versões identificadas como karaoke/tributo, exclui faixas e artistas da seleção e, para álbuns, outras faixas do mesmo álbum. Prefere artistas ausentes do histórico deste aparelho, evita repetir a mesma banda entre as sugestões e conserva as reprises da seleção original. Histórico local ausente, apagado ou com nomes diferentes não permite prometer que você nunca ouviu um artista em outro aparelho.

Se não houver gêneros ou candidatos compatíveis, se a API negar acesso ou a rede falhar, a seleção original continua. O app informa quantas faixas externas foram preparadas; ao acabarem, segue com as músicas restantes do álbum/playlist. Uma nova reprodução pode preparar outro conjunto. Não usa o endpoint de recomendações do Spotify.

## Recursos e limites

A busca só acontece ao solicitar uma reprodução com descoberta. Não há varredura contínua, download de letras de todos os candidatos, modelo grande ou serviço adicional em segundo plano. O preparo tem orçamento de 6,5 segundos; callbacks atrasados e novas escolhas são descartados. O áudio atual continua enquanto a nova fila é preparada.

O preparo mantém até 500 faixas da seleção, 120 candidatos e 500 itens na fila final. Quando o limite não comporta mais sugestões externas, preserva as faixas originais da seleção. Coleções maiores têm o limite informado na interface; a reprodução normal por contexto Spotify continua disponível. A ordem visual e a confirmação de páginas carregadas são separadas da ordem remota, inclusive para faixas repetidas. As telas Android renderizam inicialmente até 100 faixas por seção, com Mostrar mais, sem criar 500 linhas visuais a cada página recebida.

Nenhuma mudança foi feita no processamento PCM, equalização ou buffer de áudio por esses controles. Os testes incluem ordenação, descoberta 2+1, repetição de faixas, novidade, exclusões, falha de API, cancelamento e respostas atrasadas. Resultados: `listening-mix-0100-tests.json`.

Créditos ao Tonal original de Matheus Nunes (natheus), preservados em CREDITS.md e nos pacotes.
