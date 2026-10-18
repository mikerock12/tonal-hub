# Correção de integridade do áudio no Windows — 0.8.1

O relato foi de uma faixa que ficou muda, com estalos/ruídos, enquanto outra
tocava normalmente. O registro anterior não contém a falha nem identifica
com certeza a faixa afetada. Portanto, não se atribui o incidente a um arquivo
corrompido do Spotify sem evidência.

## Defeito reproduzido

O pipe nativo transporta PCM estéreo float32: cada quadro ocupa oito bytes.
`NativePcmPipe::discard()` apagava tanto bytes incompletos já lidos quanto
qualquer quantidade de bytes disponível no pipe. Se a limpeza terminasse no
meio de um quadro, a próxima leitura começava no meio de um float/canal.
Os bytes seguintes eram interpretados como outras amostras: ruído, números
inválidos ou amplitudes incorretas. Trocar/buscar novamente não garantia
realinhamento. Um pipe/stdout é um fluxo de bytes, sem fronteiras de quadros.

Um teste novo, executado antes da correção, falhou em
`PCM alignment after discard offset=1 pending=0 repeated=0`.
Após a correção, 28 combinações de cortes entre os bytes 1–7, dados pendentes
e limpezas repetidas preservam exatamente os valores e a ordem dos canais.

## Alterações

- A posição de bytes é conservada durante leituras e descartes. O restante de
  um quadro antigo incompleto é descartado quando chegar, sem contaminar o novo.
- Pré-carregar outra música não limpa o áudio que está tocando. A mensagem
  `Loading` do librespot também é emitida por tarefas de pré-carregamento;
  somente comandos de carregamento/reprodução e busca justificam essa limpeza.
- PCM com NaN, infinito, quadro incompleto ou amplitude absurda é recusado antes
  dos filtros e na saída OpenAL. O limite de segurança é 32 vezes a amplitude
  nominal, com folga para PCM float acima de 0 dBFS. Silêncio válido não é falha.
- Um erro durante processamento Spotify descarta a saída e o grafo afetados e
  aciona a recuperação existente. As preferências de DSP continuam habilitadas.
  Há até três tentativas automáticas na janela de cinco minutos; falha
  persistente interrompe o ciclo e permite reinício manual.
- Os registros limitados e sanitizados passam a conservar avisos do decoder e
  identificação de carregamento para auxiliar diagnósticos de faixa/cache.

Não foi necessário apagar o cache de músicas nem refazer login. Favoritos,
playlists, histórico, volume e equalizador são preservados.

## Validação e limites

As regressões incluem alinhamento exato, rejeição de dados inválidos antes de
contaminar o estado dos filtros, recusa no dispositivo de saída, recuperação de
um produtor defeituoso e limite de reinícios para defeito persistente. A
regressão de alinhamento também roda no CI Windows sem depender do SDK FFmpeg.

Testes com saída OpenAL nula verificam PCM e transporte; não são uma avaliação
auditiva da caixa de som, do Bluetooth ou do conteúdo específico do incidente.
Resultados da validação final e do teste com sessão real constam em
`audio-081-tests.json`.

Esta versão corrige o caminho de áudio do Windows. O Android usa outro caminho
nativo e mantém os APKs 0.8.0 já publicados no backup privado.

Instalador: `Tonal-Hub-0.8.1.0-Windows-x64-Setup.exe`.
Créditos e licença do projeto original de Matheus Nunes estão em `CREDITS.md`.
