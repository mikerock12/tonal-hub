# Halloween — entrega 0.15.4

O tema Halloween é selecionável no catálogo de temas do Windows e Android. Ele usa roxo escuro e laranja, teias nas capas, controles em brasa e marcador em forma de gota. Na área de letras, os cenários Halloween, Bruxas, Monstros e Vampiros são escolhas independentes do tema do aplicativo. Seus elementos se movem apenas enquanto a tela de letras está ativa e a reprodução está em andamento.

A implementação usa formas vetoriais e gradientes nativos. O IndieTonal no Windows tem seletor próprio de tema e usa o catálogo na tela principal e no painel de música; sua escolha é independente da do Tonalify no PC. As capturas de validação usam capa, nomes e letras sintéticos, identificados como tal. Não representam streaming Spotify ou teste no telefone físico.

## Sangue animado e thumb com volume

Pedido adicional do usuário em 28/10/2026: escorrimentos animados e mais realismo.
A barra agora desenha sete fios de comprimento variável, pequenas gotas que se
desprendem e somem, reflexo superior e gradação de vermelho. O thumb tem curva
de gota, sombra de volume e reflexos. É uma representação vetorial estilizada,
sem GIF/vídeo ou simulação física de fluidos.

O relógio fica limitado a um quadro a cada 67 ms (até aproximadamente 15 fps),
somente na barra de música visível e durante reprodução. Janela sem foco,
controle oculto, música pausada e troca para outro tema interrompem a animação.
No Android, a opção do sistema que desativa animações também é respeitada.
Volume e prévias de tema têm desenho estático. As cores do restante dos temas
e o algoritmo de seek não mudaram; o relógio só redesenha, sem enviar comandos
de áudio, volume ou posição. Há um custo de renderização; não se promete custo
zero, ausência absoluta de impacto ou desempenho em hardware não medido.

Windows compartilha o Canvas BloodSurface entre barra principal, página da
música e IndieTonal. Android usa Drawable nativo com Paint/Path e gradientes
reutilizados; nenhuma inferência, rede ou banco participa desse desenho.

## Validação e atualização

O catálogo passou em 213 verificações. O teste QML dedicado captura os quatro
cenários, confere troca/retorno do tema com arte já carregada, compara quadros
dos escorrimentos e verifica a imobilidade em pausa/ocultação. O teste nativo
Android verifica o relógio visível, pausa, ocultação e retorno em AVD dedicada.
Resultados finais, tentativas e limites devem ser registrados no manifesto de
entrega. A validação física no POCO X7 Pro permanece pendente.

Android 0.15.4 usa versionCode 24 e a mesma identidade de assinatura da 0.15.3.
A identidade é conferida por certificado dos APKs, sem copiar/publicar a chave.
Atualize por cima, sem desinstalar, para preservar biblioteca, preferências e
modelo local. Os pesos continuam opcionais e fora dos instaláveis/backups.
