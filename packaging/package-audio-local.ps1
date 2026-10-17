param([switch]$Zip)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'out\build\release'
if(-not (Test-Path -LiteralPath (Join-Path $build 'Tonal.exe'))){throw 'Execute packaging/build-local.ps1 -Test primeiro.'}
$version=(Select-String -LiteralPath (Join-Path $root 'CMakeLists.txt') -Pattern 'project\(Tonal VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$stage=Join-Path $root "out\release-local\Tonal-$version-audio-local"
New-Item -ItemType Directory -Force $stage | Out-Null
Get-ChildItem -LiteralPath $build -File | Where-Object {($_.Extension -eq '.dll' -and $_.Name -notmatch '^avdevice-') -or $_.Name -in @('Tonal.exe','librespot.exe')} | Copy-Item -Destination $stage -Force
foreach($name in @('qml','platforms','imageformats','iconengines','networkinformation','tls','generic')){
    $source=Join-Path $build $name
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $stage -Recurse -Force}
}
@'
[Paths]
Prefix=.
Plugins=.
QmlImports=qml
Binaries=.
'@ | Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding ascii
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses') -Destination $stage -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'licenses\Tonal-LICENSE.txt') -Force
& (Join-Path $PSScriptRoot 'collect-audio-notices.ps1') -Destination (Join-Path $stage 'licenses\audio')
Copy-Item -LiteralPath (Join-Path $root 'docs\AUDIO_LOCAL.md') -Destination $stage -Force
if($Zip){Compress-Archive -Path (Join-Path $stage '*') -DestinationPath ($stage+'.zip') -Force}
Write-Host "Pacote de teste LOCAL: $stage"
