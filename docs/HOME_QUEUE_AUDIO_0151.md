# Início, fila, navegação e áudio — trabalho 0.15.1

Estado em 25/10/2026: alterações em validação, sem novos instaláveis publicados.
Esta página não afirma que o leitor local ou o Jam já foram entregues.

## Página inicial

O início combina álbuns com escutas locais efetivas e coleções retornadas pelo
Spotify. Abrir uma coleção não conta como escuta. O agrupamento usa o ID do
álbum, soma o tempo ouvido e conserva a data da última sessão. O SQLite histórico
é consultado num worker; o cache recente de sessões não é o limite do histórico.
A agregação diária preserva tanto a primeira como a última data para não
substituir a recência pela primeira escuta do dia.

As prateleiras mostram recentes, mais ouvidos, álbuns da conta e álbuns dos
artistas mais ouvidos para explorar. Sugestão significa álbum ainda não observado
no conjunto local consultado, não lançamento novo ou exclusividade. São até três
consultas de discografia de cinco resultados; sem inferência ou busca de letras.
Até 60 recentes e 20 campeões históricos compõem a coleção local da tela.

## Página da música e letra

No Android foram retirados Agora/Biblioteca/Letras e o X da barra adicional.
O cabeçalho global conserva os atalhos e ganha uma casa para voltar ao início.
O botão Voltar do Android fecha a apresentação e revela a rota de catálogo
preservada; outro Voltar navega para a rota anterior. O áudio não é reiniciado.

Atualizar letra fica também nos controles de tela cheia nas duas plataformas.
Uma atualização explícita ignora o cache da letra corrente e permite retomar a
tradução automática. A tela principal de letras Windows atualiza o seu próprio
provider, em vez de buscar numa segunda coleção invisível. Ações em andamento
continuam serializadas. O primeiro toque na tela cheia só revela os controles.

## Inserção na fila

O menu de três pontos oferece colocar a música como próxima ou última.
O limite é 500 entradas: ao atingi-lo, recusa a inserção, sem descartar originais.
Ocorrências repetidas e marcadores de Descoberta 2+1 são preservados.

Android usa uma edição que mantém o decoder e a intenção de pausa. Uma edição
que chegar depois de outra faixa ser selecionada é descartada antes de alterar o
contexto nativo. Repetição é reaplicada, como nas demais cargas de contexto.

Windows mantém a edição preparada enquanto está pausado; aplica a ordem ao
retomar explicitamente pelo Tonalify. Não envia play para executar uma edição
durante a pausa. Enquanto toca, utiliza a reaplicação de fila já existente, com
posição e compensação de latência. A fila informada pelo serviço pode ser parcial;
as filas ordenadas conhecidas pelo app têm prioridade. Não é uma operação de
edição da playlist Spotify salva.

## Áudio

Mover uma banda passa a ativar o equalizador. O bypass mestre continua
prevalecendo; o painel Android mostra quais etapas estão habilitadas no perfil
da saída atual. Configurações Spotify que dependem da inicialização mantêm a
indicação de reconectar o motor.

HRTF para fones continua respeitando o perfil de saída. Foi acrescentada uma
escolha explícita **HRTF em caixas / carro — experimental**, que aplica o mesmo
binaural OpenAL sem o bloqueio do perfil. Não inclui cancelamento de crosstalk,
medição do ambiente, head tracking ou controle de codec Bluetooth. A reprodução
por caixas mistura os sinais dos dois ouvidos; posição e ambiente afetam o
resultado. [Documentação primária do OpenAL Soft](https://github.com/kcat/openal-soft/blob/master/docs/hrtf.txt).

Testes de resposta elétrica/digital não equivalem a audição ou medição no rádio
do carro. O relato físico do usuário ainda não foi reproduzido nesse receptor.

## Validação local

CTest Windows: **10/10**, 64,53 s. CTest complementar: **7/7**, 25,76 s.
QML: **23/23** de fila/início/Jam e **6/6** de letras. Java: três variantes
compiladas. Política de transporte: 43 verificações. JNI/Android: 35 cenários
instrumentados, 13 verificações de navegação e tela cheia, suíte digital de
áudio aprovada com as dez bandas de EQ e os perfis HRTF/bypass. Compilados
ARM64 e x86_64; assinaturas dos dois APKs verificadas.

Os testes Android usam a AVD exclusiva, sem conta Spotify ou telefone físico.
A suíte de áudio mede sinais digitais sintéticos, não o rádio do carro. O Jam
continua experimental, sem prova de participação entre contas. A leitura local
ainda tem cobertura limitada; não confundir CI com interpretação semântica.
Tentativas iniciais, limites e resultados em [evidências](release-0151-tests.json).
