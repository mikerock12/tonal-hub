param([switch]$Test,[switch]$Abrir,[switch]$WithoutIamf)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
function Check([string]$label){if($LASTEXITCODE -ne 0){throw "$label falhou ($LASTEXITCODE)"}}
$tools=Join-Path $projectRoot 'out\tools\python'
$nativeCmake=Join-Path $tools 'Lib\site-packages\cmake\data\bin'
$stableCmake=Join-Path $projectRoot 'out\tools\cmake331\cmake\data\bin'
if(Test-Path -LiteralPath (Join-Path $stableCmake 'cmake.exe')){$nativeCmake=$stableCmake}
$env:Path="$nativeCmake;$(Join-Path $tools 'Scripts');$env:Path"
if(-not $env:QTDIR){$env:QTDIR=[Environment]::GetEnvironmentVariable('QTDIR','User')}
if(-not $env:QTDIR){$env:QTDIR=Join-Path $projectRoot 'out\tools\Qt\6.8.3\msvc2022_64'}
if(-not (Test-Path -LiteralPath (Join-Path $env:QTDIR 'bin\qmake.exe'))){throw 'Configure QTDIR com um kit Qt 6.8+ MSVC x64.'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio com ferramentas C++ nao encontrado.'}
$env:Path+=';'+(Split-Path $vswhere)
& (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
chcp.com 65001 | Out-Null
Push-Location $projectRoot
try {
    & (Join-Path $tools 'Scripts/python.exe') -X utf8 (Join-Path $PSScriptRoot 'repair-msvc-diagnostics.py') (Join-Path $projectRoot 'out/build/release')
    $repair=$LASTEXITCODE
    if($repair -ne 0 -and $repair -ne 10){throw 'Falha ao conferir dependências MSVC'}
    $iamf=if($WithoutIamf){'OFF'}else{'ON'}
    cmake --preset release "-DTONAL_ENABLE_IAMF=$iamf" -DBUILD_TESTING=ON -DALSOFT_ENABLE_MODULES=OFF
    Check 'CMake'
    $rebuild=if($repair -eq 10){@("--clean-first")}else{@()}
    cmake --build --preset release --parallel 4 @rebuild
    Check 'Build'
    & (Join-Path $PSScriptRoot 'build-stories-local.ps1')
    $build=Join-Path $projectRoot 'out\build\release'
    $librespot=Join-Path $env:USERPROFILE '.cargo\bin\librespot.exe'
    if(-not (Test-Path -LiteralPath $librespot)){throw 'Instale librespot: cargo install librespot --locked'}
    Copy-Item -LiteralPath $librespot -Destination $build -Force
    $runtime=Get-ChildItem -LiteralPath (Join-Path $env:VCToolsRedistDir 'x64') -Directory -Filter 'Microsoft.VC*.CRT' | Select-Object -First 1
    Get-ChildItem -LiteralPath $runtime.FullName -Filter '*.dll' | Copy-Item -Destination $build -Force
    & (Join-Path $PSScriptRoot 'collect-audio-notices.ps1') -Destination (Join-Path $build 'licenses\audio')
    if($Test){
        python tests/audio/generate_fixtures.py
        Check 'Fixtures'
        ctest --test-dir $build -R '^audio_' --output-on-failure
        Check 'Testes de audio'
    }
    if($Abrir){Start-Process -FilePath (Join-Path $build 'Tonal.exe') -WorkingDirectory $build}
    Write-Host "Pronto: $build\Tonal.exe"
}finally{Pop-Location}
