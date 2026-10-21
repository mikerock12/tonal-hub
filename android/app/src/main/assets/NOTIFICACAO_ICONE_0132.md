# Notificação com capa e ícone no tema — 0.13.2

## Notificação de reprodução no Android

A notificação e o player da tela de bloqueio/ajustes rápidos passam a mostrar
a **capa do álbum**, o **nome da música** e o **nome da banda**, em vez de
“Tonalify / Reproduzindo”. O álbum aparece como linha secundária e mensagens
do motor (reconectando, falhas) continuam visíveis abaixo da música.

A barra do player do sistema mostra a **posição real** e a duração da faixa.
No Tonalify com Spotify, arrastar essa barra faz o seek; em arquivos locais a
barra só acompanha. O IndieTonal também ganhou a capa da gravação.

**Como funciona.** O serviço de reprodução lê a música atual da biblioteca
pessoal uma vez por segundo e publica na MediaSession título, artista, álbum,
duração e capa. A capa é baixada fora da thread principal, reaproveitando o
mesmo cache de capas das telas (até 3 MiB, prazo de 20 s, reduzida a 512 px).
Sem capa disponível, a notificação mostra só o texto. A posição só é
republicada quando o estado ou um salto (seek) muda; o sistema calcula o
avanço entre uma atualização e outra.

## Ícone acompanhando o tema

- **Android:** o ícone na tela inicial (Tonal Hub) muda para a cor do tema:
  círculo na cor de destaque e barras na cor de contraste do tema. Também o
  ícone pequeno da notificação usa a cor do tema. O Tonal Azul mantém o
  ícone original.
- **PC:** o ícone da janela e da barra de tarefas do Tonalify é desenhado com
  as mesmas cores do tema e muda na hora ao trocar de tema. O atalho da área
  de trabalho e o instalador continuam com o ícone original, que é fixo no
  executável.

No Android, cada tema tem um atalho de inicializador (activity-alias); só o
do tema escolhido fica ativo. O novo é ativado antes de o anterior ser
desativado, sem fechar o app nem parar a música. **Alguns inicializadores
(como o do HyperOS/MIUI) podem levar alguns segundos para trocar o ícone ou
remover o atalho da tela inicial**; nesse caso, arraste o Tonal Hub de novo a
partir da gaveta de apps. Os desenhos são gerados de `resources/themes.json`
por `packaging/generate-theme-icons.py`, e a verificação de temas confere se
estão atualizados.

## Validação

| Verificação | Resultado |
|---|---|
| Android na AVD (`tests/android/notification_smoke.py`) | 7 verificações: título na MediaSession e na notificação, relógio de reprodução, ícone do inicializador seguindo o tema e voltando ao Tonal Azul, capa na notificação do IndieTonal com gravação pública, sem exceções |
| Catálogo de temas (`check_themes.py`) | 183 checagens, incluindo ícones por tema atualizados e um atalho por tema no manifesto |
| CTest Windows | 10/10 |

Registro: [notificacao-icone-0132-tests.json](notificacao-icone-0132-tests.json).

**Não validado:** capa de uma música do Spotify na notificação (exige a conta;
o caminho de capa é o mesmo testado com o IndieTonal), o inicializador do POCO
X7 Pro (HyperOS) e o ícone da barra de tarefas no PC com o app aberto
interativamente.

Base original: Matheus Nunes (natheus), [Tonal](https://github.com/natheus/tonal-player), MIT.
