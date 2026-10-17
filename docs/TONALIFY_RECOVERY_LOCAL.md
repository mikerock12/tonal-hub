# Tonalify — recuperação de reprodução (pacote local 0.1.2)

Correção local em 16/10/2026. Sem commit, push ou PR.

## Evidência e limites do diagnóstico

Não havia processo Tonalify/librespot ativo, dump local ou evento de falha do
Windows correspondente disponível durante a inspeção. A versão anterior mantinha
apenas 300 linhas do motor em memória: a causa exata do episódio relatado não
pôde ser reconstruída retrospectivamente.

Foi encontrada uma configuração concreta que impedia a recuperação:
`RUST_LOG=warn` no ambiente. O librespot 0.8 publica `Authenticated as` em nível
INFO (`librespot-core-0.8.0/src/session.rs`), e o Tonalify dependia desse evento
para reconhecer Ready e reencontrar seu dispositivo. O teste real anterior à
correção não chegou ao estado de motor/dispositivo pronto. A configuração de
log agora preserva os eventos de sessão/conexão necessários por processo filho,
sem alterar a variável de ambiente do usuário.

Também foram corrigidos caminhos verificáveis de recuperação:

- Saída inesperada do processo preserva a faixa/fila/posição antes de invalidar
  o dispositivo e limpa o canal PCM antigo.
- EOF do canal PCM é sinalizado, inclusive quando o processo permanece vivo.
- Ausência de progresso PCM por 30 segundos enquanto se espera reprodução
  aciona recuperação; pausa intencional, arquivo local e outro dispositivo não
  ativam essa detecção. Não são feitas consultas extras à API nesse monitor.
- Reinícios de recuperação não usam espera bloqueante na interface. Há até três
  reinícios automáticos em cinco minutos; autenticar novamente não zera esse
  limite. Reinício explícito permite nova tentativa após falhas persistentes.
- Retomada só descarta a intenção após HTTP 2xx. Erro de rede/404/5xx preserva
  a seleção e permite novas tentativas limitadas. HTTP 0 não equivale a sucesso.
- Escolher outra faixa durante reconexão substitui a intenção antiga. Pausar,
  alternar pelo hub ou abrir áudio local cancela retomadas e seus retries de API.
- API tem prazo total por tentativa de 30 segundos; renovação de token, 20
  segundos. Isso complementa os timeouts de inatividade e libera os comandos
  que estavam aguardando uma resposta que não termina.

## Diagnósticos

O programa passa a manter `diagnostics/playback.log` em sua pasta local de dados
(`%LOCALAPPDATA%/Tonal/Tonal`). Há uma cópia anterior `.1`; cada arquivo fica em
torno de 256 KiB. URLs, Bearer, access/refresh tokens e senhas são removidos das
mensagens gravadas. O pacote não contém esses logs nem credenciais de execução.
As configurações de efeitos, volume e credenciais existentes são preservadas.

## Validação

`TonalSpotifyRecoveryTests --local-only` usa rede simulada e um processo filho
local que produz PCM pelo mesmo canal nativo da aplicação. Não acessa contas:

- crash do filho; EOF com filho vivo; fluxo que para sem EOF; panic simulado;
- pausa sem reinício; limite de crash-loop e reinício explícito posterior;
- heartbeat da interface enquanto recupera e novo PCM após o reinício;
- fila/posição preservadas, nova escolha durante reconexão e cancelamento de
  comandos antigos, inclusive retries já agendados na camada HTTP;
- renovação de token e requisição API que nunca concluem, seguidas de recuperação;
- ambiente de log global `warn`, redaction e gravação do diagnóstico.

O teste opcional `--live-session` usa a sessão existente somente se não houver
reprodução ativa. Trabalha em um dispositivo de diagnóstico com saída OpenAL
nula e cache temporário removido ao encerrar. O resultado real e eventuais
falhas/limitações ficam em `out/research/spotify-recovery-live.json`; o primeiro
resultado anterior ao ajuste de Ready foi preservado separadamente. Não se
afirma que esse teste passou sem consultar o relatório.

Nesta rodada, as seis verificações CTest passaram. A verificação real
`--ready-only` confirmou motor autenticado e dispositivo descoberto no Spotify
Connect, com **zero comandos de reprodução**. O teste completo de reprodução e
queda do motor com conta real foi pulado porque havia música ativa. Não houve
interrupção dessa reprodução. As quedas e a retomada foram validadas com o
produtor PCM local nos testes controlados, não como um ensaio longo na conta.

Testes e evidências:

- `out/research/spotify-recovery-tests.json` e `.xml`
- `out/research/spotify-recovery-live-before.json`
- `out/research/spotify-recovery-live.json`
- `out/research/spotify-readiness-live.json`
- `out/research/hub-ctest.xml`
- `docs/tonal-hub-tests.json` (pacote e regressões consolidados)

Não é avaliação auditiva, de Bluetooth físico ou de reprodução prolongada. Uma
queda externa de rede/servidor ainda pode causar interrupção; a correção limita
travamentos persistentes e preserva a intenção de reprodução.

## Executar

Feche a versão anterior e abra
`out/release-local/Tonal-Hub-0.1.2-local/Tonalify.exe`.
Mantenha o executável junto das DLLs e demais pastas do pacote.
