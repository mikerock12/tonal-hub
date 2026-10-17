# Biblioteca pessoal, letras, cenários e backup — 0.5.0

## Onde abrir

- Windows Tonalify: menu da conta → **Biblioteca pessoal, estatísticas e letras…**.
- Windows IndieTonal: **Sua música**.
- Android: ícone de coração no cabeçalho de cada player.

O painel tem **Favoritas e playlists / Biblioteca pessoal**, **Estatísticas** e
**Letras e cenário**. Cada player mantém sua biblioteca local separada. O hub
continua alternando entre os players.

## Favoritos e playlists

IndieTonal mantém favoritos e playlists pessoais neste aparelho. O painel permite
criar, renomear e excluir playlists e adicionar/remover faixas. O identificador e
a plataforma da gravação são preservados: demos, shows e versões de estúdio não
são fundidos por terem o mesmo título.

No Tonalify, favoritar usa as Músicas Curtidas da conta Spotify e só confirma
sucesso após a resposta da API. As playlists da conta continuam na Biblioteca;
o Android também permite criar uma playlist privada na conta a partir do painel.
Playlists pessoais são locais e são identificadas como tais. Permissões de
gravação da conta podem exigir novo login; isso não apaga a sessão do motor.

## Histórico e gráficos

O banco registra o avanço real da reprodução local, com verificação de progresso
PCM no Spotify. Pausas, carregamentos e saltos de posição não contam como tempo
ouvido. Uma escuta é qualificada após 30 segundos ou metade da faixa curta.

Há períodos de 7, 30, 90 e 180 dias, com gráficos de artistas, músicas, estilos e
dias. Clique/toque numa barra para ver e reproduzir as faixas daquele grupo.
O registro pode ser desativado e o histórico pode ser apagado sem excluir likes
e playlists. O histórico começa nesta versão; não se inventam escutas antigas.

Os estilos disponíveis nos provedores são conservados; você pode corrigir os
dados/estilo da gravação em **Editar dados**. Faixas sem gênero aparecem como
**Não informado**, sem adivinhação apresentada como metadado oficial.

O **clima do repertório** é uma brincadeira baseada em palavras das letras e
estilos informados. Mostra cobertura das letras analisadas e requer pelo menos
três escutas qualificadas com evidência lírica. Não interpreta ironia e não é
um diagnóstico do humor ou da saúde de quem escuta. As sugestões são editoriais
por estilo, identificadas como tais; não garantem disponibilidade no catálogo.

## Letras e tradução dentro do app

**Buscar letra** consulta o LRCLIB sob demanda. A busca confere título, artista e
duração para reduzir a associação com versões erradas. Havendo LRC, destaca a
linha atual e acompanha a música; letras simples usam rolagem manual. Shows,
introduções e demos podem não ter letra correspondente. Você pode colar TXT/LRC.

**Traduzir pt-BR** pede o idioma original e usa Google Tradutor sob demanda. Exibe a
tradução abaixo de cada linha, sem abrir navegador. Há alinhamento por linha,
cache de frases repetidas, limite local diário e tratamento de cota/indisponibilidade.
A tradução automática pode conter erros; é possível importar uma tradução sua.

Buscar envia título/artista ao LRCLIB; traduzir envia o texto ao Google Tradutor. O
histórico e a análise do repertório permanecem locais. O app não promete letras
ou traduções para todas as gravações. Letras não são incluídas no banco/backup;
ficam na memória da sessão.

Fundos: aurora animada, ondas, espectro, capa, foto escolhida pelo usuário e sem
animação. A letra fica em primeiro plano com contraste e fonte grande. Ondas e
espectro usam o PCM do player, sem microfone, e são calculados somente enquanto
a visualização está em uso. Fotos grandes são reduzidas antes da exibição.

## SQLite e backup

O SQLite é local. As tabelas incluem `tracks`, `listens`, `favorites`, `playlists`,
`playlist_items` e `settings`. As gravações são incrementais, em transação e fora
da thread da interface, com WAL. O protótipo JSON é lido na migração; a fonte da
migração é preservada. Há limites de 3.000 metadados de faixas, 100 playlists com
até 1.000 faixas cada e até 10.000 eventos/180 dias de histórico.

No Windows, o banco fica em `AppLocalDataLocation/personal/music-library.sqlite`.
No Android, fica em `databases/music-library-<player>-v1.sqlite`, na pasta privada
do pacote. Tonalify e IndieTonal usam arquivos separados para evitar conflito
entre processos. Configurações de áudio existentes continuam nos seus perfis
próprios; tokens e senhas continuam no cofre do sistema/Keystore.

**Backup → Exportar** salva um arquivo `*.tonal-backup.json` numa pasta escolhida
por você. No Android, o seletor de documentos permite Documentos, cartão/armazenamento
disponível e provedores de documentos, como Drive quando instalado.
O arquivo contém biblioteca, histórico, estilos e preferências visuais. Não contém
tokens, senhas, músicas do streaming ou cache. O arquivo é legível em JSON: guarde-o
como um arquivo pessoal.

**Restaurar** valida formato, versão, player e tamanho antes de substituir os
dados locais. Faça um backup antes da substituição. O formato é comum entre
Windows e Android para o mesmo player. Caminhos de arquivos/fotos locais não são
portáveis automaticamente; disponibilize os arquivos no aparelho de destino.
Contas externas podem exigir reconexão. Restaurar um backup local não modifica
retroativamente as playlists/likes já existentes numa conta de streaming.

**Desinstalar o Android normalmente apaga o banco privado.** Salve o backup fora
da pasta do aplicativo antes. Não há sincronização automática em nuvem prometida.
O backup de desenvolvimento no GitHub/Drive é separado desse backup pessoal.

## Créditos e dependências

Projeto original: **Matheus Nunes (natheus)**,
https://github.com/natheus/tonal-player. Consulte `CREDITS.md` e `LICENSE`.
Letras: https://lrclib.net/docs. Tradução: https://translate.google.com/.
A consulta web de tradução não oferece SLA nem garantia de estabilidade de uma
API contratada; a interface trata indisponibilidade e permite tradução importada.

Os resultados executados desta versão são registrados em
`docs/personal-library-050-tests.json` após a validação final. Testes em emulador
não substituem uso no POCO X7 Pro ou em Bluetooth de carro/fones reais.
