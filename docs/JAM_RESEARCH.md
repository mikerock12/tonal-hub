# Jam com amigos — investigação em 25/10/2026

Solicitação atualizada do usuário: amigos usando o Spotify nativo participam e
adicionam faixas. Se não houver aprovação de entrada disponível, mostrar apenas
quem entrou. Mostrar também músicas adicionadas/alteradas e autoria quando
disponível. Aprovação prévia deixou de ser requisito nessa condição. Alterações geram
avisos leves no canto inferior esquerdo, subindo e desaparecendo em fade.
Preservar áudio, fila e Descoberta 2+1; nenhum convite é enviado automaticamente.

Estado: **experimental no build local 0.15.1**, sem teste
com participantes reais. O código de interface não comprova interoperabilidade.

Em 25/10, o transporte nativo Windows autenticou com o cache existente e fez
somente `GET sessions/current`: retorno 404 em 1,71 s; a repetição com o
transporte final e fallback HTTPS retornou 404 em 3,32 s, esperado também quando não
há sessão ativa. Nenhum dispositivo Connect foi anunciado por esse auxiliar,
nenhum Jam foi criado e não houve comando de áudio. Esse resultado não comprova
criação, participação de amigos ou alterações de fila numa sessão real.

## Evidências

- API pública: [Get Queue](https://developer.spotify.com/documentation/web-api/reference/get-queue)
  fornece faixas, mas não atribui a alteração a uma pessoa nem oferece controles
  de participantes do Jam na referência pública pesquisada.
- [Suporte Spotify](https://support.spotify.com/us/article/jam/) descreve o Jam
  nativo. Não confundir aprovação de Request to Jam em Messages com aprovação
  obrigatória de toda pessoa que abriu um link de convite.
- O librespot 0.8 embarcado recebe `social-connect/v2/session_update`; sua
  implementação só reconcilia session_id. O proto social_connect_v2 contém
  lista de membros e eventos USER_JOINED/USER_LEFT, mas não pedidos pendentes.
- [librespot #1576](https://github.com/librespot-org/librespot/issues/1576)
  registra suporte a Jam como recurso ainda solicitado nessa base.
- [go-librespot](https://github.com/devgianlu/go-librespot) declara suporte a
  reprodução em Jam. A pesquisa de código encontrou mudanças de fila entregues
  como transferências que retêm sessão/faixa e modos `jam:on/off`. GPL-3.0:
  não copiar sua implementação Go para o código MIT; o projeto serve aqui como
  evidência de comportamento do protocolo, não como nova dependência embarcada.
- Fontes adicionais de interoperabilidade: Sonora `connect.rs` revisão
  `8bac90bbd41de8dc9c7f93c9ff3c99818932f09b`, Nexfy `social.rs` revisão
  `d283b7c7c2df02cac572704ae0a1a6cd6ed1a657`, spotisloth `docs/JAM.md`
  revisão `9620115f9057cec840e37bc9358a49117f774deb`. Lidas pelo conector GitHub.
  A existência de código nesses projetos não comprova funcionamento no Tonal.

## Protocolo experimental a verificar

Leitura de sessão por `GET /social-connect/v2/sessions/current`; criação
`current_or_new?activate=true&local_device_id=…&type=REMOTE`; entrada
`sessions/join/{token}`; saída `sessions/leave`; encerramento e remoção de
membro em `social-connect/v3/sessions/{id}` e `/member/{member}/kick`.
Esses caminhos não são uma API pública estável. Verificar autenticação existente,
status e formato antes de ativar uma operação no produto. A investigação ainda
não encontrou uma operação de aprovação prévia de participante.

Não apresentar “aceitar entrada” como autorização anterior se o evento recebido
for apenas USER_JOINED, depois que a pessoa já entrou. Não inferir autor da fila
apenas comparando Get Queue. Não registrar tokens/URLs privadas de entrada,
credenciais ou IDs pessoais em logs/versionamento/capturas públicas.

## Próximas verificações

1. Priorizar criar/entrar/encerrar Jam e avisos de entrada. Aprovação não bloqueia
   a entrega, conforme esclarecimento do usuário. Não criar sala de espera/relay
   próprio sem nova necessidade; o protocolo Spotify preserva clientes nativos.
2. Conferir se o login atual dá acesso às consultas de sessão, sem trocar a
   reprodução nem criar Jam durante diagnóstico read-only.
3. Tratar transferências de Jam preservando a faixa audível antes de habilitar
   a colaboração; dados de fila não pertencem ao callback PCM.
4. Implementar UI e avisos com fila limitada, agrupamento de rajadas, timeout,
   cancelamento no ciclo de vida e animação apenas durante a exibição.
5. Distinguir testes sintéticos de uma sessão real entre contas/Spotify nativo.

## Implementação em andamento

- `native/jam-transport`: código Rust de transporte nativo reutilizado pelo JNI;
  auxiliar Windows sem Player/Spirc, usando credenciais existentes somente em
  memória e sem escrevê-las. Lista fixa de operações, comandos de 8 KiB, respostas
  de até 512 KiB durante a recepção e timeout total de 10 s. O endpoint privado usa o cliente
  HTTPS existente; não é um servidor ou serviço instalado no sistema.
- Interface C++/QML no PC e Java no Android, com início, convite por cópia,
  entrada, saída e lista de participantes. Nomes e convites ficam em memória.
- Primeira consulta estabelece uma base; não anuncia todos como recém-chegados.
  Diferenças posteriores identificam entradas por ID. Avisos de fila não deduzem
  autoria por presença de um único participante. Progresso normal da música
  estabelece uma nova base, evitando descrevê-lo como edição de um convidado.
- Avisos com fila limitada, subida/fade e tempo finito. Android usa janela do
  próprio app, sem permissão de sobrepor outros aplicativos. Consultas aguardam
  retorno ao primeiro plano. Não há aprovação prévia simulada.
- O motor Android recebe pedidos HTTP em tarefa Tokio separada; o await não
  bloqueia pausa/stop. Transferências de Jam que retêm a mesma faixa preservam
  o decoder e a pausa. Compilação nativa não prova participação real de amigos.
- O CLI Windows precisa ser reconstruído com os mesmos patches antes do pacote
  final. `packaging/build-jam-local.ps1` prepara fontes upstream verificadas do
  cache Cargo e compila localmente. Não copiar a implementação GPL do go-librespot.

O serviço informa uma janela parcial da fila. O app alinha ocorrências dessa
janela com a seleção preparada, conserva os marcadores das descobertas aceitas
e a cauda conhecida ainda não observada. Uma seleção sem sobreposição substitui
a anterior. Atualizar esse cache não envia play/seek. Edições locais Windows
pendentes durante uma pausa não são sobrescritas por polls anteriores.
Se a combinação ultrapassar 500 entradas, bloqueia reaplicar os modos até haver
espaço; não trunca silenciosamente a fila. A autoria de uma música não é inferida.

Testes de interface verificaram abrir o painel e limitar os avisos a oito itens,
com texto simples. Ainda falta prova de participação entre contas com Spotify
nativo. O recurso continua identificado como **experimental**, não Jam estável.
