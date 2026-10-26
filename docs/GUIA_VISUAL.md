# Guia visual: Tonalify e IndieTonal

Este guia mostra **telas reais dos aplicativos e das suítes de interface**,
registradas em diferentes versões, incluindo os temas 0.13.0. Não são
mockups; capturas de teste não representam sessões reais Spotify. Os rótulos distinguem versões e dados
sintéticos. Algumas telas históricas ainda usam botões de texto; as capturas
0.11 mostram o cabeçalho com ícones. Fontes e dimensões variam com o aparelho.

[Voltar ao README](../README.md) · [Descoberta 2+1](DESCOBERTA_2_MAIS_1.md) ·
[Benchmarks](BENCHMARKS.md) · [Origem, dimensões e hashes dos PNGs](screenshots/manifest.json).

**Atualização 0.15.0:** Estatísticas e Backup ficam no cabeçalho global ao lado
de busca, favoritas, áudio e conta. A faixa interna da música contém Agora,
Biblioteca e Letras. As capturas abaixo preservam a versão indicada em cada
legenda. [Guia das estatísticas e histórias locais](STATISTICS_STORIES_0150.md).

## Atualizar letras em tela cheia — 0.15.1

Toque uma vez na tela cheia para revelar os controles. O ícone de seta circular
**Atualizar letra** força uma nova busca para a faixa atual, mesmo com cache.
Uma busca/tradução em andamento continua serializada. O primeiro toque numa
linha apenas revela os controles; outro toque pode alinhar a letra.

<img src="screenshots/android-letra-tela-cheia-0151.png" width="300" alt="Android 0.15.1: controles de tela cheia, incluindo atualizar letra; dados sintéticos na AVD">
<img src="screenshots/desktop-letra-tela-cheia-0151.png" width="480" alt="Windows 0.15.1: atualizar letra e controles com contraste corrigido; fixture QML sintética">

As duas capturas usam o código de produção com textos sintéticos. A do Android
foi obtida na AVD, a do Windows na suíte QML offscreen; não são benchmarks ou
prova de reprodução Spotify no telefone físico. A página da música 0.15.1
retira Agora/Biblioteca/Letras e o X do topo e oferece uma casa para o início.
As imagens das seções históricas abaixo conservam suas versões originais.

## 1. Entenda a navegação e os favoritos

No Android, abra a música pelo miniplayer. O cabeçalho mantém **pesquisa,
favoritos, áudio/efeitos e conta**. Na segunda faixa de ícones, da esquerda
para a direita: **Agora, Biblioteca pessoal, Estatísticas, Letras, Backup e Fechar**.
O coração azul preenchido indica favorito.

<img src="screenshots/android-musica-modos.png" width="320" alt="Android: cabeçalho por ícones, coração azul e modos sob o player em dados de teste">

*Android, validação 0.11.0: nomes e capa são fixtures de teste; o placeholder
é intencional. Não representa uma falha de carregamento de uma capa real.*

Para comparar com uma capa carregada e o favorito ativo:

<img src="screenshots/android-capa-favorito.png" width="320" alt="Android: Get Back com capa American Thighs carregada e coração preenchido azul">

*Android 0.8.0, catálogo da sessão de validação. O cabeçalho com textos é histórico.*

## 2. Shake, Aleatório, Descoberta e fila

No álbum/playlist, os controles aparecem junto do Play. Na página da música,
também ficam abaixo do transporte. Azul indica um modo ativo.

| Ícone | Ação | Como voltar |
|---|---|---|
| Lista em movimento | **Shake:** nova ordem visual temporária a cada toque | Segurar restaura a original |
| Setas cruzadas | **Aleatório:** nova sequência de reprodução | Na fila Android, segurar desliga |
| Lupa/play com + e setas | **Descoberta 2+1:** sugestões externas entre pares de originais | Na fila Android, segurar desliga |
| Lista com play | **Fila:** ver atual/próximas, preparar e aplicar prévia | Fechar abandona a confirmação da prévia |

![Windows: modos no álbum](screenshots/desktop-modos.png)

*Windows, suíte QML 0.11.0: álbum e faixas sintéticos. Os modos alteram a ordem
temporária, sem regravar a playlist salva.*

Depois de escolher modos na página da música, confira a prévia e confirme
**Aplicar e reproduzir fila**. Preparar não interrompe o áudio; aplicar pode
recarregar a faixa atual na posição preservada. Se a faixa mudou, prepare novamente.

<table>
<tr><th>Windows: prévia</th><th>Android: fila preparada</th></tr>
<tr><td><img src="screenshots/desktop-fila.png" width="520" alt="Windows: diálogo de prévia com música atual e próxima"></td><td><img src="screenshots/android-fila.png" width="280" alt="Android: fila com modos e botão Aplicar e reproduzir fila"></td></tr>
</table>

*Capturas de validação 0.11.0, dados sintéticos. No Android o exemplo mostra
Aleatório, não uma fila com sugestões externas reais. O Windows 0.11.1 passou
a esperar a fila atual antes de preparar essa prévia.*

A descoberta usa compatibilidade de gêneros/ano e novidade local. Não é uma
garantia de músicas inéditas: veja [todas as regras e limites](DESCOBERTA_2_MAIS_1.md).

## 3. Criar playlist colando texto

Abra **Biblioteca → Criar playlist → Criar com Text to Tonalify**.
Cole sua lista, revise as gravações e personalize nome, descrição e capa.
Resultados duvidosos pedem revisão; você pode desmarcar, trocar versão e reordenar.

<table>
<tr><th>Windows: revisar gravações</th><th>Android: mesma etapa nativa</th></tr>
<tr><td><img src="screenshots/desktop-texto-revisao.png" width="600" alt="Windows: revisão de Seether e Shove, com resultado duvidoso desmarcado"></td><td><img src="screenshots/android-texto-revisao.png" width="280" alt="Android: revisão de gravações com opções de versão, edição e ordem"></td></tr>
</table>

*Suítes QML/Android de Text to Tonalify, implementado na 0.9. Catálogos de
teste controlados; os PNGs desktop não registram um identificador do build.*

Salvar primeiro cria a playlist **neste aparelho**. Só depois você escolhe
copiar para Spotify, privada ou pública. Colar/buscar/salvar local não publica nada.

![Windows: playlist local pronta e cópia opcional](screenshots/desktop-texto-salvo.png)

*Captura QML com playlist sintética. Essa tela não comprova envio na conta real.*

[Passo a passo completo, permissões e retomada de envio](TEXT_TO_TONALIFY_090.md).

## 4. Letras, tradução e apresentação

Abra **Letras** ou a seção Letra na página da música. Mantendo-a aberta, a
busca acompanha a troca de faixa. A tradução pt-BR usa idioma detectado
automaticamente e aparece junto das linhas originais. Não é necessário sair
do player. Falta de letra/tradução é informada, sem inventar conteúdo.

![Windows: letra e tradução com cenário de estrelas](screenshots/desktop-letras.png)

*Tela QML real, letra/tradução sintéticas. Não é uma medição do serviço externo.
O estilo de fontes/controles corresponde ao ambiente da suíte.*

<img src="screenshots/android-letras-controles.png" width="320" alt="Android: atualizar, personalizar, expandir, traduzir, sincronia e exportar ajuste">

*Android 0.11.0, letra sintética. Ícones: seta circular atualiza; T personaliza;
cantos expandem; símbolo de idiomas traduz; seta para baixo exporta o ajuste.*

- Desmarque **Sincronia** para desativar o destaque que acompanha a música.
- Toque na linha que está ouvindo para alinhar a letra, sem buscar outra posição do áudio.
- Em tela cheia, **primeiro toque mostra os controles**. Com eles visíveis,
  tocar numa linha alinha e os recolhe. Rolagem não deve alinhar.
- Personalize fontes, cores, tamanho e estilos da letra e da tradução
  separadamente. Há contorno, brilho/neon, piscante, negrito, itálico e sublinhado.
- Cenários: Aurora, Ondas, Espectro, Capa, Foto, Estrelas, Chuva, Órbitas,
  Pulso, Grade e Sem animação. Ondas/Espectro usam PCM; os outros são decorativos.

Busca e tradução dependem dos provedores. Exportar o ajuste gera um arquivo
local, sem enviar automaticamente um feedback ao LRCLIB. Animação tem custo
de CPU/GPU; **Sem animação** é a opção mais econômica.

## 5. Estatísticas e clima do repertório

Abra o gráfico de **Estatísticas**, escolha período e dimensão. Toque numa
barra para detalhar tempo ouvido, escutas qualificadas, músicas distintas e
álbuns. Nas páginas de artista/álbum, o ícone de gráfico mostra/oculta resumos.

<table>
<tr><th>Artista com estatísticas nas faixas</th><th>Histórico por período</th></tr>
<tr><td><img src="screenshots/android-artista.png" width="300" alt="Android: artista Veruca Salt com tempo e contagem nas músicas"></td><td><img src="screenshots/android-estatisticas.png" width="300" alt="Android: estatísticas por artista com barra clicável e card do repertório"></td></tr>
</table>

*Android 0.8.0. Contagens pertencem às sessões de validação; não são totais
completos da conta Spotify. O cabeçalho com texto é anterior aos ícones atuais.*

Toque no card **Clima do repertório** para temas, cobertura, artistas,
álbuns e estilos. É uma interpretação experimental das músicas, não um
diagnóstico do seu estado emocional. Com pouca evidência, mostra a limitação.

<table>
<tr><th>Windows: temas e cobertura</th><th>Android: detalhes e estilos</th></tr>
<tr><td><img src="screenshots/desktop-clima.png" width="520" alt="Windows: cards do repertório, tema rebeldia e cobertura"></td><td><img src="screenshots/android-clima.png" width="300" alt="Android: repertório com cobertura insuficiente, artista, álbum e estilos"></td></tr>
</table>

*Windows: dados/escores sintéticos da suíte QML. Android: sessão limitada 0.8.0.
Nenhuma captura estabelece precisão temática em letras reais.*

## 6. Raridades no IndieTonal e áudio no Android

No IndieTonal, pesquise artista e selecione a fonte. Shows, demos e gravações
dependem do que o acervo disponibiliza. O resultado informa a procedência;
abra a coleção para ver faixas públicas. MusicBrainz é discografia, sem áudio.

No Tonalify/IndieTonal, abra **Áudio/efeitos** para equalização e saída. No
Android, escolha perfil do dispositivo e buffer. Comece com Carro/Original/Flat
para comparar volume e dinâmica sem HRTF ou compressor.

<table>
<tr><th>IndieTonal: show raro de L7</th><th>Perfis de saída e buffer</th></tr>
<tr><td><img src="screenshots/android-raridades.png" width="300" alt="IndieTonal Android: L7 Live at Dreamerz, acervo Aadam Jacobs"></td><td><img src="screenshots/android-audio.png" width="300" alt="Android: painel de áudio com saída, perfil e buffer adaptativo"></td></tr>
</table>

*Capturas históricas 0.5.0 e 0.3.0. A disponibilidade do show não foi reconsultada
para este guia. Não há carro/fone físico conectado na captura de áudio.*

DSP e buffer do app não controlam codec/atraso interno do receptor Bluetooth.
Na 0.11.1, retomar por Bluetooth/tela bloqueada após pausa exige habilitar a
opção correspondente em Áudio; áudio já iniciado continua ao bloquear a tela.

## 7. Guarde seus dados

Use **Backup → Exportar** e salve o arquivo fora dos dados internos do app.
Para restaurar/trocar de aparelho, use Importar. Desinstalar pode apagar o
banco local. Conta Spotify não é backup das estatísticas e preferências locais.
Os backups não levam senha/token nem os áudios das músicas.

O [backup privado de entrega](https://drive.google.com/drive/folders/1sNjL35fdHzoLY8QcqtBX2XF4YQJbeKlL)
também guarda fontes, histórico Git, instaláveis e documentos. O banco do
telefone físico precisa ser exportado no próprio telefone.

Original Tonal: **Matheus Nunes (natheus)**,
[projeto original](https://github.com/natheus/tonal-player). Imagens de catálogo
pertencem aos respectivos titulares; os PNGs preservam as telas registradas,
sem edição do conteúdo para simular funcionalidades.

## 8. Descobertas preservadas e análise de letras — 0.12.0

Na Fila, Shake e Aleatório preservam as músicas azuis marcadas como Descoberta.
Reabrir a janela mantém o estado confirmado. Apenas desligar Descoberta remove
as próximas externas na prévia; Aplicar confirma. A música atual continua.
A caixa de análise permite interromper a busca de letras das candidatas; os
resultados refinam preparações futuras, sem alterar a fila já confirmada.

<img src="screenshots/android-fila-preservada-0120.png" width="300" alt="Android: descobertas preservadas após Shake e Aleatório">
<img src="screenshots/android-descoberta-desligada-0120.png" width="300" alt="Android: próximas descobertas removidas somente após desligar o modo">

*Android 0.12.0: APK benchmark em AVD, nomes sintéticos. Prévia sem iniciar
reprodução ou escrever em playlists reais. [Relatório dos testes](discovery-enrichment-0120-tests.json).*

## 9. Temas, barra de progresso e marcador — 0.13.0

No PC, abra o menu da conta (avatar) e escolha **Temas**; no Android,
**Conta → Temas**. Cada cartão mostra as cores e a barra com o marcador
daquele tema. O clique aplica na hora e a música continua tocando.

<img src="screenshots/desktop-temas-0130.png" width="650" alt="Windows: seletor com seis temas e prévia da barra de cada um, Neon em uso">

*Windows 0.13.0: suíte QML com álbum sintético; o Neon está em uso e já
tinge a página ao fundo.*

<img src="screenshots/desktop-tema-solar-0130.png" width="650" alt="Windows: página de álbum no tema Pôr do sol, barra coral com marcador em pílula">

*Windows 0.13.0: mesma página sintética no tema Pôr do sol; capa ausente intencional.*

<img src="screenshots/android-temas-0130.png" width="300" alt="Android: lista de temas com Oceano em uso e prévias da barra">
<img src="screenshots/android-musica-neon-0130.png" width="300" alt="Android: página da música no tema Neon, barra rosa e marcador com halo">

*Android 0.13.0: APK benchmark na AVD dedicada, durante o áudio de ensaio
local do próprio APK, sem conta. O PCM seguiu avançando, sem underruns, em
cada troca. [Mudanças e testes](TEMAS_SEEK_FILA_0130.md).*

## 10. Controles junto da barra e repetição — 0.13.1

No PC, a barra inferior e a página da música reúnem **Shake, Aleatório,
anterior, tocar, próxima, Repetir e Descoberta 2+1** (e Fila na página da
música). Os modos aplicam direto na fila atual; a música continua. Repetir
alterna entre desligada, repetir a fila e repetir só esta música (ícone com “1”).

<img src="screenshots/desktop-barra-controles-0131.png" width="760" alt="Windows: barra inferior com Shake, Aleatório, anterior, tocar, próxima, Repetir ativo e Descoberta 2+1">

<img src="screenshots/desktop-musica-controles-0131.png" width="650" alt="Windows: página da música com todos os controles numa linha acima da barra de progresso">

*Windows 0.13.1: suíte QML com dados sintéticos.*

<img src="screenshots/android-controles-repeticao-0131.png" width="300" alt="Android: Aleatório, anterior, tocar, próxima e Repetir numa linha; Shake, Descoberta e Fila abaixo">
<img src="screenshots/android-repetir-faixa-0131.png" width="300" alt="Android: Repetir esta música ativo, com o número 1 no ícone">

*Android 0.13.1: APK benchmark na AVD dedicada, durante o áudio de ensaio
local, sem conta. [O que mudou](REPETICAO_MODOS_0131.md).*

## 11. Notificação com capa e ícone no tema — 0.13.2

A notificação e o player do sistema mostram capa, música e banda, com a
posição real. O ícone do Tonal Hub na tela inicial e o da notificação seguem
a cor do tema; no PC, o ícone da janela e da barra de tarefas também.

<img src="screenshots/android-notificacao-capa-0132.png" width="300" alt="Android: player do sistema com capa da gravação, música intro & tunings e banda L7">
<img src="screenshots/android-notificacao-tonalify-0132.png" width="300" alt="Android: player do sistema com o título do ensaio local do Tonalify">

*Android 0.13.2: APK benchmark na AVD dedicada, sem conta; à esquerda, uma
gravação pública do acervo Aadam Jacobs no IndieTonal. [Detalhes](NOTIFICACAO_ICONE_0132.md).*

## 12. Música, letra e compartilhar — 0.14.0

Toque no miniplayer para abrir **Música**. Deslize para baixo para **Letra**;
ao terminar a leitura, outro gesto no limite da rolagem abre **Artista**.
Os três atalhos abaixo do cabeçalho fazem o mesmo percurso diretamente.
Cada seção ocupa a área disponível, sem recortes das seções vizinhas.

<img src="screenshots/android-musica-0140.png" width="280" alt="Android 0.14: música e controles inteiros numa seção, com ícone compartilhar junto ao coração">
<img src="screenshots/android-letra-0140.png" width="280" alt="Android 0.14: seção Letra com sua própria área de leitura e tradução">

O ícone **Compartilhar** abre a personalização: cor, estilo sólido/degradê/papel
e capa opcional. No Android, Enviar link e Compartilhar cartão + link abrem o
seletor do sistema; você escolhe o destinatário. No PC, copie o link e salve PNG.

<img src="screenshots/android-compartilhar-0140.png" width="280" alt="Android: prévia do cartão com opções de cor, estilo, capa, copiar e compartilhar">
<img src="screenshots/desktop-compartilhar-0140.png" width="210" alt="Cartão de compartilhamento no componente Windows, com degradê e dados sintéticos">

No Android, **Biblioteca → Criar playlist → Criar com setlist.fm…** abre a busca
de shows. Informe artista e chave própria; marque shows, confira as músicas e
avance para a revisão Text to Tonalify antes de salvar. Covers continuam sendo
buscados como versões da banda do show. A cópia no Spotify é uma escolha posterior.

<img src="screenshots/android-setlists-0140.png" width="280" alt="Android: criar playlist de shows com chave própria do setlist.fm e revisão posterior">
<img src="screenshots/desktop-musica-0140.png" width="650" alt="Windows: seção Música completa, com compartilhar, capa e controles">

*Capturas 0.14.0: componentes Qt/QML de produção num harness isolado e APK
benchmark na AVD dedicada. Dados de músicas, letras e traduções são sintéticos;
a capa ausente é intencional. Não demonstram reprodução Spotify real, busca
autenticada setlist.fm ou teste no telefone físico. Não houve envio a terceiros.
Procedência e hashes no [manifesto](screenshots/manifest.json).
[Funcionamento e limites](SONG_PAGES_SHARE_SETLISTS_0140.md).*
