# Renderização segura no Windows — 0.17.2

## Incidente de 20/11/2026

Depois de instalar a 0.17.1, o Tonal Hub e o Tonalify abriam e fechavam
sozinhos. O Windows registrou `APPCRASH` em `dxgi.dll` (Direct3D),
exceção `0xC0000005`, deslocamento `0x3d900`.

Investigação, com evidência:

| Verificação | Antes de reiniciar | Depois de reiniciar |
|---|---|---|
| Tonal 0.17.1 instalado | caía | abre |
| Tonal 0.16.2 e 0.16.3 (funcionavam antes) | caíam | — |
| Outro programa sem relação (SmellsLikeTechConverter) | caía no mesmo ponto | — |
| Sonda mínima Direct3D 11, sem Qt (swapchain flip e blt) | caía | 60 quadros OK |
| Mesma sonda com WARP (sem driver Intel) | `dxgi.dll` igualmente alterado | — |
| Código `.text` do `dxgi.dll` alterado na memória | 222 bytes, um a cada 8, logo antes de `0x3d900` | 0 |
| Controle: `d3d11.dll` | 0 bytes alterados | 0 |
| Módulo do Warsaw (`wslbscr64.dll`) injetado | sim | sim |
| OpenGL ou renderização por software | funcionavam | — |

Conclusões: o problema não estava no código do Tonal nem no Qt. O código do
`dxgi.dll` estava alterado na memória do Windows e afetava qualquer app
Direct3D (inclusive com WARP); reiniciar resolveu. O módulo de proteção do
Warsaw (Topaz OFD) continuou injetado depois do reinício sem causar falhas,
então não é tratado como causa nem detectado pelo app. A origem da alteração
não foi identificada.

## Comportamento desde a 0.17.2 (Tonal Hub, Tonalify e IndieTonal)

* Cada programa grava em `%LOCALAPPDATA%\Tonal\graphics\<programa>.txt` um
  marcador antes de abrir a janela e o confirma 3 s depois do primeiro quadro
  ou ao fechar normalmente.
* Se a abertura anterior terminou antes disso, a próxima usa OpenGL; se o
  OpenGL também cair, usa renderização por software.
* A troca nunca é silenciosa: aparece um aviso explicando que o DirectX do
  Windows falhou, a placa de vídeo e o renderizador em uso, com "Tentar
  DirectX de novo na próxima abertura" e "Entendi". Em **Sobre** aparece
  "Desenho da tela: …" e, se houve troca, "(o DirectX falhou neste PC)".
* Cada decisão é registrada em `%LOCALAPPDATA%\Tonal\graphics\<programa>.log`.
* Depois de reiniciar o Windows o app tenta o Direct3D de novo sozinho; se cair,
  o OpenGL volta e o aviso reaparece.
* `QSG_RHI_BACKEND` ou `QT_QUICK_BACKEND` definidos externamente prevalecem.

Limites: a primeira queda não pode ser evitada (o processo some dentro do
`dxgi.dll`); a troca vale da abertura seguinte. Testes com o estado de queda
simulado em diretório temporário, sem reproduzir a falha real (que o reinício
eliminou). Android não muda e continua na 0.17.1.
