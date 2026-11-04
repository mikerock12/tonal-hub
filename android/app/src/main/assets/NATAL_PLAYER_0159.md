# Natal, seek e áudio no início da faixa — 0.15.9

Entrega Windows/Android 0.15.9, Android versionCode 29, 04/11/2026.

## Fios e luzes sobre a capa

Na página da música, os fios contornam a capa de verdade: quatro trechos passam pela frente e cinco por trás, unidos na margem do cartão. Uma lâmpada atrás é cortada pela borda da capa. A luz vem de cima e da esquerda: o que flutua sobre a capa projeta sombra curta nela, e no cartão, mais distante, a mesma sombra é mais suave. A capa também sombreia o fio que entra por baixo dela.

São 65 lâmpadas modeladas como C9, com vidro cônico, soquete e reflexos, em quatro cores. Quatro padrões se revezam com transição: cintilação, onda, alternância e faíscas.

Fio torcido, soquetes e sombras são pré-renderizados por `packaging/generate-christmas-assets.py`, com desfoque real. Em execução só muda a opacidade dos sprites das lâmpadas acesas. As imagens ficam em `resources/seasonal`, com cópia idêntica em `android/app/src/main/assets/seasonal`.

## Seek

O fio é uma corda de luzes apagadas. Cada lâmpada acende quando a estrela a alcança, com um clarão que se assenta na cintilação; voltar a barra apaga as lâmpadas de novo. Quem decide é só a posição: nada acende por relógio. A estrela tem facetas, brilho pulsante e raios girando devagar. Nenhum tick emite seek ou áudio.

## Papai Noel

Só existe na capa grande da página da música, por decisão do usuário. Início e listas mostram no máximo uma imagem parada dos fios, sem relógio; `tests/themes/check_themes.py` impede que a animação vaze para outros lugares.

O clipe fornecido era um vídeo opaco de 944×944 com o xadrez gravado nos pixels, câmera à deriva e sem ponto de loop. `packaging/generate-christmas-santa.py` remove o xadrez, gera alpha e sombras, estabiliza os quadros e mantém fundo estático nítido para árvore, trenó, renas e brinquedos, de modo que só o Papai Noel se move. As luzes da árvore piscam um número inteiro de vezes por ciclo. O ciclo tem 81 quadros (3,38 s a 24 fps) e fecha entre poses coincidentes de um trecho lento, misturado por fluxo óptico. O salto do fechamento mede 2,82 contra 3,68 de média entre quadros vizinhos, ou seja, é menor que o passo normal.

Resultado: `santa.webp`, 384×384, 1,8 MiB. A animação anda só com a música tocando e a página visível; ao pausar, o quadro atual fica parado. O fundo não é uma simulação nem foi gerado pelo Tonal: é uma adaptação do material do usuário.

## Prioridade da música

- Android: decodificação na thread de animação da plataforma, um quadro por vez, sem manter os 81 quadros em memória. Windows: `AnimatedImage` com `cache: false`.
- Um relógio único de 15 fps para capa, seek e aro do play. Relógios separados redesenhavam a janela inteira fora de fase e eram a maior parte do custo medido.
- Qualquer underrun novo silencia as decorações por 30 s no Android. Economia de bateria, `isLowRamDevice`, animações do sistema desligadas, janela sem foco e página oculta também as interrompem.
- O fio apagado da seek é pintado uma vez e reutilizado como bitmap.
- Nada disso toca o callback PCM. A saída de áudio continua na própria thread, com prioridade de áudio.

Medição na AVD dedicada, 60 s por rodada, tom sintético numa `AudioTrack` real, mesma página:

| Tema | CPU (% de um núcleo) | PSS | Quadros/s | Underruns |
|---|---|---|---|---|
| Tonal | 4,0–4,6 | 78–79 MiB | 0 | 0 |
| Natal | 42,6–43,1 | 96–97 MiB | 33 | 0 |

Antes do relógio único o Natal media 44–50% a 41 quadros/s, mesmo sem Papai Noel nem lâmpadas da capa. Parte do custo vem da GPU emulada: isto compara temas, não é benchmark de celular. O consumo real no aparelho e o impacto na bateria não foram medidos. O custo existe e é maior que o de um tema comum; por isso o tema Natal não é padrão e as proteções acima existem.

## Apito no início das músicas

Causa encontrada no Android: ao trocar de música, o motor fecha a saída temporariamente e o `AudioOutput` a pausava sem descartar o que já estava na fila. Na retomada esse resto da música anterior tocava primeiro. Em `AudioTrack` real a sobra mediu **74 ms**. Um trecho curto de nota sustentada, logo antes da música nova, soa como um bip, e é mais perceptível em volume alto. O Windows já descartava esse áudio em `command=Load {`.

Agora o motor avisa quando fecha a saída para carregar outra faixa (`SinkStatus::TemporarilyClosed`), e uma seleção feita durante a pausa marca o mesmo estado. Na retomada a fila é descartada: a sobra mede 0. Uma pausa comum na mesma música continua preservando o buffer, assim como `requeue` e `editqueue`, que mantêm o decoder audível. O painel de diagnóstico mostra `sobras descartadas` com a contagem e a duração.

Limite importante: o defeito era real e foi corrigido, mas não foi possível confirmar que era exatamente o som ouvido no carro. Não houve teste com Spotify autenticado, Bluetooth ou o rádio do carro. Se o apito continuar, os próximos suspeitos são o início do stream Bluetooth no próprio rádio e o modo experimental HRTF em caixas/carro; o diagnóstico acima ajuda a separar os casos.

## Validação

Detalhes em `release-0159-tests.json` e `artifacts-0159.json`. Capturas com dados sintéticos. Não houve teste em aparelho físico, streaming Spotify autenticado, Bluetooth, carro, medição acústica nem benchmark em celular.
