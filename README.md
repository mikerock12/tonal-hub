# Tonal

Player leve para Spotify (Premium) feito em **C++ / Qt 6 (QML)**, com o áudio tocado pelo
**[librespot](https://github.com/librespot-org/librespot)**. Nada de Chromium embutido.

```
┌──────────────────────── Tonal.exe (Qt Quick) ────────────────────────┐
│  QML (interface)  ──►  C++ (App, Player, SpotifyApi, AuthManager)    │
│                              │ Web API (HTTPS)                       │
│                              ▼                                       │
│                     api.spotify.com  ◄── comandos: play, pause...    │
└──────────────────────────────┬───────────────────────────────────────┘
                               │ inicia e vigia o processo
                     librespot.exe  ──►  áudio (dispositivo "Tonal @ PC")
```

O Tonal fala com a Web API oficial (biblioteca, busca, controle). O librespot aparece como um
dispositivo Spotify Connect e é quem baixa e toca a música. O Tonal manda tocar nele.

## Instalar (sem compilar)

Baixe a versão mais recente em **[Releases](https://github.com/natheus/tonal-player/releases)**:

- `Tonal-<versão>-setup.exe`: instalador. Não precisa de administrador e cria atalho no Menu Iniciar.
- `Tonal-<versão>-win64.zip`: versão portátil. Descompacte e abra o `Tonal.exe`.

Os dois já trazem o Qt, o runtime do Visual C++ e o librespot. Precisa de Windows 10 (1809) ou 11, 64 bits, e
de uma conta Spotify Premium. O executável não é assinado: se o SmartScreen avisar, clique em
**Mais informações → Executar assim mesmo**. Na primeira vez, o Tonal guia a criação do app no painel do Spotify.

O resto deste arquivo é para compilar a partir do código.

## 1. O que instalar (uma vez)

| O quê | Onde | Observação |
|---|---|---|
| Visual Studio 2022 ou 2026 | já tem | workload **Desktop development with C++** (traz CMake e Ninja) |
| Qt 6.8 ou mais novo | [Qt Online Installer](https://www.qt.io/download-qt-installer-oss) | marque **MSVC 2022 64-bit** na versão escolhida e, em *Additional Libraries*, **Qt Image Formats** (as capas de playlist vêm em WebP) |
| Rust | [rustup.rs](https://rustup.rs) | só para compilar o librespot |

Depois de instalar o Qt, crie a variável de ambiente `QTDIR` apontando para o kit, por exemplo:

```powershell
setx QTDIR "C:\Qt\6.8.3\msvc2022_64"
```

(feche e abra o Visual Studio depois do `setx`)

## 2. Compilar o librespot

No PowerShell:

```powershell
cargo install librespot --locked
```

O executável fica em `%USERPROFILE%\.cargo\bin\librespot.exe`. O Tonal procura nesta ordem:
ao lado do `Tonal.exe`, no `PATH` e em `.cargo\bin`. Não precisa copiar nada.

## 3. Compilar o Tonal

**Pelo Visual Studio:** *Arquivo → Abrir → Pasta...* e escolha esta pasta. Na barra de cima escolha
o preset **Release (MSVC + Qt)**, depois *Compilar → Compilar Tudo* e rode `Tonal.exe`.
O `windeployqt` roda sozinho depois do build e copia as DLLs do Qt para a pasta do executável.

**Pela linha de comando** (Developer PowerShell do Visual Studio):

```powershell
cmake --preset release
cmake --build --preset release
.\out\build\release\Tonal.exe
```

## 4. Criar o app no Spotify (uma vez)

A tela de login do Tonal guia tudo, com botões para abrir o formulário e copiar cada campo:

1. Abra <https://developer.spotify.com/dashboard/create> (o formulário **Create app**).
2. Preencha nome e descrição, adicione em **Redirect URIs** exatamente `http://127.0.0.1:8898/callback`
   (clique em **Add**), marque **Web API**, aceite os termos e clique em **Save**.
3. Copie o **Client ID** do app. Ao voltar para o Tonal ele é colado sozinho e conferido com o Spotify
   antes de abrir o navegador (o Client Secret não serve e não é usado).

No primeiro uso abrem **duas** autorizações no navegador: a do Tonal (Web API) e a do librespot
(o motor de áudio). Depois disso fica tudo salvo e o app abre direto.

## 5. Gerar uma release

Com o Inno Setup 6 instalado (`winget install JRSoftware.InnoSetup`), num PowerShell comum:

```powershell
.\packaging\release.ps1             # gera em out\release\: ZIP, instalador e SHA256SUMS.txt
.\packaging\release.ps1 -Publish    # gera e publica a release v<versão> no GitHub (precisa do gh logado)
```

A versão vem do `project(Tonal VERSION ...)` do `CMakeLists.txt`: suba-a antes de cada release nova.

## O que já funciona

- Login OAuth PKCE (sem client secret), renovação automática do token
- Início: atalhos, tocadas recentemente, artistas e músicas mais ouvidas, suas playlists, álbuns salvos
- Busca com melhor resultado, músicas, artistas, álbuns, playlists, filtros, "carregar mais" e buscas recentes
- Páginas de playlist, álbum, Músicas Curtidas (com paginação infinita) e artista (populares + discografia)
- Sua Biblioteca com filtros, busca, redimensionável e recolhível (arraste a borda)
- Player: play/pause, anterior/próxima, aleatório, repetir (lista/faixa), barra de progresso, volume, curtir
- Fila e tela "Tocando agora" no painel direito
- Seletor de dispositivo (Spotify Connect): manda a música para o celular, outro PC etc.
- Degradê do cabeçalho tirado da cor da capa, barra fixa ao rolar, cards com play no hover
- Atalhos: `Espaço` play/pause · `Ctrl+→/←` próxima/anterior · `Ctrl+↑/↓` volume · `Ctrl+L` busca · `Alt+←/→` voltar/avançar · botões laterais do mouse
- Qualidade 96/160/320 kbps e normalização de volume (menu da conta)

## Por que é leve

- Interface em Qt Quick renderizada pela GPU, sem motor de navegador
- Estilo "Basic" do Qt Quick Controls (o mais simples), visual 100% próprio
- Ícones vetoriais desenhados com `QtQuick.Shapes` (sem PNGs, sem fonte de ícones)
- Capas decodificadas no tamanho exibido (`sourceSize`) e com cache em disco
- As páginas são criadas e destruídas sob demanda (`Loader`); listas reciclam linhas (`reuseItems`)
- O estado do player é consultado a cada 5 s e a barra de progresso é interpolada localmente;
  o log do librespot avisa na hora quando a faixa muda

Para medir: abra o Gerenciador de Tarefas e some `Tonal.exe` + `librespot.exe`.

## Limitações (regras do Spotify, não do app)

Desde fevereiro de 2026 apps em **modo de desenvolvimento** têm restrições:

- só a conta dona do app e até 5 usuários cadastrados no painel podem usar, todos Premium
- **playlists de outras pessoas** (inclusive as editoriais do Spotify) não mostram as faixas pela
  API. O Tonal avisa e deixa tocar mesmo assim
- a busca devolve no máximo 10 itens por tipo (o "Carregar mais" pagina)
- "Novos lançamentos", categorias e "top tracks do artista" foram removidos. Os "Populares" da página
  de artista vêm de uma busca `artist:"Nome"`

## Próximos passos possíveis

- Teclas de mídia do teclado e o controle de mídia do Windows (SMTC)
- Letras sincronizadas
- Criar/editar playlists e arrastar faixas
- Guardar o token no Gerenciador de Credenciais do Windows em vez do registro

## Estrutura

```
src/
  main.cpp              inicializa app, estilo, fonte e o motor QML
  App.*                 singleton exposto ao QML (App.auth, App.api, App.player, App.librespot)
  AuthManager.*         OAuth PKCE + servidor de callback em 127.0.0.1:8898
  SpotifyApi.*          cliente HTTP da Web API (401 → refresh, 429 → Retry-After)
  Player.*              estado de reprodução, comandos, fila, dispositivos
  LibrespotProcess.*    inicia/reinicia o librespot, lê o log, Job Object no Windows
  NetworkFactory.*      cache em disco para as capas
qml/
  Main.qml              janela, layout, navegação, atalhos, menu de faixa, toast
  Theme.qml             cores (azul, preto e branco), medidas e funções de formatação
  Icons.qml             paths SVG dos ícones
  Nav.qml               histórico de navegação e estado compartilhado
  components/           Icon, Cover, MediaCard, TrackRow, SeekBar, Chip...
  panels/               TopBar, LibraryPanel, PlayerBar, RightPanel, DevicePopup
  pages/                Login, Home, Search, Collection (playlist/álbum/curtidas), Artist
resources/
  tonal.ico             ícone do app (gerado por make-icon.ps1, ligado ao .exe pelo tonal.rc)
packaging/
  release.ps1           gera o ZIP portátil e o instalador (e publica a release com -Publish)
  tonal.iss             instalador (Inno Setup 6)
  licenses/             licenças do Qt e do librespot, que vão junto na release
```

O Tonal é um cliente pessoal e não oficial. "Spotify" é marca da Spotify AB; este projeto não tem
relação com a empresa.
