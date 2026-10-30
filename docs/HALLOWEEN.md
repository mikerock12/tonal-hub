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
Android foi preparado para verificar o relógio visível, pausa, ocultação e retorno em AVD dedicada.
Resultados finais, tentativas e limites devem ser registrados no manifesto de
entrega. A validação física no POCO X7 Pro permanece pendente.

Android 0.15.4 usa versionCode 24 e a mesma identidade de assinatura da 0.15.3.
A identidade é conferida por certificado dos APKs, sem copiar/publicar a chave.
Atualize por cima, sem desinstalar, para preservar biblioteca, preferências e
modelo local. Os pesos continuam opcionais e fora dos instaláveis/backups.

Na sessão Windows de 29/10, o launcher do emulador encerrou com 0xC0000135.
Uma cópia isolada das dependências existentes conseguiu iniciar o executável
headless, mas não completou a verificação do hipervisor nem disponibilizou ADB.
Por isso, capturas Android, relógio em execução e instalação `adb install -r`
da 0.15.3 para a 0.15.4 permanecem pendentes. Certificado, pacote, versionCode,
compilação e alinhamento dos APKs foram verificados; isso não equivale a teste
de atualização instalado nem a validação física. Nenhuma outra VM foi encerrada.

## Correção Android 0.15.5 em preparação

Na página da música, a barra atualiza a posição interpolada até 20 vezes por segundo durante a reprodução, sem emitir comandos adicionais de seek. Eventos recentes do motor têm prioridade sobre consultas de estado potencialmente atrasadas; pequenas correções para trás são absorvidas, enquanto saltos maiores e seeks explícitos continuam possíveis. A gota fica abaixo da barra, com o corpo junto à trilha e a ponta voltada para baixo. O efeito não altera o som.

A compilação Java e testes de política não equivalem a teste de streaming autenticado ou validação em aparelho físico. Um APK de atualização precisa ser assinado com a mesma chave da versão instalada; não desinstale o aplicativo para contornar diferenças de assinatura.

## Android 0.15.6 — gota e controles em revisão

Após a avaliação no telefone, a gota foi desenhada com a ponta junto à barra e o corpo arredondado abaixo, usando os vermelhos do próprio rastro. Os botões principais de reprodução nos players recebem aro de brasa e partículas animadas; o desenho é vetorial e estilizado, não os GIFs originais. A animação limita-se a cerca de 15 fps enquanto o botão está visível e tem foco; respeita a opção do sistema que desativa animações. Não altera a reprodução nem os comandos de seek.

O build assinado deve usar o certificado já existente, com versionCode 26, para permitir atualização sobre 0.15.5. Validação visual sintética de componentes não substitui confirmação da aparência e reprodução no aparelho; não publicar o APK antes das verificações de assinatura, alinhamento, atualização e aceitação visual.

## Windows 0.15.6 — correção em validação

O Tonalify e o IndieTonal compartilham o desenho de gota com ponta na barra e corpo pendente e o aro de brasa animado nos controles principais. A superfície usa animação vetorial limitada a 15 fps em janela ativa; pausar/ocultar a tela suspende a animação da barra, enquanto o aro visível continua animado. Atualizações do relógio Spotify ocorrem a cada 100 ms durante reprodução; correções pequenas para trás são absorvidas, mas seeks explícitos e recuos maiores permanecem possíveis. Custo visual não foi medido.

As cenas de letras e recursos da versão 0.15.4 permanecem disponíveis. Verificações Qt offscreen são sintéticas; o instalador e a reprodução real exigem validação separada, e o teste físico do usuário no Android não confirma o Windows.

## Publicação 0.15.6 — 30/10/2026

O usuário relatou teste do APK no celular e autorizou a publicação. Os dois APKs foram conferidos quanto a versão, certificado e alinhamento; seus hashes, os do instalador e do ZIP Windows constam da release v0.15.6. O Windows foi compilado e testado na VM do CI 40996835093: dez grupos de testes, instalação, inicialização com áudio local sintético, reinstalação preservando arquivo e remoção da cópia isolada. Testes QML usam dados sintéticos; não demonstram Spotify autenticado nem hardware físico Windows.
