# Benchmark local 0.8

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
