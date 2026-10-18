# Tonal Hub 0.8 — desempenho e funcionalidades

Medição local de 17/10/2026. Projeto privado de Maicon Nunes, derivado de [tonal-player, de Matheus Nunes (natheus)](https://github.com/natheus/tonal-player), com crédito e licença MIT preservados.

## Resultado que os dados sustentam

No Windows, nesta sessão com streaming real da mesma faixa, o Tonalify usou **122.55 MiB de RAM privada**, contra **540.53 MiB** do Spotify: **77.33% menos**. A CPU média foi **0.50% contra 2.87% de um núcleo** (82.63% menor nesta amostra). A comparação inclui subprocessos e tem duração curta; não é promessa para todas as bibliotecas, máquinas ou efeitos.

A CPU deve ser lida primeiro em valores absolutos. Com médias próximas de zero, diferenças relativas grandes podem corresponder a poucos milissegundos e ruído de amostragem. Estas rodadas não estabelecem uma vantagem universal de CPU; a evidência mais clara é o uso de memória neste cenário.

No Android emulado, a tela inicial **sem login em ambos** ficou em **16.99 contra 154.78 MiB PSS**. Isso mede a interface inicial, não a eficiência relativa de streaming. O Tonalify autenticado, medido separadamente, ficou em **72.59 MiB PSS** e **5.23% de um núcleo**. **Não foi concluída uma amostra equivalente de streaming autenticado no Spotify Android. Não há conclusão de vantagem de reprodução no Android.**

## Hardware, versões e cenário

- PC: Windows 10 Pro, Intel Core i5-6500 3,2 GHz, 4 núcleos/4 threads, 16 GB instalados. Intel HD Graphics 530, driver 26.20.100.7262. Lexar NQ100 240 GB e WDC 1 TB; inventário exato no JSON.
- Tonalify Windows: build Release MSVC 2022/Qt 6.8.3, pacote 0.8.0. Spotify oficial Store: 1.3.5.3058, pacote 1.305.3058.0.
- Emulador dedicado: Android 16/API 36, x86_64, 2 vCPUs, páginas 4 KiB. Spotify 9.1.90.2270 com ABI x86_64; sem tradução de código ARM. Não é uma medição no POCO X7 Pro.
- Android comparável: variante Release `comparison`, sem depuração, perfil independente e sem login, contra a tela inicial sem login do Spotify. Teste de streaming Tonalify separado: variante `benchmark` com depuração, mesmas bibliotecas nativas Release.
- Faixa Windows: **Get Back — Veruca Salt**, identificada pela sessão real, URI registrada no coletor privado. Início solicitado em 30 s; há aquecimento e consultas antes da coleta, portanto os trechos medidos não são exatamente os mesmos milissegundos.
- Configuração Windows Tonalify existente: efeitos ligados, preset Rock, EQ ativo, graves +8 dB, compressor desligado, espacial original, normalização ligada, alvo local −16 LUFS e teto −1 dBFS. Configurações de qualidade/normalização do cliente oficial não foram igualadas/confirmadas. Não é um teste de fidelidade sonora ou volume.
- Sem compilação, compressão ou emulador durante a coleta Windows. A posição/visibilidade das janelas não foi imposta pelo coletor; os rótulos brutos com “background” não comprovam minimização equivalente. Essa limitação impede atribuir toda a diferença apenas ao motor de áudio.

## CPU e memória

CPU: **100% = um núcleo totalmente ocupado**. Divida os valores por 4 para a capacidade total deste PC e por 2 para a do emulador. RAM Windows é **USS**, páginas residentes exclusivas. Android é **PSS**, com páginas compartilhadas rateadas. Não compare diretamente os números absolutos entre sistemas.

| Plataforma | Cenário | Aplicativo | RAM mediana MiB | CPU média / mediana % de 1 núcleo | Amostras |
|---|---|---|---:|---:|---:|
| Windows | Reproduzindo | Tonalify | 122.55 (USS) | 0.50 / 0.00 | 60 |
| Windows | Pausado | Tonalify | 122.65 (USS) | 0.15 / 0.00 | 60 |
| Windows | Reproduzindo | Spotify | 540.53 (USS) | 2.87 / 0.00 | 60 |
| Windows | Pausado | Spotify | 534.82 (USS) | 0.43 / 0.00 | 60 |
| Android emulado | Tela inicial, sem login | Tonalify | 16.99 (PSS) | 0.06 / 0.00 | 12 |
| Android emulado | Tela inicial, sem login | Spotify | 154.78 (PSS) | 1.00 / 0.00 | 12 |
| Android emulado | Segundo plano, sem login | Tonalify | 17.02 (PSS) | 0.00 / 0.00 | 3 |
| Android emulado | Segundo plano, sem login | Spotify | 147.46 (PSS) | 0.00 / 0.00 | 3 |
| Android emulado | Tela inicial + Hub, sem login | Tonalify | 30.37 (PSS) | 0.13 / 0.00 | 6 |
| Android emulado | Streaming Tonalify, autenticado | Tonalify | 72.59 (PSS) | 5.23 / 5.23 | 6 |

### Rodadas individuais no Windows

| Rodada | Aplicativo | Estado | RAM privada mediana MiB | CPU média % de 1 núcleo |
|---|---|---|---:|---:|
| 1 | tonalify | Reproduzindo | 122.17 | 0.70 |
| 1 | tonalify | Pausado | 122.24 | 0.05 |
| 1 | spotify | Reproduzindo | 538.88 | 0.45 |
| 1 | spotify | Pausado | 541.26 | 0.42 |
| 2 | spotify | Reproduzindo | 542.96 | 5.29 |
| 2 | spotify | Pausado | 528.26 | 0.44 |
| 2 | tonalify | Reproduzindo | 122.96 | 0.30 |
| 2 | tonalify | Pausado | 123.00 | 0.25 |

Variação entre rodadas também faz parte do resultado. As médias agregadas não ocultam picos, atualização do catálogo ou trabalho de interface. Consulte os intervalos por amostra no CSV antes de atribuir uma diferença pequena ao decoder.

### Memória adicional e I/O no Windows

| Aplicativo | Cenário | RAM privada mín.–máx. MiB | Commit privado mediano MiB | Soma RSS mediana MiB | Processos | Bytes lidos / escritos |
|---|---|---:|---:|---:|---:|---:|
| Tonalify | Reproduzindo | 122.09–123.02 | 160.28 | 211.79 | 3–3 | 25228464 / 22530560 |
| Tonalify | Pausado | 122.24–123.00 | 160.36 | 211.88 | 3–3 | 0 / 0 |
| Spotify | Reproduzindo | 538.82–619.36 | 679.09 | 980.74 | 8–8 | 11773183 / 5170507 |
| Spotify | Pausado | 527.89–542.23 | 674.06 | 975.93 | 8–8 | 350483 / 116769 |

RSS somado conta páginas compartilhadas mais de uma vez. Commit não é RAM residente. I/O é lógico por processo, incluindo cache, arquivos e outras operações; não é medição isolada da internet nem do disco físico. Intervalos e dados completos estão em `samples.csv` e `summary.json`. Contadores de memória ausentes abortam o coletor; processos de Chromium com caminho protegido continuam incluídos pelo nome. Nenhuma árvore incompleta foi aceita como medição final.

## Protocolo e confirmação de áudio

Windows: duas rodadas com ordem invertida; 30 amostras de aproximadamente 1 s por aplicativo e estado em cada rodada; aquecimento de 8 s tocando e 6 s pausado. Os dois clientes permanecem carregados, um deles pausado, e medem-se apenas os processos pertencentes ao aplicativo alvo. Tonalify inclui librespot e seus filhos; Spotify inclui seus processos de interface, GPU e serviços.

A reprodução foi confirmada pela API da sessão e pelo pico não nulo da sessão Core Audio pertencente ao processo alvo, antes de cada rodada. Contextos artificiais de play por URI não foram tratados consistentemente pelos clientes na preparação; tentativas sem áudio foram descartadas. A faixa foi inicialmente aberta pelo protocolo do cliente oficial. Nas rodadas válidas, o contexto já carregado foi transferido para cada dispositivo com reprodução ativa e ajustado a 30 s pela API da sessão autorizada. Não se mede um aplicativo parado como se estivesse tocando.

Android: duas rodadas em ordem invertida, 6 amostras de 5 s por aplicativo em primeiro plano; 3 amostras no cenário de segundo plano; 6 amostras com o Hub. Aquecimento de 10 s. Memória e CPU somam todos os PIDs, incluindo o Hub. A coleta de streaming Tonalify teve avanço de PCM de **4,708,160 para 6,677,888 frames**, sem aumento dos underruns (**0 → 0**). Isso não mede estalos acústicos, uma troca de rede ou Bluetooth físico.

As amostras sucessivas não são observações independentes de uma população. Os intervalos mín./máx. são descritivos; não são intervalos de confiança. RAM/CPU podem mudar com cache, tamanho da biblioteca, tela, letras, efeitos, atualizações e tarefas do sistema.

## Abertura, GPU, armazenamento e lacunas

No Windows, estes números são **tempo até o processo aparecer**, com mecanismos de abertura diferentes (executável direto e protocolo Store), sem medir prontidão da interface e sem limpar cache do sistema. Não sustentam um vencedor em abertura.

| Aplicativo Windows | Três tentativas, ms |
|---|---|
| tonalify | 51.10, 51.54, 170.94 |
| spotify | 1267.88, 1245.40, 1269.89 |

Android: `am start -S -W`, processo encerrado antes de cada tentativa, cache do sistema preservado. `TotalTime` mede abertura da Activity inicial sem login, não prontidão do catálogo ou do áudio.

| Aplicativo Android | Três tentativas, ms | Mediana ms |
|---|---|---:|
| Tonalify | 943, 1182, 1235 | 1182.00 |
| Spotify | 2262, 1845, 1775 | 1845.00 |

GPU: snapshots dos engines atribuídos aos PIDs antes da coleta, no JSON; não há série contínua nem energia/VRAM comparável. Contadores zerados não provam ausência de uso. Tamanho do APK/instalador abaixo é o arquivo distribuído, não o espaço total instalado/cache. Bateria, temperatura, dados móveis, latência acústica, Bluetooth do carro, estabilidade por horas e transições Wi-Fi/4G no aparelho físico permanecem sem medição nesta rodada.

## Funcionalidades e qualidade

| Recurso | Tonal Hub 0.8 | Spotify oficial |
|---|---|---|
| Busca de artistas, álbuns e faixas Spotify | Tonalify, conta autorizada; streaming via librespot depende de Premium | Catálogo e integração oficiais |
| Outras fontes e raridades | IndieTonal: Audius, Archive/LMA/Aadam Jacobs, MusicBrainz; Jamendo com credencial e OpenSubsonic configurados; disponibilidade varia | Não agrega essas fontes externas; conteúdos do próprio catálogo variam |
| Playlists e favoritos | Biblioteca pessoal local e integração já existente com Spotify; coração preenchido; exportação pessoal | Playlists, favoritos e sincronização da conta |
| Histórico e estatísticas | Buscas locais; tempo, escutas qualificadas, músicas, álbuns e artistas; opção nas listas; gráficos clicáveis | Recursos oficiais de atividade e retrospectiva; não foi auditada equivalência de cada estatística |
| Clima do repertório | TonalMuse Tiny C++ local, 32 KiB, corpus sintético; temas experimentais de letras/estilos, fontes e cobertura | Sem comparação equivalente validada para este classificador |
| Letras e tradução | Busca automática, tradução pt-BR com detecção automática, pares por linha, retomada de pendências | Letras/traduções conforme faixa, dispositivo e disponibilidade; traduções móveis anunciadas globalmente [1] |
| Sincronia e aparência | Desligar destaque, alinhar linha ao áudio, salvar por versão, exportar ajuste; fontes/cores/contorno/neon/brilho/piscante e tela cheia | Não foi encontrada/documentada equivalência para os ajustes locais desta versão; não é prova de ausência em versões futuras |
| EQ e efeitos | 10 bandas, presets, graves, compressor, limiter e perfis de saída; C++ nativo no DSP | Equalizador documentado no desktop e Android [2] |
| Espacial | HRTF OpenAL Soft; IAMF apenas se existir no arquivo compatível | Recursos variam com plataforma/serviço; não auditados aqui |
| Qualidade Spotify | 96/160/320 kbps; não há implementação Spotify lossless nesta versão | Automática e níveis conforme plano [3]; lossless até 24-bit/44,1 kHz documentado para clientes/dispositivos compatíveis [5]; disponibilidade e ativação nesta conta não verificadas |
| Uso sem rede | Buffer e cache temporários de streaming; arquivos locais; não equivale a downloads permanentes de álbuns Spotify | Downloads offline Premium e regras oficiais de uso [4] |
| Bluetooth | PCM via saída do Android; perfis de efeitos e buffer; o sistema/receptor negociam o codec | Integração do cliente oficial; codec também depende do sistema/receptor |
| Backup | Exportação/importação pessoal e cópias de segurança; conta e senha ficam fora | Biblioteca sincronizada com a conta; downloads não equivalem a backup pessoal |

Não existe conversão que recupere informação perdida de uma fonte com compressão. EQ, HRTF e mais volume não demonstram maior fidelidade; ganhos altos podem reduzir headroom e dinâmica. Bluetooth recodifica conforme aparelho/codec/receptor. Nenhuma medição feita aqui comprova “som superior”, baixa latência garantida ou ausência absoluta de falhas.

[1] [Anúncio oficial de traduções e letras offline](https://newsroom.spotify.com/2026-02-04/lyric-translations-offline-previews/), consultado em 17/10/2026.

[2] [Equalizador Spotify](https://support.spotify.com/br-pt/article/equalizer/), consultado em 17/10/2026.

[3] [Qualidade do áudio Spotify](https://support.spotify.com/br-pt/article/audio-quality/), consultado em 17/10/2026. Recursos adicionais dependem do plano, região e dispositivo; este relatório não verifica lossless na conta.

[4] [Ouça offline — Spotify](https://support.spotify.com/br-pt/article/listen-offline/), consultado em 17/10/2026.

[5] [Qualidade lossless — documentação oficial](https://support.spotify.com/il-en/article/lossless-audio-quality/), consultada em 17/10/2026. Não há prova de uso de lossless nesta medição, nem de transporte lossless pelo Bluetooth do usuário.

## Correções entregues e validação

Capas: metadados do álbum acompanham as faixas da fila; atualizações parciais preservam imagens conhecidas; cache compartilhado e tentativas limitadas. Ícones mantêm proporção e alinhamento; álbum/artista têm hierarquia e metadados legíveis. Estatísticas resumidas podem ser mostradas/ocultadas. Cards do repertório separam temas, artistas, álbuns e estilos, com consulta a Spotify/MusicBrainz e identificação de ambiguidades; dados ausentes ficam pendentes, sem inventar gêneros ou ranquear “Não informado”.

Sincronia pode ser desativada ou alinhada clicando na linha atual, sem pular o áudio. Ajustes são por gravação/versão da letra. Exportar ajuste gera arquivo local; não envia automaticamente ao LRCLIB. Paletas e seletor atualizam o hexadecimal imediatamente. Estilos originais/tradução independentes incluem contorno, brilho, neon, piscante, negrito, itálico e sublinhado. Uma falha de tradução não interrompe as linhas seguintes, e o botão retoma pendências.

Durante a preparação, foi corrigida também a reconciliação da pausa física no Windows quando o Spotify Connect retoma sem o evento de transporte esperado. O estado confirmado da sessão serve de apoio; comandos locais recentes são protegidos e consultas antigas são descartadas. A regressão cobre retomada remota, pausa, outro dispositivo e consulta em voo durante a pausa.

- Windows: 8/8 testes CTest e 6/6 testes da tela QML real; instalador final validado em instalação isolada, atualização preservando arquivo pessoal, abertura sem SDK e desinstalação.
- Android: 34 verificações de núcleo e 4 cenários de tradução (retry, falha isolada, retomada e final em cache). Testes de página de música/letras/tela cheia, artista/álbum com Spotify real, capas, estatísticas e avanço de PCM passaram.
- APK Release atualizado sem desinstalar, com UID, pasta de dados e data da primeira instalação preservados. ARM64 e universal verificados quanto às bibliotecas/empacotamento e alinhamento de 16 KiB. APK para POCO X7 Pro: ARM64.
- Nenhum teste emulado substitui a validação de Bluetooth/rede no carro ou telefone físico. A página de letras depende da cobertura e disponibilidade dos provedores.

## Arquivos e integridade

Instalador Windows instala Tonal Hub, Tonalify e IndieTonal. APKs usam a mesma identidade de assinatura local da atualização testada. Código correspondente das bibliotecas de áudio e avisos acompanham o backup; as versões são experimentais privadas.

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
| Tonal-Hub-0.8.0.0-Windows-x64-Setup.exe | 81,414,615 | `a5ac9320fdfd56bc8a2295c0177ef9784a2a028d6b7b07c5c31a22db4a73cbf0` |
| Tonal-Android-0.8.0-arm64-v8a.apk | 24,723,593 | `db42ebe2c69e722b70b35d636a5a4361cef2ac7e4936f3c054eceb3742f64252` |
| Tonal-Android-0.8.0-local.apk | 46,597,624 | `6f75d13d318a9c179963b61e2ea3c7c318ef0df20b21499bd85763a1889ac18c` |
| Tonal-Android-0.8.0-audio-sources.tar.gz | 49,126,429 | `c526e7f546a4c0d5495aa09d55e110527d9064628b5c94f49195a5d6bac9b405` |

Dados revisados: `benchmarks/2026-10-17/summary.json`, `samples.csv` e `report.html`. Coletores: `tests/benchmark/`. Dados brutos em `out/benchmark-2026-10-17`, fora do Git. Resumo publicado não contém tokens, IDs de dispositivo nem nome da máquina. Os valores iniciais com coleta incompleta do Hub Android e a tentativa Windows sem áudio foram descartados.
