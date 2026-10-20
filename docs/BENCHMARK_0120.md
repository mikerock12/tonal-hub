# Benchmark Tonalify 0.12.0 — 19–20/10/2026

Nova coleta local comparando os binários instalados do Tonalify e Spotify.
Os resultados numéricos e gráficos estão no [relatório da coleta](benchmarks/2026-10-19/RESULTADOS.md).
As [amostras CSV](benchmarks/2026-10-19/samples.csv) e o
[resumo JSON](benchmarks/2026-10-19/summary.json) permitem conferir os cálculos.

## Método

Host: Core i5-6500, quatro núcleos/threads, 16 GB, Windows 10 Pro build
19045 e Intel HD Graphics 530 (driver 26.20.100.7262). Na série minimizada,
a CPU total do host teve média de 23,3% e a RAM disponível não caiu abaixo
de aproximadamente 7,6 GiB. É uma máquina de uso normal, com serviços e Codex
abertos, não um laboratório isolado.

Windows: mesma conta autorizada e mesma faixa, **Get Back — Veruca Salt**.
Dois blocos em ordem invertida por aplicativo; cada janela mede 60 intervalos
de aproximadamente um segundo. Aquecimento de 12 segundos reproduzindo e oito
pausado. Áudio efetivo confirmado por pico não nulo da sessão Core Audio do
processo alvo, URI e dispositivo de reprodução antes/depois da janela.
Incluem-se processos filhos do Tonalify e todos os processos Spotify.
No estado pausado, a API confirmou pausa antes da coleta; não houve
monitoramento contínuo do transporte durante os 60 intervalos.
Na série principal, ambos permanecem minimizados; esse estado é verificado
antes/depois de cada janela. A série exploratória inicial foi interrompida
antes do último par do Tonalify e conserva somente seis janelas válidas.
Uma tentativa minimizada anterior também foi abortada quando o processo alvo
deixou de estar disponível; não foi classificada como resultado ou crash
com causa determinada. A coleta controlada foi reiniciada.

CPU é tempo de usuário+sistema dividido pelo tempo de parede;
**100% representa um núcleo**, e não a máquina toda. PIDs que aparecem ou
somem entre amostras não têm todo seu tempo recuperado; a contagem de trocas
de processos está no resumo. RAM Windows principal é USS, memória residente
exclusiva; RSS somado pode contar páginas compartilhadas repetidas. Commit
privado também é informado, mas não equivale a RAM física.
Referência: [documentação psutil](https://psutil.readthedocs.io/).

| Área de hardware | Evidência desta coleta |
|---|---|
| CPU | Tempo de usuário+sistema de todos os processos do alvo, média e p95 por amostra |
| RAM | USS/RSS/commit no Windows; PSS/swap no Android; processos incluídos |
| Disco | Bytes lógicos de leitura/escrita dos processos Windows; não mede IOPS físicos ou vida útil do SSD |
| GPU | Modelo/driver do PC no inventário; utilização e VRAM não medidas |
| Energia | Bateria, watts e temperatura não medidos |
| Rede e áudio | Reprodução verificada; sem teste de velocidade, fidelidade, latência acústica ou Bluetooth nesta rodada |

Android: somente AVD de teste `Tonal_Local_20261015`, Android 16/API 36 x86_64.
Solicitados 3.072 MiB e duas vCPUs; o total efetivo de RAM do convidado consta
no JSON. Boot frio em aproximadamente 71 s. A primeira Activity de login
restaurada do Spotify ficou preta; reabrir o aplicativo sem limpar dados
carregou a tela de boas-vindas. Essa tela preta não entrou na coleta.
Comparação inicial usa variante `comparison` não depurável, com perfil próprio
e bibliotecas nativas Release, preservando contas/dados existentes. O Hub é
aberto antes do player no Tonalify e entra no custo do pacote. Duas rodadas
invertidas, seis intervalos de cinco segundos por aplicativo/estado/rodada,
após 12 segundos de aquecimento. Somam-se todos os processos nomeados do pacote.
PSS divide proporcionalmente as páginas compartilhadas; swap é apresentado à
parte. Referência: [Android dumpsys meminfo](https://developer.android.com/tools/dumpsys#meminfo).

Suplemento de streaming Tonalify: sessão própria autorizada no perfil de
teste, player sem Hub, bibliotecas nativas Release e variante depurável com
polling de UI a cada 2,5 s. Mesma faixa, posição inicial solicitada em zero,
duas janelas consecutivas de seis intervalos de cinco segundos, aquecimento
de oito segundos. Cada snapshot exige um arquivo novo do processo atual;
URI, PCM crescente e saída não pausada são condições obrigatórias. Tentativas
que leram diagnóstico antigo ou foram interrompidas para login foram descartadas.
Essa variante não é uma medição exata do APK Release distribuído.

## Condições que limitam a interpretação

- Janelas Windows e qualidade/DSP não foram inicialmente igualados. A primeira
  série registra se cada janela estava em primeiro plano, coberta ou minimizada.
  Essa série exploratória não sustenta sozinha uma comparação justa de CPU.
- Tonalify usa a configuração pessoal existente: EQ ativo, graves +8 e
  normalização, compressor desligado, espacial original. Configuração de
  qualidade/EQ do Spotify não confirmada; volumes Connect diferem. Não é um
  teste de fidelidade, volume ou latência acústica.
- Pico de áudio antes/depois confirma saída nesses instantes; não constitui
  monitoramento contínuo de estalos, silêncio ou underruns durante toda a janela.
  A falha de contexto após pausa na preparação foi recuperada antes de medir.
- Aplicativos já abertos e caches quentes. Coletor, Codex e serviços normais do
  PC permanecem; não há build pesado ou emulador durante as medições Windows.
  Dois blocos curtos não caracterizam todas as cargas ou hardware.
- A tela Android sem login não realiza streaming. Seus números só permitem
  comparar essa tela; não provam economia enquanto se ouve música.
- O usuário autorizou manter a comparação Android sem login. O cliente oficial
  havia exigido OTP; a tentativa com código novo fornecido pelo titular e uma
  repetição terminou em “Something went wrong”. Não foi determinada a causa.
  Nenhum código, endereço ou identificador de conta entra nos dados públicos.
- Streaming Tonalify em variante depurável, quando medido separadamente, não
  pode ser dividido pelos números do Spotify sem login.
- Emulador não mede consumo elétrico, bateria, Bluetooth ou desempenho no POCO
  X7 Pro. Sem conclusão sobre codecs, cortes no carro ou latência dos fones.
- Não há medição específica do custo da busca de letras das candidatas da
  Descoberta 2+1, de todas as animações ou de tradução nesta coleta.

## Recursos e validação

O [README](../README.md#funcionalidades-implementadas) lista os recursos do
produto. Esta entrega mede consumo; não repete integralmente a validação
funcional do 0.12.0, registrada em
[discovery-enrichment-0120-tests.json](discovery-enrichment-0120-tests.json).

Tonalify acrescenta biblioteca pessoal local, estatísticas detalhadas, temas
do repertório, tradução, personalização de letras, DSP e Descoberta 2+1 com
letras em cache. Spotify oficial oferece a experiência oficial e recursos
conforme conta, região e plataforma. Não se atribui ausência de um recurso ao
Spotify apenas por não tê-lo testado nesta execução. IndieTonal e Hub não são
concorrentes equivalentes ao cliente Spotify e não receberam teste de streaming
comparativo aqui.

## Reproduzir e auditar

Coletores: [Windows](../tests/benchmark/windows_0120.py),
[Android](../tests/benchmark/android_0120.py) e
[coletor Android de CPU/PSS](../tests/android/measure.py).
Saídas brutas permanecem em `out/benchmark-2026-10-19`, fora do Git.
O exportador publica somente campos selecionados, sem tokens, nomes de conta,
IDs de dispositivos, nomes de máquina, PIDs ou bancos pessoais. Não execute
coletores autenticados sem autorização do titular.

Produto Windows/Android de referência: merge
`d4acee3ca35147d289ea9b71dc80038a99950eab`; a entrega do benchmark não altera
o produto ou recompila os instaláveis distribuídos. O projeto deriva do
[Tonal de Matheus Nunes (natheus)](https://github.com/natheus/tonal-player), MIT.
