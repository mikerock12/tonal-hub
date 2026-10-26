# Instalador Windows

## Atualização 0.15.1

Instalador atual: `Tonal-Hub-0.15.1.0-Windows-x64-Setup.exe`, em
`out/release-local/`. Contém Tonalify, IndieTonal e Hub, Qt, motor de áudio,
auxiliares nativos opcionais TonalStories e TonalJam, e licenças. Pesos do leitor
não estão embutidos: o download de cerca de 1,2 GiB é opcional e verificado pelo app.

Instale por usuário, sem administrador, e feche a versão antiga antes de atualizar.
A atualização mantém o AppId e os dados pessoais fora da pasta dos executáveis.
Não use desinstalação/limpeza de dados como etapa de atualização. Exporte um
backup pelo app antes de alterar uma instalação com dados importantes.

Em letras em tela cheia, toque uma vez para revelar os controles e use a seta
circular **Atualizar letra**. O Jam e o HRTF para caixas são experimentais; o
leitor pode mostrar faixas inconclusivas. Consulte HOME_QUEUE_AUDIO_0151.md,
LOCAL_STORIES_QUALITY_0151.md e JAM_RESEARCH.md incluídos no pacote.

## Histórico 0.8.0

O instalador `Tonal-Hub-0.8.0.0-Windows-x64-Setup.exe` inclui **Tonal Hub**,
**Tonalify** e **IndieTonal**, os runtimes, plugins Qt, motor de áudio e licenças.
A versão de empacotamento 0.8.0.0 corresponde à base 0.8.0. O APK Android
0.8.0 permanece separado desta entrega Windows. Veja também PLAYER_POLISH_080.md.

Instale executando o Setup. A instalação é por usuário, normalmente em
`%LOCALAPPDATA%\Programs\Tonal Hub`, sem exigir administrador. Há atalhos dos
três programas no menu Iniciar e um atalho opcional do Hub na área de trabalho.
Atualizações do instalador usam um AppId estável próprio do Hub. A desinstalação
fica disponível em Aplicativos do Windows.

Feche os players antigos antes de instalar. Abra **Tonal Hub** pelo novo atalho.
Pastas portáteis antigas continuam existindo; atalhos que apontam para elas
abrem a versão antiga. O instalador não remove essas pastas.

Na tela de letras do Tonalify (microfone na barra inferior), clique em
**Traduzir pt-BR**. O idioma original é detectado automaticamente. A letra
que já está na tela é reutilizada, com a tradução abaixo de cada verso.
**Mostrar tradução** permite ocultar/exibir o português. Trocar de música ou de
versão da letra impede exibir uma tradução antiga. O acompanhamento e o clique
para alinhar uma linha ao áudio são preservados. O clique não pula a música.
Desmarque **Sincronia** para ler e rolar livremente.

A tradução usa Google Tradutor e pode ficar parcial ou indisponível. A opção
automática busca a letra/tradução ao trocar de faixa com a página aberta.
Falhas isoladas não interrompem os versos seguintes; use o botão de tradução
para retomar pendências. O app informa o estado na própria tela.

As sessões e bibliotecas continuam nos diretórios de dados do usuário, separados
dos executáveis. O instalador não inclui credenciais e não apaga os dados pessoais
ao desinstalar. Faça também os backups de biblioteca pelo próprio app.

## Build local

```powershell
.\packaging\build-local.ps1
.\packaging\package-hub-local.ps1 -Zip
.\packaging\build-hub-installer.ps1
```

Requer Qt 6.8+ MSVC 2022, as dependências de áudio já configuradas e Inno Setup 6.
Saída em `out/release-local/`, junto do arquivo `.sha256`.

Créditos ao projeto original de **Matheus Nunes (natheus)**:
https://github.com/natheus/tonal-player. Licença MIT e avisos das dependências
acompanham a instalação.
