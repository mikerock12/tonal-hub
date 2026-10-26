# Seu ritmo e histórias das músicas — histórico 0.15.0

Estado atual do leitor: [retomada 0.15.2](READER_RESUME_0152.md). A 0.15.1
foi entregue como experimental; as abas internas da música foram simplificadas
e os atalhos globais continuam disponíveis. Os detalhes abaixo preservam a
descrição da implementação 0.15.0 e suas limitações.

Windows e Android. Os atalhos **Estatísticas** (gráfico) e **Backup** (salvar)
ficam no cabeçalho global, junto de busca, favoritas, áudio e conta. Também
permanecem acessíveis quando a página da música está aberta. As abas internas
da música ficam com Agora, Biblioteca e Letras.

## Histórico e espaço ocupado

Seu ritmo oferece **1 semana, 1 mês, 3 meses, 6 meses, 1 ano e Desde o começo**.
O valor zero significa todo o histórico disponível, sem voltar silenciosamente
para sete dias. O corte usa o início do dia no fuso local. Os filtros de meses
correspondem a 30/90/180 dias e o de ano a 365 dias.

Escutas deixam de ser apagadas automaticamente depois de 180 dias. O SQLite
preserva sessões e metadados históricos; o cache normal continua limitado às
10.000 sessões recentes e a 3.000 metadados recentes, além das referências da
biblioteca. Consultas ao arquivo e agregação acontecem fora da interface. Uma
sessão em andamento substitui a cópia persistida, sem somar duas vezes. O perfil
de estilo é calculado uma vez por faixa em cada consulta, não por escuta.
Arquivos que versões anteriores já apagaram só podem ser recuperados de backups
anteriores que ainda os contenham.

O rodapé informa os **bytes reais do SQLite + WAL + SHM** e o tamanho do
**último backup exportado com sucesso**. É uma fotografia do arquivo na hora da
exportação; o arquivo pode ter sido movido ou excluído depois. Não se trata de
backup automático nem sincronização. Modelo e cache de análise têm medição
separada dentro do card de histórias.

Exportação e restauração completas usam a fila de disco, incluindo sessões
arquivadas fora do cache recente. Mantêm o formato JSON v1 e a separação entre
Tonalify e IndieTonal. Importação tem proteção de 64 MiB, 250.000 sessões,
100.000 metadados e 100 playlists; exportação recusa ultrapassar esses limites.
Não existe expiração por idade no SQLite. Tokens, senhas, modelo e letras
completas não entram nesse arquivo. Restauração substitui a biblioteca local;
credenciais continuam no armazenamento protegido do sistema.

## Por que o card antigo ficava vazio

Minutos e número de músicas ouvidas não significam que suas letras foram
capturadas. O TonalMuse Tiny v1 é um classificador de 32 KiB treinado em exemplos
sintéticos. Seu limiar de temas pode não ser atingido por letras reais. O novo
recurso não reduz esse limiar para apresentar conclusões inventadas: usa um
leitor generativo local separado e mostra exatamente quais músicas entraram.
O card vazio do classificador antigo deixa de ocupar espaço; seus temas ainda
aparecem quando há resultados.

## Leitor local opcional

Em **Estatísticas → Clima do repertório → As histórias das suas músicas**:

1. Toque em **Baixar modelo · 1,19 GiB**. É necessário fazer isso uma vez no
   aparelho/player; o download pode ser cancelado e só é ativado após verificar
   tamanho exato e SHA-256.
2. Toque em **Analisar letras**. O app seleciona até oito das faixas mais
   ouvidas no período. Usa as letras já capturadas e consulta o LRCLIB para as
   ausentes, sem mudar a música que toca.
3. Leia a síntese e toque em **Ver leitura por música** para abrir cada interpretação
   vinculada à sua faixa. O contador informa leituras disponíveis, não confiança.
   **Atualizar análise** reaproveita um resultado em cache se a entrada não
   mudou. Se o prazo terminar com leituras prontas, **Continuar análise** reutiliza
   essas leituras e processa as pendentes. **Remover modelo** libera os pesos.

Modelo: [Qwen3.5-2B](https://huggingface.co/Qwen/Qwen3.5-2B) com quantização [Unsloth Q4_K_M](https://huggingface.co/unsloth/Qwen3.5-2B-GGUF),
Q4_K_M, licença Apache 2.0. Runtime:
[llama.cpp](https://github.com/ggml-org/llama.cpp), MIT. Revisões, tamanho e
hashes estão em [local-stories-model.json](../resources/local-stories-model.json).
Não houve treinamento ou ajuste fino do Qwen neste projeto. O classificador
TonalMuse continua identificado separadamente, com seu corpus sintético.

São **1.280.835.840 bytes** de pesos, não incluídos no APK/instalador. CPU somente,
uma thread na geração e na preparação, contexto de 1.024 tokens, entrada até 768 tokens e
saída até 64 tokens por faixa mais 64 na síntese, até 585 no lote. Para caber nesse orçamento, usam-se trechos do começo,
meio e fim das letras. O resultado descreve esse lote, não todas as músicas
ouvidas. Letras não disponíveis e versões divergentes são omitidas, sem
inventar seu conteúdo. Pode haver erros, especialmente em ironia, metáforas,
idiomas e contradições. Não é diagnóstico de humor, personalidade ou saúde.

O Windows executa `TonalStories.exe` em processo separado com prioridade
reduzida. Android usa JNI C++ num serviço privado `:stories`, vinculado à tela,
com thread de CPU em prioridade reduzida (nice +2), executada só com a
música pausada. Cache e preparação continuam em prioridade BACKGROUND (+10). Fechar/cancelar a leitura impede
entregas atrasadas e encerra o processo de inferência. O modelo não fica
residente durante a reprodução normal. A inferência tem orçamento de 90 s e
supervisão de 105 s; falhas de memória/tempo retornam estado explícito. Nenhum
processamento, rede ou banco entra no callback PCM.

### Prioridade da reprodução

Preparação, recorte do texto, cache, hash e rede rodam em workers de baixa
prioridade, separados da interface. O Windows entrega apenas snapshots para
QML e usa um processo de inferência com prioridade ociosa; o Android usa uma
thread de segundo plano e um serviço privado separado. Capturas pendentes são
agrupadas e limitadas a 128 faixas. Os pesos só são carregados por solicitação.

A prioridade acompanha a intenção de reproduzir, inclusive durante buffering;
falta momentânea de frames não é interpretada como pausa.
Download do modelo e busca adicional de letras desta análise aguardam uma pausa
na reprodução. No Windows e no Android, a inferência aguarda pausa; retomar a música encerra somente o
processo de análise e mantém a solicitação pendente até outra pausa.
Pausar ou cancelar o leitor não envia comandos ao player. A solicitação pendente
pode ser cancelada enquanto aguarda recursos, sem iniciar música ou alterar fila.

Antes de abrir o modelo, exige-se memória disponível de pelo menos o tamanho dos
pesos mais 512 MiB (aproximadamente 1.734 MiB). Sob pressão durante a inferência
(menos de 256 MiB livres; também `lowMemory` no Android), o processo opcional é
encerrado, preservando o player. São guardas conservadoras, não reserva física
nem garantia de impacto zero em qualquer aparelho. A análise consome recursos;
prioridade baixa e limites dão preferência ao áudio. Com outros processos pesados,
pode aguardar ou atingir o prazo em vez de produzir um resultado.

O cache privado guarda no máximo 128 letras limitadas e 20 resumos. Os resumos
dependem da versão do modelo, período e trechos de entrada. A mudança de período
não apresenta o resumo do período anterior. A consulta ao LRCLIB é sequencial,
com intervalo de 1,25 s, resposta até 256 KiB e timeout de 6 s por operação de
rede; 429/503 impõem cooldown de pelo menos um minuto. A identificação exige
artista e título normalizados iguais e diferença de duração de até cinco
segundos quando conhecida. Não usa tokens Spotify e não envia as letras para
um serviço de inferência.

## Validação e limites

Evidências finais: [statistics-stories-0150-tests.json](statistics-stories-0150-tests.json).
O teste de inferência Windows usa descrições originais sintéticas em português;
não valida a interpretação de todo um repertório real. Testes em AVD são
identificados como emulador. Não houve novo benchmark comparativo com Spotify
nesta entrega; os números de 0.12.0 permanecem históricos, separados em
[BENCHMARKS.md](BENCHMARKS.md). Análise com LLM consome CPU/RAM durante a execução.


## Correção semântica 0.15.1 (em validação)

Na 0.15.0, a frase “O conjunto reúne letras que contrastam e contrastam.”
foi aceita indevidamente. O usuário autorizou priorizar qualidade com um modelo
opcional de cerca de 1,2 GiB. A 0.15.1 troca os pesos, analisa as faixas
separadamente e usa as leituras válidas na síntese. Formato, repetição, eco das
instruções e cópia de versos são filtrados; isso não prova correção semântica.

O cache v3 não é reaproveitado. O novo download é solicitado pelo usuário,
verificado por tamanho/SHA-256 e só então substitui os pesos antigos. Nenhum
favorito, playlist ou histórico é apagado nessa troca. Não desinstale o aplicativo
para atualizar. Os arquivos de modelo continuam fora do APK/EXE.

Um lote tem até oito letras selecionadas; faixas ainda pendentes são identificadas.
Cada ação usa até 90 s de inferência, com supervisão de 105 s. Pausa/retomada de
áudio e cancelamento prevalecem. Uma retomada da música encerra só o leitor; o
app não pausa nem inicia música para liberar recursos de análise.

A 0.15.0 e suas tentativas de AVD permanecem históricas no relatório de testes.
O novo leitor exige validação própria. Não chamar um CI de compilação de prova
de compreensão de letras reais, nem de benchmark do Spotify.
