# Leitor local 0.15.2 — retomada e CPU Android

## Falha relatada

Em 26/10/2026, o usuário informou que o POCO X7 Pro chegava a
“O limite de tempo foi atingido ao preparar a conferência”, sem leitura alguma.
O modelo de 1.221,50 MiB já estava presente. O print confirma a falha de execução;
não fornece uma medição de CPU, temperatura ou memória do aparelho.

Na 0.15.1, dois contextos eram preparados antes da primeira leitura. Sem uma
interpretação aceita, o resultado era descartado, inclusive a preparação já
executada. Repetir podia executar o mesmo trabalho até o mesmo timeout.

## Correção

O leitor v5 prepara a leitura e, só depois da proposta, a conferência. Cada ação
lê ou retoma uma faixa do lote de até oito. Uma ação seguinte reutiliza as
leituras anteriores e o estado da faixa pendente. O botão **Continuar análise**
aparece mesmo antes da primeira interpretação aceita. Progresso de preparação
não é apresentado como quantidade de músicas interpretadas.

Um checkpoint privado guarda contexto, posição da preparação, rascunho e a
decisão necessária para retomar geração/conferência. O estado do runtime por si
só não preserva logits: isso foi encontrado num teste de interrupção e corrigido
antes da entrega. Rascunhos sem conferência não viram conclusão na interface.

O arquivo é vinculado ao modelo, prompt, período, textos e nomes exatos. Formato,
tamanhos e checksum são conferidos; cache incompatível/corrompido é descartado.
Há um checkpoint por leitor, até 64 MiB, mais o temporário de escrita atômica.
O teste de desenvolvimento observou aproximadamente 20–24 MiB por estado;
cópias transitórias podem consumir mais memória que o arquivo isolado.
Checkpoint e pesos ficam fora do backup da biblioteca.

O orçamento de inferência continua em 90 s e a supervisão em 105 s. Uma thread
CPU; processo separado; somente durante pausa explicitamente escolhida pelo
usuário. Retomar música encerra o leitor, sem esperar a gravação e sem tocar a
fila. A próxima tentativa retoma o último estado válido já salvo. Fechar cancela
pedidos pendentes. Não há promessa de impacto físico absolutamente zero.

## ARM64 otimizado, com alternativa genérica

O POCO X7 Pro usa Dimensity 8400-Ultra, segundo as
[especificações oficiais](https://www.po.co/global/product/poco-x7-pro/specs/).
A compilação anterior usava o backend ARM genérico. A 0.15.2 inclui também uma
biblioteca com DotProd/I8MM/FP16. Ela só é carregada após consulta HWCAP do kernel
confirmar todas as instruções exigidas, incluindo atomics; não se decide pela
marca/nome do telefone. Ausência da biblioteca ou dos recursos mantém o backend
genérico. A inspeção do ELF confirmou instruções SMMLA e ligação interna da
variante; isso não mede ganho de velocidade no telefone físico.

Os pesos Qwen3.5 2B são os mesmos, com o mesmo SHA-256. Atualizar o APK sem
desinstalar permite reutilizar o modelo já verificado e os dados existentes.
Não houve novo treinamento nem inclusão dos pesos dentro do APK/EXE.

## Conteúdo e limites

O texto é uma interpretação proposta, sem diagnóstico do ouvinte. O revisor é o
próprio modelo e seus escores não são calibrados. A passagem por esse filtro
não comprova correção semântica. Num lote de quatro letras identificadas, o
fluxo terminou quatro propostas por ações de aproximadamente 43–51 s no desktop.
A leitura manual ainda notou nuances discutíveis; não é uma aprovação de
qualidade para todo o repertório ou medição no POCO.

Uma primeira versão curta do prompt foi rejeitada na avaliação: completava o
fluxo, mas não entregou leituras aprovadas pelo próprio revisor. Testes de
retomada e testes de conteúdo são documentados separadamente. Textos completos,
rascunhos privados e checkpoints não são publicados no repositório.

## Validação

Resultados em [release-0152-tests.json](release-0152-tests.json): Windows CTest
10/10, portáteis 7/7, guardas do leitor 1/1, QML 24 + 6 casos. A instrumentação
Android passou os dez grupos de histórico/relatório e a retomada com os pesos
reais: a preparação avançou de 16 para 48 tokens em processos separados,
inclusive com cobertura zero. Também conferiu espera de pausa e cancelamento.
O limite de 20 s usado para forçar interrupção é exclusivo do build de teste;
a versão de uso mantém 90 s. A AVD x86 usa o backend genérico, sem aceleração ARM.

O teste desktop interrompido em 17 ações confirmou 13 retomadas e remoção do
checkpoint ao concluir; era um prompt preliminar, sem leituras aceitas. Foi
preservado como evidência de execução, separado da avaliação de conteúdo final.
Uma tentativa inicial revelou falha ao retomar logits, corrigida e repetida.
Um teste Java inicialmente não compilou por uso de API de arquivo incompatível;
a leitura foi ajustada e a compilação repetida passou. Não foram afrouxados
limites de tempo/áudio para aprovar testes.

A execução em AVD não equivale ao teste no POCO X7 Pro; a validação física
continua dependendo de uma nova tentativa do usuário com o APK atualizado.
Não houve benchmark novo contra Spotify nem instalação/download de ferramentas
adicionais no PC nesta correção; as dependências e os pesos locais já existiam.
