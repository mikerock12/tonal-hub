# Volume Windows — 0.14.1

O volume do Tonalify agora muda durante o arraste. O percentual escolhido
aparece junto da barra; 0 silencia e 100 corresponde ao máximo do controle.
O botão de alto-falante silencia e restaura o último valor anterior ao mudo.
A roda ajusta 2 pontos percentuais por passo de 120 unidades; dois passos
ajustam 4 pontos. Deltas menores são acumulados; eventos horizontais e sem
movimento vertical não alteram o som.

## Causas e correção

Antes, o timer de 250 ms era reiniciado a cada pixel, adiando a alteração até
parar de mover. O valor pendente era liberado logo após enviar o PUT, antes
do ACK: um GET atrasado podia devolver o volume anterior à barra. PUTs podiam
se sobrepor e seus retries podiam aplicar um valor já superado. A roda sempre
saltava 5 pontos, inclusive para deltas verticais zero.

Agora, throttle de 150 ms agrupa entradas sem esperar encerrar o gesto. Há
um pedido em voo por rota/dispositivo e apenas o último valor pendente.
Um pedido superado não é repetido após 401/429/503. O cancelamento opt-in
notifica 499 local para liberar o slot, sem enviar esse código à rede.
A guarda não altera o contrato de cancelamento das outras chamadas da API.

Gerações invalidam o volume de GETs iniciados antes da intenção ou do ACK.
Somente o campo volume é descartado: metadados e transporte continuam sendo
atualizados. Uma guarda de 3 s após o ACK ignora valores remotos ainda antigos;
depois, alterações externas do Connect voltam a ser aceitas. Trocar dispositivo,
sair da sessão ou abrir arquivo local cancela a rota antiga; um ACK tardio dela
não pode liberar o slot de uma rota nova. Falha da intenção atual gera aviso e
libera reconciliação. Preferências são gravadas de forma agrupada.

Não há rede, disco ou alteração de arquitetura no callback PCM. O master é
aplicado uma vez: librespot para Spotify, OpenAL para arquivos locais.
O percentual é o controle do mixer, não uma escala linear de pressão sonora.
Rede, buffer e dispositivo ainda influenciam o tempo até a mudança ser ouvida.

## Build e validação

Testes de regressão usam o Player C++ real com transporte HTTP simulado e a
barra QML de produção. Cobrem arraste contínuo, último valor, polls atrasados,
limites, mudo, delta da roda, transferência e retry 429 superado, além da
recusa de comando. Resultados finais: `volume-windows-0141-tests.json`.
Sem benchmark novo ou alegação de validação acústica/Spotify autenticado.

A primeira tentativa de build foi interrompida pela ausência do Ninja local.
O usuário confirmou bloqueio do Malwarebytes e informou sua liberação. A
versão configurada 1.13.2 foi restaurada do mesmo pacote; nenhuma proteção foi
desativada pelo agente. A tentativa alternativa NMake foi cancelada.
Build final pelo preset release e scripts normais de packaging/.

Instalador: `out/release-local/Tonal-Hub-0.14.1.0-Windows-x64-Setup.exe`.
Feche os players e instale por cima. Dados continuam separados dos executáveis.
Android permanece 0.14.0; não houve nova compilação de APK nesta correção.
Créditos e MIT ao Tonal original de Matheus Nunes (natheus) preservados.


## Repetição da suíte completa — 24/10/2026

A nova execução passou os 10 CTest Windows, sem falhas, em 58,04 s. Inclui
recuperação de processos, pipeline/DSP/decoders e smoke de UI de áudio.
Não houve mudança de código ou de limites de teste entre as tentativas.
Ao conferir a AVD cuja parada o usuário autorizou, ela já não estava ativa;
outra AVD e os processos Java foram preservados. Isso não estabelece a causa
das falhas anteriores, mantidas no histórico do relatório JSON.
Os 10 cenários focados de volume, QML 20/20 e runtime do pacote já estavam
aprovados. A repetição não mede áudio físico nem usa Spotify autenticado.

Este complemento altera documentação/evidências. O instalador 0.14.1 mantém
SHA-256 `38e1af18bfef777bed37f7d8d169550301cf75baeab868c0b293680b43da3182`,
compilado a partir de `d4c812d9cda8b66a1f40ccecd0c46aaddb3f5192`.
Não houve rebuild de EXE/APK.
