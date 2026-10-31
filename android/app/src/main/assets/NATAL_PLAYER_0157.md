# Natal, Halloween e player — 0.15.7

Entrega Windows/Android 0.15.7, Android versionCode 27. O catálogo tem oito temas. Tonalify e IndieTonal compartilham os recursos visuais quando o componente correspondente existe nas duas plataformas.

## Tema Natal

- Verde-pinheiro, vermelho e dourado, com luzes sobre as capas.
- O seek é um fio com lâmpadas apagadas na parte restante e acesas até a posição atual. O thumb é uma estrela. As lâmpadas mudam de cor e têm brilho intermitente durante reprodução, sem alterar posição ou emitir seek.
- Play/pause usa uma grinalda e uma transição vetorial de 340 ms. A transição ocorre nos dois sentidos e acompanha o estado real do player, não apenas o toque.
- Capa, seek e aros usam desenho nativo limitado a aproximadamente 15 fps quando visíveis. Falta de foco, ocultação e a opção Android de desativar animações interrompem o relógio correspondente.

## Halloween revisado

- O botão play de álbuns/playlists deixa de usar a mesma cor no ícone e no fundo.
- Play/pause tem a mesma transição de estado do tema Natal, com o aro de brasa existente.
- As teias têm raios curvos, anéis irregulares e duas passagens de traço. A aranha tem pernas articuladas, corpo com volume e fio; num ciclo de 14 s, desce pela capa, tece e retorna. A animação fica restrita a capas grandes visíveis.
- Sangue e gota usam vermelhos mais vivos, bordas escuras e reflexos. Continua sendo uma representação vetorial, não uma simulação física de fluidos ou aranhas reais.

## Cenários de letras

Halloween, Bruxas, Monstros e Vampiros foram redesenhados com casas, castelo, cemitério, abóbora, caldeirão, morcegos, neblina e iluminação, conforme o cenário. Natal, Neve e Lareira acrescentam árvore/presentes, vila nevada e lareira/meias. Há 18 cenários totais no seletor, incluindo os anteriores e Sem animação. As cenas continuam independentes do tema do aplicativo e só avançam quando sua área está ativa e a reprodução está em andamento.

## Favoritos e rotação

Álbuns, playlists, curtidas e buscas mostram um coração junto de cada música no Android; coleções no Windows também. A ação atualiza a biblioteca local junto com o Spotify, sem reconstruir a coleção ou mudar sua rolagem.

Ao girar o telefone para paisagem, a página da música apresenta player à esquerda e letra à direita, com cabeçalho e identidade do aplicativo. Capa e metadados ficam lado a lado. Anterior/play/próxima permanecem visíveis; repetição, aleatório e demais modos ficam numa faixa rolável. Ao voltar a retrato, a página anterior é restaurada. A Activity, o motor e a fila não são recriados.

## Text to Tonalify

A revisão ganhou **Só não encontradas**, com contagem e acesso a Editar busca. Marcar uma faixa ou escolher outra versão atualiza somente o cartão correspondente, sem recriar a lista nem fazê-la subir. Paginação, edição e tentativa de pendentes continuam disponíveis. Não há escrita no Spotify durante busca, filtro ou revisão.

## Validação local

- Catálogo: oito temas, 243 checagens, ícones e contraste.
- QML: 25 testes de fila/temas, seis de letras e três de playlist; o teste de playlist inclui quatro referências ausentes e preservação da rolagem.
- Capturas QML sintéticas: três testes de execução e comparações de capas, aranha, luzes, transição e sete cenários sazonais.
- Android: compilação release/benchmark; `seasonalTests` aprovado para transições, estrela, fio, luzes e cenas. `halloweenTests` falhou uma vez na espera de retomada do sangue e passou na repetição isolada; a tentativa inicial não foi apagada.
- AVD dedicada: quatro verificações de rotação/cabeçalho e três de filtro/rolagem aprovadas, sem exceção fatal na execução final. Outra AVD foi preservada.
- CTest: a primeira suíte teve quatro aprovações, cinco timeouts e uma falha de saída de produtor de áudio. Os seis testes afetados passaram isoladamente após encerrar a AVD; a suíte completa posterior passou 10/10. A carga foi reduzida, mas causalidade não foi provada.

As capturas e o assistente usam dados sintéticos. Não houve teste no aparelho físico, streaming Spotify autenticado, escrita real de playlist, medição acústica ou novo benchmark. O texto assistente não executou escrita remota. Consulte `release-0157-tests.json` e `artifacts-0157.json` para os artefatos finais.
