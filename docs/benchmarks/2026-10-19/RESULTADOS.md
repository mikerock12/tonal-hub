# Resultados — Tonalify 0.12.0 vs Spotify

Coleta de 19–20/10/2026 (horário de São Paulo). [Método, condições e limitações](../../BENCHMARK_0120.md).

Windows minimizado tocando a mesma faixa: **77.80% menos RAM exclusiva no Tonalify nesta amostra**. A direção da diferença de CPU mudou entre rodadas; não houve vantagem consistente de CPU. DSP/qualidade não foram igualados.
Android compara as telas sem login, com Hub no Tonalify. Isso não demonstra economia durante streaming; manter esse cenário sem login foi autorizado pelo usuário.

| Série / estado | Tonalify RAM mediana | Spotify RAM mediana | CPU média Tonalify / Spotify |
|---|---:|---:|---:|
| Windows minimizado, pausado | 110.30 MiB USS | 469.32 MiB USS | 0.21% / 0.16% |
| Windows minimizado, reproduzindo | 110.30 MiB USS | 496.83 MiB USS | 0.77% / 0.67% |
| Android emulado, sem login, tela inicial | 31.49 MiB PSS | 156.84 MiB PSS | 0.11% / 1.20% |
| Android emulado, sem login, segundo plano | 27.63 MiB PSS | 151.37 MiB PSS | 0.08% / 0.47% |

100% de CPU = um núcleo. RAM USS Windows e PSS Android não são comparáveis entre si.
Android sem login não demonstra vantagem durante streaming. A série exploratória Windows tem visibilidade desigual.
A série exploratória é parcial: 60 amostras por estado do Tonalify e 120 do Spotify. A série minimizada tem 120 por aplicativo/estado; Android tem 12.

![Comparação das medições](comparison.png)

## Série exploratória parcial — visibilidade desigual

Spotify em primeiro plano e Tonalify coberto. Estes números não sustentam uma comparação justa de CPU; ficam separados da série principal minimizada.

| Estado | Tonalify RAM / CPU | Spotify RAM / CPU |
|---|---:|---:|
| reproduzindo | 142.56 MiB USS / 0.57% | 525.98 MiB USS / 20.67% |
| pausado | 142.77 MiB USS / 0.12% | 520.41 MiB USS / 0.65% |

## Rodadas e dispersão

| Série / app / estado | Rodada | RAM mediana (MiB) | CPU média | CPU p95 agregado |
|---|---:|---:|---:|---:|
| Windows minimizado / tonalify / pausado | 1 | 126.56 | 0.29% | 1.50% |
| Windows minimizado / tonalify / pausado | 2 | 94.02 | 0.12% | 1.50% |
| Windows minimizado / tonalify / reproduzindo | 1 | 126.55 | 0.84% | 2.98% |
| Windows minimizado / tonalify / reproduzindo | 2 | 94.11 | 0.70% | 2.98% |
| Windows minimizado / spotify / pausado | 1 | 473.09 | 0.14% | 1.46% |
| Windows minimizado / spotify / pausado | 2 | 465.41 | 0.17% | 1.46% |
| Windows minimizado / spotify / reproduzindo | 1 | 531.34 | 0.58% | 2.91% |
| Windows minimizado / spotify / reproduzindo | 2 | 460.56 | 0.75% | 2.91% |
| Android emulado, sem login / tonalify / tela inicial | 1 | 31.24 | 0.09% | 0.37% |
| Android emulado, sem login / tonalify / tela inicial | 2 | 31.75 | 0.12% | 0.37% |
| Android emulado, sem login / tonalify / segundo plano | 1 | 27.35 | 0.12% | 0.19% |
| Android emulado, sem login / tonalify / segundo plano | 2 | 27.77 | 0.03% | 0.19% |
| Android emulado, sem login / spotify / tela inicial | 1 | 156.29 | 1.10% | 4.73% |
| Android emulado, sem login / spotify / tela inicial | 2 | 157.55 | 1.29% | 4.73% |
| Android emulado, sem login / spotify / segundo plano | 1 | 152.27 | 0.63% | 2.08% |
| Android emulado, sem login / spotify / segundo plano | 2 | 150.95 | 0.31% | 2.08% |
| Windows exploratório / tonalify / pausado | 1 | 142.77 | 0.12% | 1.50% |
| Windows exploratório / tonalify / reproduzindo | 1 | 142.56 | 0.57% | 2.94% |
| Windows exploratório / spotify / pausado | 1 | 519.93 | 1.07% | 2.91% |
| Windows exploratório / spotify / pausado | 2 | 520.54 | 0.24% | 2.91% |
| Windows exploratório / spotify / reproduzindo | 1 | 539.28 | 21.88% | 34.82% |
| Windows exploratório / spotify / reproduzindo | 2 | 523.43 | 19.47% | 34.82% |

## Streaming Tonalify Android, separado

Duas janelas, 12 amostras: **72.73 MiB PSS** e **4.69% de um núcleo** em média. 0 novos underruns nos intervalos verificados.
Player de teste sem Hub, variante depurável, bibliotecas nativas Release e polling diagnóstico de UI a cada 2,5 s. PCM aceito pelo AudioTrack cresceu; o emulador não tem saída acústica no host.
**Sem amostra equivalente do Spotify oficial autenticado: não calcular economia de streaming a partir das telas sem login.**

P95 usa nearest-rank: valor na posição ceil(0,95×n) da ordenação; não é intervalo de confiança.
CPU da tabela principal é a média dos intervalos. Consulte JSON para mínimos/máximos, commit/RSS, I/O, swap, processos, carga do host e estado das janelas.

## Integridade e reprodução

[CSV](samples.csv) · [JSON](summary.json) · [Método e funcionalidades](../../BENCHMARK_0120.md).
Os hashes identificam os executáveis efetivamente medidos; a variante Android de comparação tem identificador e perfil separados do APK pessoal.
Dados brutos de contas e dispositivos não foram publicados. Nenhum instalável foi alterado por esta entrega.
