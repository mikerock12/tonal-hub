# Instalador Windows — 0.5.0, correção 1

O instalador `Tonal-Hub-0.5.0.1-Windows-x64-Setup.exe` inclui **Tonal Hub**,
**Tonalify** e **IndieTonal**, os runtimes, plugins Qt, motor de áudio e licenças.
A versão de empacotamento 0.5.0.1 identifica a correção de acesso à tradução
na base 0.5.0. O APK Android 0.5.0 permanece separado desta entrega Windows.

Instale executando o Setup. A instalação é por usuário, normalmente em
`%LOCALAPPDATA%\Programs\Tonal Hub`, sem exigir administrador. Há atalhos dos
três programas no menu Iniciar e um atalho opcional do Hub na área de trabalho.
Atualizações do instalador usam um AppId estável próprio do Hub. A desinstalação
fica disponível em Aplicativos do Windows.

Feche os players antigos antes de instalar. Abra **Tonal Hub** pelo novo atalho.
Pastas portáteis antigas continuam existindo; atalhos que apontam para elas
abrem a versão antiga. O instalador não remove essas pastas.

Na tela de letras do Tonalify (microfone na barra inferior), clique em
**Traduzir pt-BR** e escolha o idioma original, inicialmente Inglês. A letra
que já está na tela é reutilizada, com a tradução abaixo de cada verso.
**Mostrar tradução** permite ocultar/exibir o português. Trocar de música ou de
versão da letra impede exibir uma tradução antiga. O acompanhamento e o clique
para buscar uma linha sincronizada são preservados.

A tradução é solicitada explicitamente, envia a letra ao Google Tradutor e pode
ficar parcial ou indisponível. O app informa o estado na própria tela.

As sessões e bibliotecas continuam nos diretórios de dados do usuário, separados
dos executáveis. O instalador não inclui credenciais e não apaga os dados pessoais
ao desinstalar. Faça também os backups de biblioteca pelo próprio app.

## Build local

```powershell
.\packaging\build-local.ps1
.\packaging\package-hub-local.ps1 -Zip -PackageName Tonal-Hub-0.5.0-letras-local
.\packaging\build-hub-installer.ps1
```

Requer Qt 6.8+ MSVC 2022, as dependências de áudio já configuradas e Inno Setup 6.
Saída em `out/release-local/`, junto do arquivo `.sha256`.

Créditos ao projeto original de **Matheus Nunes (natheus)**:
https://github.com/natheus/tonal-player. Licença MIT e avisos das dependências
acompanham a instalação.
