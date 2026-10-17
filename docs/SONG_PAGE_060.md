# Página da música e interface — 0.6.0

No Windows Tonalify, clique na capa ou no nome da música na barra inferior.
No IndieTonal, clique no nome/capa do player. No Android, toque no miniplayer
ou no nome da música. A página acompanha a faixa atual.

A página reúne capa, artista, álbum, controles, coração e letras. No Android,
os cartões de letras e informações da gravação ficam abaixo dos controles;
**Letras** abre a leitura ampliada. As capas dependem dos metadados da fonte;
há uma imagem de reserva quando a gravação não tem capa.

Enquanto a página de letras estiver aberta, a busca é automática a cada troca
de faixa. A tradução para pt-BR detecta o idioma original automaticamente e fica
abaixo de cada verso. No Android, **Tradução automática** também permite ocultar
o português. No Tonalify Windows, **Mostrar tradução** oculta/exibe as linhas
traduzidas. Não é preciso selecionar Inglês, Espanhol etc. a cada música.

A letra vem do LRCLIB e a tradução do Google Tradutor. Abrir a página envia
título/artista ao serviço de letras; a tradução automática envia a letra ao
serviço de tradução. Fechar a página interrompe a busca automática de novas
faixas. Os serviços podem falhar ou não ter a versão exata. Erros temporários
do serviço de letras recebem até duas novas tentativas; não há repetição infinita.
Letras sem tempos usam rolagem manual. Letras LRC acompanham a posição da música.

As estatísticas foram reorganizadas em resumo de minutos, ranking interativo,
clima do repertório e sugestões. O Android mostra as primeiras 20 entradas no
gráfico. Toque numa barra para ver as músicas. O histórico existente é mantido;
a leitura do repertório é lúdica, com cobertura explícita, sem diagnóstico pessoal.

O coração fica junto da música no miniplayer dos dois apps. Tonalify usa as
curtidas da conta Spotify quando autenticado e confirma a alteração pela API.
IndieTonal e arquivos locais usam os favoritos pessoais. Playlists e backups
continuam disponíveis em **Sua biblioteca** / **Biblioteca pessoal**.

## Arquivos

- Windows: `Tonal-Hub-0.6.0.0-Windows-x64-Setup.exe` (inclui os três programas).
- Windows portátil: `Tonal-Hub-0.6.0-local.zip`.
- POCO X7 Pro: `Tonal-Android-0.6.0-arm64-v8a.apk`.
- Android universal/emulador: `Tonal-Android-0.6.0-local.apk`.

Instale o APK por cima da versão anterior, sem desinstalar. No Windows, feche
players antigos e use os atalhos criados pelo instalador; atalhos de pastas
portáteis antigas continuam abrindo aquelas versões. O banco e as sessões ficam
separados dos executáveis. Exporte também seu backup pessoal pelo próprio app.

Testes e limites desta entrega ficam em `docs/song-page-060-tests.json`.
Esta alteração de interface não constitui novo benchmark contra Spotify,
validação auditiva nem teste físico de Bluetooth/POCO.

Projeto original: **Matheus Nunes (natheus)**,
https://github.com/natheus/tonal-player. Licença MIT e créditos preservados.
