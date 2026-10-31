# Música, letra e artista — 0.14.0

Abra a música tocando pelo miniplayer. A página tem três seções verticais:

1. **Música:** capa, título, artista, álbum, favorito, compartilhar e controles.
2. **Letra:** original e tradução, sincronia e personalização existentes.
3. **Artista:** informações da gravação, trajetória e conexões identificadas,
   com sugestões para continuar a descoberta.

Deslize/role para mudar de seção. A transição encaixa a seção na área disponível
abaixo do cabeçalho; ao parar, não ficam pedaços dos outros cartões aparecendo.
Os atalhos Música, Letra e Artista também permitem navegar diretamente.
A leitura da letra mantém sua própria rolagem. No Android, um novo gesto no
limite dessa rolagem permite passar à seção vizinha; os atalhos estão sempre
disponíveis. Na tela cheia, continuam as regras de primeiro toque para revelar
controles e segundo toque numa linha para alinhar. A troca de música atualiza a
letra sem sair da seção escolhida.

## Compartilhar

Na tela da música, toque no ícone com três pontos conectados, junto ao coração
no Android e no cabeçalho da página no Windows. O link é o da faixa:
`https://open.spotify.com/track/<id>`. Não é um link de áudio nem um download.

Escolha uma cor pronta ou digite `#RRGGBB`, inclua/oculte a capa e escolha
**Cor sólida**, **Degradê** ou **Papel / polaroid**. A prévia e a exportação usam
o mesmo desenho. Título e artista permitem quebra de linha; títulos muito longos
usam reticências no espaço reservado. O cartão identifica Tonalify e o Spotify.
Uma capa indisponível usa um desenho de reserva.

- **Android:** Copiar link, Enviar link ou Compartilhar cartão + link. Esta
  última opção abre o seletor nativo do Android; você escolhe o aplicativo e o
  destinatário. Alguns aplicativos receptores ignoram o texto junto da imagem;
  nesse caso, use também Copiar link / Enviar link.
- **Windows:** Copiar link e Salvar cartão PNG. Envie a imagem e o link pelo
  aplicativo que preferir. Não há envio automático nem acesso a contatos.

O cartão congela a identidade da faixa ao abrir: se a música seguinte começar,
a prévia ainda corresponde à música que você escolheu compartilhar. A imagem
tem 600 × 1066 pixels. No Android, somente PNGs gerados em `cache/share-cards`
são expostos pelo provider privado, com concessão temporária de leitura; caminhos
arbitrários e escrita são recusados. Permanecem no máximo oito cartões recentes,
com limpeza dos anteriores a um dia na próxima geração. Esses arquivos são cache,
não o backup da biblioteca.

## Informações e sugestões do artista

Os fatos vêm do [MusicBrainz](https://musicbrainz.org/doc/MusicBrainz_API):
origem, país, formação/nascimento, período registrado, estilos e até seis
conexões/integrantes, conforme o cadastro. Não há texto biográfico inventado
nem uma LLM. A fonte aparece na tela. A base colaborativa pode estar incompleta.

A busca exige nome exato normalizado, escore de busca ≥98 e resultado único.
Se houver homônimos, tenta identificar o artista pelos créditos do álbum, após
retirar qualificadores entre parênteses/colchetes. Créditos ambíguos continuam
sem atribuição. O cache usa artista **e álbum**, evitando reutilizar a identidade
de outro artista homônimo. A ordem de membros é a fornecida pela fonte;
“integrantes registrados” não significa necessariamente a formação atual.

**Continue a descoberta** usa relações editoriais de `resources/music-rules.json`,
por artista/estilo, excluindo o próprio artista e limitando a oito sugestões.
É identificado como sugestão editorial, não como recomendação oficial Spotify
nem conclusão de semelhança acústica. Tocar em um nome busca o artista no player.

Consulta sob demanda ao abrir Artista; até três pedidos por identificação
(busca, eventual álbum, detalhes), sem consultar cada faixa da fila. Cache de 32
identidades, sete dias para fatos e uma hora para identificação insuficiente;
no Windows, o cache dura apenas a sessão; no Android, fica no cache do app.
Falhas de conexão no Android podem ser consultadas novamente, respeitando a
espera compartilhada do serviço. Os consumidores MusicBrainz deste player
compartilham o intervalo mínimo de 1,25 s e espera após HTTP 429/503.
Respostas são limitadas (Windows 1 MiB; Android MusicBrainz 512 KiB), com
timeouts e descarte de resultados antigos. Não interfere no callback de áudio.
Os efeitos e o visualizador da letra ficam inativos quando essa seção está oculta.

## Playlists com setlist.fm no Android

Abra **Biblioteca → Criar playlist → Criar com setlist.fm…**. Também há um
atalho na biblioteca pessoal. O Windows mantém o assistente Setlists existente.

1. Cole sua chave setlist.fm no campo protegido e informe a banda.
   A chave própria é exigida pela [API setlist.fm](https://api.setlist.fm/docs/1.0/index.html),
   separadamente do token Spotify. “Obter chave gratuita” abre a página do serviço.
2. Busque shows e veja suas músicas antes de selecionar. Há paginação de até
   cinco páginas. A busca trata chave recusada, limite do serviço e indisponibilidade.
3. Marque um ou mais shows e use **Revisar músicas dos shows selecionados**.
   Intros marcadas como `tape` são descartadas; ordem dos sets/encores e reprises
   são mantidas. Covers buscam a versão do artista do show; não substituem
   silenciosamente a banda pelo autor original da composição.
4. O Text to Tonalify recebe o texto e a procedência dos shows. Revise as versões,
   personalize nome, descrição e capa e salve primeiro no aparelho. A cópia
   privada/pública no Spotify continua sendo uma escolha explícita posterior.

Até 500 músicas por importação. Setlist não garante que a gravação do show
exista no Spotify: as faixas são as versões escolhidas na revisão. A chave
Android é criptografada com Android Keystore, em namespace próprio; não entra
na playlist nem na exportação pessoal. Esquecer chave invalida respostas
pendentes antes que elas possam salvá-la novamente. Não há cópia automática
da chave do cofre do Windows para o telefone.

## Validação e instaláveis

Consulte `song-pages-0140-tests.json` e as capturas com procedência em
[GUIA_VISUAL.md](GUIA_VISUAL.md). Componentes Qt renderizados com dados sintéticos
e AVD dedicada não equivalem a teste no telefone físico. A automação nativa da
janela Windows encontrou `GetCursorPos: acesso negado`; isso fica registrado.
Sem novo benchmark, teste físico Bluetooth ou publicação de playlists reais
nesta entrega. As medições 0.12 continuam no [índice de benchmarks](BENCHMARKS.md).

Instaláveis 0.14.0: Setup Windows x64, APK ARM64 e APK universal (ARM64+x86_64),
versionCode 19. Atualize por cima para preservar dados e mantenha uma exportação
da biblioteca. Créditos originais de **Matheus Nunes (natheus)** e MIT preservados.

## Revisão 0.15.7

Corações ficam visíveis junto das músicas nas coleções. Em paisagem, o Android apresenta player à esquerda e letra à direita, com capa/metadados compactos e uma faixa rolável para os modos; ao voltar a retrato, restaura a página anterior sem recriar Activity, motor ou fila. Detalhes: [NATAL_PLAYER_0157.md](NATAL_PLAYER_0157.md).
