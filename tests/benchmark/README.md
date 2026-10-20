# Benchmarks locais

## Coleta 0.12.0 — 19/10/2026

`windows_0120.py` registra a série exploratória e o inventário do produto instalado.
`windows_minimized_0120.py` exige os dois players previamente minimizados pela UI,
verifica esse estado antes/depois e coleta duas rodadas invertidas de 60 intervalos
por app/estado. O contexto real da faixa deve estar carregado no Spotify oficial.
Após pausa longa, o coletor retoma esse transporte antes de transferir a reprodução;
ausência de faixa/áudio aborta a etapa. Não publicar janela abortada como válida.

`android_0120.py` usa a variante `comparison` 0.12.0 e o AVD exclusivo, abrindo o Hub
antes do Tonalify. Inspeciona as telas sem login, preserva dados e mede dois blocos
de seis intervalos de cinco segundos, por app e visibilidade. Não é streaming.

`render_0120.py` exporta campos selecionados para `docs/benchmarks/2026-10-19`,
gera Markdown/JSON/CSV e gráfico estático com matplotlib. Requer as coletas completas
sob `out/benchmark-2026-10-19`; a série exploratória preservada pode ser parcial.
Revise a saída pública e os limites em `docs/BENCHMARK_0120.md` antes do push.

## Coleta histórica 0.8 — 17/10/2026

Execute apenas com autorização do titular das sessões. Os coletores não exportam tokens.
Python requer `psutil`. Os scripts de Windows usam PowerShell, contadores de processo,
Core Audio e a credencial do próprio Tonal no Windows Vault para controlar a mesma faixa.
Nunca substitua isso por extração de credenciais de outro aplicativo.

`windows_comparison.py`: feche builds, emuladores e tarefas pesadas; os dois players devem
estar autenticados. Soma todos os processos Spotify e Tonalify/librespot. A RAM principal
é USS (working set privado). 100% de CPU corresponde a um núcleo. A organização das
janelas não é imposta pelo coletor; registre-a no relatório. A primeira aparição do
processo não equivale a uma interface pronta.

`android_comparison.py`: usa somente o AVD de teste declarado no script. A variante
`comparison` tem perfil separado e não é debuggable. As duas telas iniciais devem estar
sem login. `measure.py` soma PSS, swap e CPU de **todos** os PIDs do pacote, incluindo o
Hub. A soma de RSS não substitui PSS. Não limpe dados pessoais para preparar o teste.

`android_streaming.py`: mede separadamente streaming autenticado no Tonalify de teste,
com avanço de PCM. Não transforma esse valor em comparação com o Spotify sem uma
amostra equivalente autenticada. O emulador não representa bateria, Bluetooth ou
latência acústica do POCO X7 Pro.

Saídas brutas ficam em `out/benchmark-2026-10-17`, ignorado pelo Git. Publique somente
resumos revisados, sem IDs de dispositivos, nomes de máquina, dumps pessoais ou sessões.
Os caminhos e versões destes roteiros documentam uma execução, não um laboratório
universal. Repita com mais duração e aparelho físico antes de generalizar os resultados.
