# Gera a release do Tonal para Windows: build Release limpo, pasta com tudo o que o app precisa
# (Qt, runtime do Visual C++, librespot e licenças), ZIP portátil, instalador (Inno Setup) e SHA256SUMS.
# Com -Publish, cria a release v<versão> no GitHub com esses arquivos (a versão vem do CMakeLists.txt).
#
# Uso, num PowerShell comum (o script carrega o ambiente do Visual Studio sozinho):
#   .\packaging\release.ps1             # só gera, em out\release\
#   .\packaging\release.ps1 -Publish    # gera e publica no GitHub
#
# Precisa de: Qt (QTDIR), Visual Studio com C++, librespot.exe (cargo install librespot --locked),
# Inno Setup 6 (winget install JRSoftware.InnoSetup) e, para -Publish, o GitHub CLI logado.
param(
    [switch]$Publish,
    [string]$Librespot = "$env:USERPROFILE\.cargo\bin\librespot.exe"
)
$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(Tonal VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$tag = "v$version"
$name = "Tonal-$version-win64"
$out = Join-Path $root "out\release"
$build = Join-Path $out "build"
$stage = Join-Path $out $name
$zip = Join-Path $out "$name.zip"
$setup = Join-Path $out "Tonal-$version-setup.exe"

function Step($text) { Write-Host "==> $text" -ForegroundColor Cyan }
function Check($what) { if ($LASTEXITCODE -ne 0) { throw "$what falhou (código $LASTEXITCODE)" } }

# ------------------------------------------------------------------ pré-condições
if (-not $env:QTDIR) { $env:QTDIR = [Environment]::GetEnvironmentVariable("QTDIR", "User") }
$windeployqt = Join-Path $env:QTDIR "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "QTDIR ($env:QTDIR) não aponta para um kit do Qt: falta windeployqt.exe." }
if (-not (Test-Path (Join-Path $env:QTDIR "plugins\imageformats\qwebp.dll"))) {
    throw "Falta o módulo Qt Image Formats (capas WebP). Com o aqt: aqt install-qt ... -m qtimageformats"
}
if (-not (Test-Path $Librespot)) { throw "librespot.exe não encontrado em $Librespot (cargo install librespot --locked)." }
$iscc = @("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $iscc) { throw "Inno Setup 6 não encontrado (winget install JRSoftware.InnoSetup)." }
if ($Publish) {
    # Um terminal aberto antes de instalar o GitHub CLI ainda não tem o gh no PATH.
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { $env:Path += ";$env:ProgramFiles\GitHub CLI" }
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw "GitHub CLI não encontrado (winget install GitHub.cli)." }
    if (git -C $root status --porcelain) { throw "Há mudanças não commitadas: a release precisa sair de um commit." }
    $head = git -C $root rev-parse HEAD
    git -C $root fetch -q origin
    if (-not (git -C $root branch -r --contains $head)) { throw "O commit atual ainda não foi enviado (git push)." }
    gh release view $tag 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) { throw "A release $tag já existe. Suba a versão no CMakeLists.txt." }
}

# ------------------------------------------------------------------ ambiente do MSVC
if (-not $env:VCToolsRedistDir) {
    Step "Carregando o ambiente do Visual Studio"
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vs) { throw "Visual Studio com o workload de C++ não encontrado." }
    # O VsDevCmd chama o vswhere pelo PATH; sem isso ele imprime "vswhere.exe não é reconhecido".
    $env:Path += ";" + (Split-Path $vswhere)
    & "$vs\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
    if (-not $env:VCToolsRedistDir) { throw "Não consegui carregar o ambiente do Visual Studio ($vs)." }
}
# Num console UTF-8 o cl.exe em português escreve "Observação: incluindo arquivo" em UTF-8, e o Ninja
# (que espera CP850) perde o rastreio de cabeçalhos: mudar um .h deixa de recompilar quem o inclui.
chcp.com 850 | Out-Null

# ------------------------------------------------------------------ build Release limpo
Step "Compilando o Tonal $version (Release)"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$env:QTDIR" -DTONAL_AUTO_DEPLOY=OFF
Check "cmake (configuração)"
cmake --build $build
Check "cmake --build"

# ------------------------------------------------------------------ pasta do app
Step "Montando $name"
New-Item -ItemType Directory $stage | Out-Null
Copy-Item (Join-Path $build "Tonal.exe") $stage
# --no-compiler-runtime: o runtime vai como DLLs logo abaixo, não como o instalador vc_redist (18 MB).
# --no-system-dxc-compiler: o compilador de shaders do Direct3D 12 (12 MB); o Qt Quick usa D3D11.
& $windeployqt --release --qmldir (Join-Path $root "qml") --no-translations --no-system-d3d-compiler `
    --no-system-dxc-compiler --no-opengl-sw --no-compiler-runtime --dir $stage (Join-Path $stage "Tonal.exe") | Out-Null
Check "windeployqt"
# O Tonal fixa o estilo Basic (main.cpp); os outros estilos do Qt Quick Controls nunca carregam.
foreach ($style in "FluentWinUI3", "Fusion", "Imagine", "Material", "Universal", "Windows") {
    Remove-Item (Join-Path $stage "qml\QtQuick\Controls\$style") -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $stage "Qt6QuickControls2$style*.dll") -Force -ErrorAction SilentlyContinue
}
# Runtime do Visual C++ junto do app (o Tonal e o librespot usam): roda sem instalar o vc_redist.
$crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
if (-not $crt) { throw "Runtime do Visual C++ não encontrado em $env:VCToolsRedistDir" }
Copy-Item (Join-Path $crt.FullName "*.dll") $stage
# O Tonal procura o librespot primeiro ao lado do Tonal.exe.
Copy-Item $Librespot $stage
Copy-Item (Join-Path $PSScriptRoot "licenses") (Join-Path $stage "licenses") -Recurse

# ------------------------------------------------------------------ ZIP, instalador, hashes
Step "Gerando o ZIP portátil"
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal

Step "Gerando o instalador"
& $iscc /Q "/DAppVersion=$version" "/DSourceDir=$stage" "/DOutputDir=$out" "/DRepoRoot=$root" (Join-Path $PSScriptRoot "tonal.iss")
Check "Inno Setup"

$sums = Join-Path $out "SHA256SUMS.txt"
$zip, $setup | ForEach-Object { "{0}  {1}" -f (Get-FileHash $_ -Algorithm SHA256).Hash.ToLower(), (Split-Path $_ -Leaf) } |
    Set-Content $sums -Encoding Ascii

Write-Host ""
Get-Item $zip, $setup | ForEach-Object { "{0,-32} {1,7:N1} MB" -f $_.Name, ($_.Length / 1MB) }

# ------------------------------------------------------------------ publicar
if ($Publish) {
    Step "Publicando a release $tag no GitHub"
    $librespotVersion = ((& $Librespot --version) -split ' ')[1]
    $qtVersion = & (Join-Path $env:QTDIR "bin\qtpaths.exe") --qt-query QT_VERSION
    $notes = (Get-Content (Join-Path $PSScriptRoot "release-notes.md") -Raw -Encoding UTF8).
        Replace("{VERSION}", $version).Replace("{LIBRESPOT}", $librespotVersion).Replace("{QT}", $qtVersion)
    $notesFile = Join-Path $out "notes.md"
    [IO.File]::WriteAllText($notesFile, $notes, (New-Object Text.UTF8Encoding $false))
    Push-Location $root
    try {
        gh release create $tag $setup $zip $sums --target $head --title "Tonal $version" --notes-file $notesFile
        Check "gh release create"
        git fetch -q --tags origin
    } finally { Pop-Location }
}
