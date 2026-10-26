# Descoberta 2+1: regras do sorteio

Documentação conferida no código da versão **0.12.0**, em 19/10/2026.
Disponível no Tonalify, para músicas do catálogo Spotify. O núcleo do sorteio
é C++ compartilhado entre Windows e Android; a consulta ao catálogo tem
diferenças entre as plataformas.

## Como usar

1. Abra um álbum ou playlist com pelo menos duas músicas.
2. Ative a **Descoberta 2+1**, identificada pela lupa com play, sinal de mais e setas.
3. Use Reproduzir para aplicar os modos à seleção. Desde a 0.13.1, os botões
   junto da barra de progresso (e os da coleção que está tocando) aplicam o
   modo direto na fila atual, sem recomeçar a música. Para conferir antes,
   abra **Fila**, prepare a prévia e aplique.
4. Confira as entradas marcadas como **Descoberta** e confirme **Aplicar e
   reproduzir fila**, quando estiver na prévia. A faixa atual continua
   tocando: desde a 0.13.0, o Android troca só a ordem seguinte, sem
   recarregá-la; no PC, a Web API reenvia a faixa na posição atual mais a
   latência medida, sem esvaziar o buffer. Detalhes em
   [TEMAS_SEEK_FILA_0130.md](TEMAS_SEEK_FILA_0130.md).

Exemplo: **original A → original B → descoberta X → original C → original D →
descoberta Y**. A playlist salva não é alterada. Depois de preparada, a fila
fica definida: não há outra busca automática a cada terceira música.

## Busca e filtros

- Os estilos vêm dos dados locais, das regras editoriais do projeto ou dos
  metadados do artista Spotify. O Windows também tenta recuperar o identificador
  do artista pela faixa e aproveitar estilos conhecidos de outras faixas da seleção.
- A busca usa até seis consultas de gênero. Quando o ano é válido, tenta
  `genre:"estilo" year:ANO-8-ANO+8` e uma consulta do mesmo gênero sem restringir o ano.
- Cada consulta pode obter dez resultados iniciais e mais dez numa página
  sorteada, dentro de até mil resultados informados pela API. São até doze
  chamadas de busca e até 120 candidatos; consultas de metadados são adicionais.
- O prazo global é **15 segundos no Windows** e **6,5 segundos no Android**.
  Rede, metadados e limites da API podem reduzir bastante o conjunto obtido.
- Rejeita entradas que não são faixas Spotify, arquivos locais, faixas
  explicitamente indisponíveis e nomes normalizados contendo `karaoke` ou `tribute`.
  Esse filtro textual simples não identifica todas as versões, covers ou remixes.
- Rejeita a URI já presente na seleção e o nome normalizado de artista já
  presente nela. Em modo álbum, rejeita também o identificador do álbum de origem.
- Entre sugestões, remove URIs repetidas e admite no máximo uma faixa por
  nome normalizado de artista. Essa comparação de nomes não é uma resolução
  universal de identidades: colaborações e créditos diferentes podem escapar.

O gênero usado na busca é atribuído ao candidato como evidência do filtro do
catálogo. Não é uma análise acústica independente da gravação.

## Compatibilidade: o que ganha pontos

Cada candidato é comparado com até **12 faixas da seleção**, começando na
escolhida e retornando ao início se necessário. Vale a **maior afinidade**
encontrada, não a média e nem uma comparação específica com as duas faixas
imediatamente anteriores na reprodução.

| Fator | Pontuação |
|---|---|
| Gêneros em comum | Se houver interseção: `3 + quantidadeEmComum / maiorQuantidadeDeGeneros`; caso contrário, zero |
| Proximidade do lançamento | Se ambos têm ano: `max(0, 1 - diferençaDeAnos / 20)` |
| Temas das letras em comum | `+0,2` por tema presente nas duas análises locais |

Os gêneros são normalizados e separados por vírgula, ponto e vírgula ou barra
vertical. Um candidato com melhor afinidade **menor que 1,1** é descartado.
Mesmo ano acrescenta um ponto; dez anos de diferença acrescentam meio ponto;
vinte ou mais não acrescentam pontos. O ano sozinho nunca passa pelo filtro.

Os temas possíveis são amor, saudade, perda, rebeldia, esperança, celebração,
tensão e introspecção. Para esse sorteio, entram os escores locais a partir
de **0,35**, desde que a letra já tenha sido analisada. Esse limiar é diferente
do usado para destacar temas no card de repertório.

A versão 0.12 busca letras automaticamente em segundo plano, com limites,
cache de escores e opção de desligar na Fila. Os resultados só influenciam
preparações futuras; a fila confirmada permanece estável. Sem análise em cache
nas duas faixas, esse fator vale zero. Veja [limites e privacidade](DISCOVERY_ENRICHMENT_0120.md).
Não há comparação de BPM, timbre, tonalidade, forma musical ou waveform.

## Novidade e sorteio ponderado

O histórico consultado é o **deste aparelho**, com eventos de tempo ouvido
maior que zero; até uma escuta parcial pode marcar uma faixa como conhecida.
Não é o histórico completo da conta Spotify e não se sincroniza automaticamente
entre PC e celular.

```text
peso = afinidade × (artistaInédito ? 8 : 1) × (faixaInédita ? 2 : 1)
prioridade = log(U) / peso, com 0 < U < 1
```

As prioridades são ordenadas da maior para a menor; depois são removidas
repetições de faixa e artista. É um sorteio ponderado por corrida exponencial.

| Situação no histórico local | Multiplicador da afinidade |
|---|---:|
| Artista e faixa inéditos | 16× |
| Artista conhecido, faixa inédita | 2× |
| Artista e faixa conhecidos | 1× |

Esses são pesos relativos, sem garantia de que uma faixa seja escolhida ou
de que uma repetição seja impossível. Não há fator de popularidade nem uso
de um endpoint de recomendação personalizada Spotify nesse algoritmo.

Cada nova preparação renova a semente. Ao reorganizar uma fila com descobertas
aceitas, preserva as mesmas músicas externas; um novo sorteio de candidatas exige
desligar e reativar a Descoberta ou abrir outra seleção. O mesmo conjunto pode produzir outra
ordem e outras sugestões, mas um conjunto pequeno ainda pode se repetir.
Sementes fixas servem para reproduzir testes.

## Relação com Shake e Aleatório

- **Shake:** Fisher–Yates, muda a ordem visual temporária; cada toque sorteia
  de novo e segurar restaura a ordem original. O núcleo evita devolver a
  identidade para uma lista com mais de uma faixa.
- **Aleatório:** usa prioridades aleatórias independentes para a reprodução,
  preservando uma música explicitamente escolhida como primeira. O botão Play
  da coleção também pode sortear a primeira antes de chamar o planejador.
- **Descoberta 2+1:** faz o sorteio ponderado das faixas externas e as intercala.
  Funciona junto do Shake e do Aleatório, sem perder as externas aceitas.
  Tocar novamente no botão desliga o modo; Aplicar confirma a remoção das
  próximas externas na prévia. A descoberta atual não é interrompida.
  Com poucas originais restantes, externas já aceitas podem ficar ao final. A cada duas faixas originais, tenta inserir
  uma descoberta; uma descoberta não conta como faixa original para a cadência.

## Limites que afetam o resultado

A fila final tem **até 500 faixas**. O planejador reserva espaço para as
originais restantes antes de inserir extras. Assim, se a seleção ocupar as
500 posições, **nenhuma descoberta cabe**. Se faltarem candidatos compatíveis,
a fila segue com as originais; a proporção 2+1 não é garantida até o fim.

O Windows trabalha com uma janela de até 500 faixas, deslocando-a quando a
faixa escolhida está além das primeiras 500. O Android limita a entrada às
primeiras 500; a interface pode carregar menos faixas do que a coleção completa.
O algoritmo não percorre todo o catálogo Spotify: sorteia entre candidatos
obtidos de páginas limitadas de busca. Não é uma distribuição uniforme global.

Uma troca real de música invalida a prévia antiga. Atualização de metadados da
mesma faixa não deve cancelar sua preparação. Erros de autorização, limite de
consultas e falta de estilos podem impedir sugestões; no Windows há mensagens
específicas para essas situações.

## Código e evidências

- [Planejador, afinidade, sorteio e consultas](../src/playback/ListeningMixCore.cpp).
- [Catálogo, histórico e prévia no Windows](../src/playback/ListeningMix.cpp).
- [Catálogo e histórico no Android](../android/app/src/main/java/app/tonal/android/ListeningMix.java).
- [Regressões do núcleo](../tests/mix/CoreChecks.cpp).
- [Enriquecimento e retenção 0.12](DISCOVERY_ENRICHMENT_0120.md).
- [Correções e validação 0.11.1](DISCOVERY_PLAYBACK_0111.md).
- [Guia visual](GUIA_VISUAL.md).

Original Tonal: **Matheus Nunes (natheus)**, [tonal-player](https://github.com/natheus/tonal-player).

## Convivência com Jam experimental (0.15.1)

A janela de fila confirmada pelo serviço é alinhada por ocorrências à seleção
preparada. Entradas correspondentes conservam os marcadores de Descoberta, e a
cauda conhecida fora dessa janela é preservada. Uma faixa adicionada por outra
pessoa não vira automaticamente uma descoberta: desligar Descoberta só remove
as externas marcadas pelo algoritmo. Nenhuma autoria é deduzida pelo alinhamento.

Atualizar o cache não recarrega a faixa audível. Shake/Aleatório continuam
independentes, dentro do limite de 500 entradas. Acima dele, reaplicar os modos
é recusado com aviso, sem truncar músicas. A janela remota é parcial; não há
promessa de reconstruir uma fila remota inteira desconhecida.
