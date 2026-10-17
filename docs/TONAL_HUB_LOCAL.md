# Tonal Hub 0.1.2 — teste local no Windows

Esta entrega cria três programas nativos:

- **Tonal Hub**: central para abrir e alternar entre os players. Não inicializa
  motor de áudio, catálogo, credenciais nem consultas periódicas em segundo plano.
- **Tonalify**: nome de apresentação do Tonal dedicado ao Spotify. Mantém o cofre,
  configurações, biblioteca e motor já existentes.
- **IndieTonal**: catálogo e reprodução de fontes independentes, em outro processo,
  com suas próprias configurações de áudio e conexões.

Os players abrem em janelas próprias. O hub pode continuar aberto atrás delas.
Reabrir um programa reutiliza sua instância. Os botões de alternância enviam pausa
ao outro player; a troca não inicia automaticamente uma faixa diferente.

## Abrir

Depois de compilar e empacotar:

```text
out/release-local/Tonal-Hub-0.1.2-local/TonalHub.exe
```

No pacote, `Tonalify.exe` é o player Spotify. Na pasta de build ele continua
chamado `Tonal.exe`, para preservar os scripts e caminhos de desenvolvimento.
Os três programas precisam permanecer junto das DLLs e pastas do pacote.
Na primeira execução, feche a versão antiga do Tonal para evitar dois motores
Spotify usando a mesma sessão. O login existente é preservado.

## Funcionalidades disponíveis

| Fonte | Funciona nesta implementação | Configuração / limite |
|---|---|---|
| Audius | Busca de faixas, perfis e álbuns/playlists; abertura de perfil/coleção; streaming público | Perfis e remixes não são automaticamente associados ao artista original; conteúdo gated não oferece play |
| MusicBrainz | Busca de artistas; grupos de lançamentos; edições e faixas da discografia | Somente metadados; consultas espaçadas; não oferece áudio |
| Live Music Archive | Busca na coleção etree; abertura de show; reprodução de MP3 público | Itens marcados como restritos são recusados; acesso ao site de origem disponível |
| Jamendo | Conector de busca, artista/álbum e reprodução das URLs disponibilizadas | Precisa de client ID próprio ativo; não há chave emprestada ou de exemplo embutida |
| OpenSubsonic | Busca de artistas/álbuns/músicas; abertura de coleções; stream original | Precisa de servidor HTTPS, usuário e credencial; autenticação clássica por token e salt |
| Bandcamp beta | Reaproveita o conector OpenSubsonic para a coleção do usuário | Configure `https://bandcamp.com/api/subsonic` e as credenciais Subsonic de Fan Settings; ainda requer teste autenticado |
| Arquivos locais | Abertura pelo seletor de arquivos e reprodução no motor existente | Sem login Spotify |

Jamendo e OpenSubsonic têm código e tela de configuração, mas **não foram
validados com uma conta real**, pois não há credenciais próprias configuradas
nesta entrega. A chave pública de exemplo do Jamendo estava suspensa na pesquisa.
Não foi criada conta externa nem contratado serviço.

O player oferece pausa, volume, seek quando aceito pelo stream, anterior/próxima
e sequência automática das faixas reproduzíveis carregadas na coleção/busca.
A fila é da sessão atual; não é uma biblioteca sincronizada entre serviços.

## Descoberta a partir do Tonalify

Na busca do Spotify, há uma seção **Explore também no IndieTonal**. Ela consulta
fontes independentes usando somente o texto digitado, mostra alguns resultados
com a fonte e permite abrir a mesma busca no outro programa. A caixa de busca do
IndieTonal acompanha a consulta recebida, inclusive com a janela já aberta.

Não há envio de tokens, IDs de catálogo ou histórico Spotify aos provedores.
MusicBrainz é consultado no IndieTonal, sem uma segunda consulta automática
simultânea da seção de sugestões do Tonalify.

Os resultados são **candidatos**, não afirmações de que a música está ausente do
Spotify. Não foi implementada uma auditoria completa de disponibilidade ou de
exclusividade entre catálogos. O código remove repetições do mesmo ID/provedor
entre páginas; não funde automaticamente gravações de fontes diferentes pelo
nome, nem esconde versões ao vivo, remixes, remasterizações ou homônimos.

Separar programas e nomes é uma decisão funcional. Não equivale a autorização
Spotify para sugestões ou integração entre serviços. A compatibilidade desse
uso com a política ainda precisa ser resolvida antes de distribuição pública.
[Política Spotify](https://developer.spotify.com/policy), especialmente III.5.

## Áudio e rede

O IndieTonal reutiliza o motor C++ FFmpeg → DSP → OpenAL existente. Oferece EQ de
10 bandas, presets, graves, compressor opcional, limitador de picos de amostra,
HRTF para fones e IAMF binaural para arquivos locais compatíveis. Tem configurações
próprias; não altera os presets do Spotify.

Streams HTTPS usam verificação TLS e o protocolo assíncrono FFmpeg com buffer
limitado em memória. A duração disponível no buffer depende do formato, bitrate
e rede; não é um download offline permanente nem um cache integral de shows.
Há timeout e reconexão limitada para erros de rede/HTTP 429 e 5xx; cancelamento
ao trocar de fonte ou fechar o player. Após falha definitiva, **Tentar novamente**
resolve a oferta outra vez. Isso pode reiniciar a faixa; não há garantia de
retomada da posição após toda falha de provedor.

Arquivos locais mantêm ReplayGain/análise de loudness. Streams não são analisados
por inteiro antes do início: usam ReplayGain quando fornecido pelo decoder;
na ausência desses dados não se promete normalização medida em LUFS. O limitador
atua em picos de amostra, sem garantia de true peak após codificação/Bluetooth.

Não se garante uniformidade de disponibilidade, formato, qualidade ou seek entre
provedores. Live Music Archive usa MP3 público nesta versão; não foi acrescentado
um seletor de FLAC. Falhas dos serviços aparecem identificadas e não bloqueiam
resultados já obtidos das demais fontes.

## Conexões e dados

Em **Conexões**, configure o client ID Jamendo e, se desejar, a biblioteca
OpenSubsonic. Segredos ficam no cofre Windows em `Tonal/IndieTonal/OpenSubsonic`,
separados de `Tonal/Spotify`. Client ID público Jamendo fica nas preferências
IndieTonal. Uma mudança de servidor/usuário exige informar a credencial de novo;
a senha anterior não é reutilizada silenciosamente em outro endereço.

Só URLs HTTPS são aceitas. Não existe opção para desativar validação de certificado.
Remover a conexão apaga sua credencial do cofre.
As conexões são relidas antes de novas buscas e resoluções, para que alterações
sejam reconhecidas entre processos. Resultados de outro servidor/usuário não são
reutilizados como se pertencessem à biblioteca atual.
O pacote contém uma lista explícita
de executáveis/DLLs/recursos/licenças; não inclui os dados de contas do computador.

## Verificação

### Correção local 0.1.2: recuperação do Tonalify

Inclui reconhecimento de sessão com `RUST_LOG=warn`, reinício assíncrono,
recuperação de crash/EOF/fluxo PCM parado, preservação e cancelamento da retomada,
deadline de token/API e diagnóstico persistente limitado. Consulte
`docs/TONALIFY_RECOVERY_LOCAL.md` e o relatório consolidado para evidências e
limites. As correções de busca da 0.1.1 continuam incluídas.

### Correção local 0.1.1: busca e indisponibilidade

- Descoberta ordenada por correspondência de título/artista, ignorando caixa,
  acentos e pontuação. A identidade exata no MusicBrainz ganha prioridade sobre
  perfis de nome igual e resultados parciais. Isso não autentica uploads de outras
  plataformas. A ordem de faixas dentro de álbuns permanece a da fonte.
  Entre artistas homônimos, a pontuação da busca do MusicBrainz é preservada:
  por exemplo, o Nirvana americano fica antes dos grupos de mesmo nome.
- Filtros com contagem: artistas, álbuns/playlists, músicas e shows ao vivo.
  A interface informa quando a consulta de shows falha ou não encontra itens.
- MusicBrainz: até duas novas tentativas automáticas para erros transitórios,
  espera progressiva de 2/5 segundos, fila de pelo menos 1,1 segundo e respeito a
  `Retry-After`. Esperas superiores a 30 segundos ficam para nova tentativa manual.
  Navegar para outra consulta cancela as tentativas anteriores; a restrição de
  espera do servidor continua válida. Sem consultas periódicas em repouso.
- Cache apenas em memória de JSON público do MusicBrainz, limitado a 16 MiB:
  válido por 15 minutos, com recuperação sinalizada de dados de até 24 horas
  quando há indisponibilidade transitória. Fechar o app descarta o cache.
  Nenhuma credencial ou resposta de biblioteca privada entra nesse cache.
- Botão **Tentar novamente** para consultas que falharam. Repetir recupera as
  requisições da página, sem duplicar resultados nem voltar ao início da busca.
  Páginas de edição/faixas do MusicBrainz não oferecem paginação inexistente.
- Testes determinísticos cobrem 503→200, limite de tentativas, `Retry-After`
  numérico/data, cancelamento, cache recente/antigo, erro 400, relevância, acentos
  e manutenção da ordem de faixas.

Feche o IndieTonal anterior antes de abrir a versão 0.1.1: a instância única
encaminha novas aberturas para o processo já em execução.

O Live Music Archive é a coleção `etree`, não todo o Internet Archive. Resultados
com referência a uma banda no título podem ser tributos/menções; não equivalem a
shows da banda original. A sondagem das bandas solicitadas está em
`out/research/live-bands-probe.json`, com identificação do artista nos metadados.

Testes de referência desta implementação:

- Catálogo: deduplicação por ID, preservação de versões, bloqueio de faixas gated,
  distinção de metadados/áudio, erro de credencial e autenticação Subsonic.
- Abertura QML do hub e IndieTonal, renderização de telas e painéis.
- Reutilização de instância, encaminhamento de consulta e pausa por IPC: teste
  isolado com WAV sintético, sem contas e sem chamadas de API.
- Reprodução real Audius e de show público Live Music Archive pelo DSP nativo;
  trechos medidos de aproximadamente dois segundos, sem underruns ou amostras
  inválidas nesses trechos. MusicBrainz carregou 20 grupos de lançamentos.
- Regressões anteriores: integração do motor de áudio e painel original do PC.

Na versão 0.1.0, houve rodadas conjuntas que falharam por **HTTP 503 do MusicBrainz** e por **5xx
no áudio do Internet Archive**, apesar de os testes individuais terem passado.
Esses resultados são preservados; não se afirma que uma rodada conjunta ficou
inteiramente estável ou que os serviços estarão sempre disponíveis.
A versão 0.1.1 acrescenta a recuperação descrita acima; consulte também
`docs/INDIE_SEARCH_FIX_LOCAL.md` para os testes de bandas, tributos e cobertura.

Relatórios e imagens ficam em `out/research/`. O resultado consolidado acompanha
este guia em `docs/tonal-hub-tests.json`, quando gerado no fechamento da entrega.
Os tempos DSP desses testes funcionais não são benchmark de CPU/RAM, especialmente
quando há compilação simultânea. Não houve comparação nova com Spotify oficial.

Faltam ensaio longo de streaming, rede instável controlada, áudio audível/Bluetooth
físico, autenticação Jamendo/OpenSubsonic/Bandcamp e controles de mídia do Windows
para o IndieTonal. Os controles de mídia existentes do Tonalify foram preservados.

## Compilar e repetir

```powershell
./packaging/build-local.ps1 -Test
./out/tools/cmake331/cmake/data/bin/ctest.exe --test-dir out/build/release -R '^(indie_|hub_)' --output-on-failure
./out/tools/python/Scripts/python.exe tests/indie/ipc_smoke.py
# Testes de rede opcionais, usando somente fontes públicas:
./out/build/release/TonalIndieTests.exe --live --output out/research/indie-live-tests.json
./out/build/release/TonalIndieTests.exe --live --provider 'Live Music Archive' --output out/research/indie-lma-tests.json
./packaging/package-hub-local.ps1 -Zip
```

O script prefere CMake 3.31 instalado no projeto e desativa módulos C++ do OpenAL;
a varredura QML não percorre o enorme cache de dependências da raiz.

Esta entrega é **Windows x64**. Os APKs Android 0.3.0 anteriores continuam com o
comportamento anterior; o hub ainda não foi portado para Android. Não houve commit,
push, PR, release ou escrita no GitHub.
