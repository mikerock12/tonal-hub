param([switch]$Zip)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=Join-Path $root 'out\build\release'
$stage=Join-Path $root 'out\release-local\Tonal-Hub-0.5.0-local'
foreach($name in @('TonalHub.exe','IndieTonal.exe','Tonal.exe')){
    if(-not (Test-Path -LiteralPath (Join-Path $build $name))){throw "Compile primeiro: falta $name"}
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Get-ChildItem -LiteralPath $build -File | Where-Object { $_.Extension -eq '.dll' -or $_.Name -in @('TonalHub.exe','IndieTonal.exe','librespot.exe') } | Copy-Item -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $build 'Tonal.exe') -Destination (Join-Path $stage 'Tonalify.exe') -Force
foreach($name in @('qml','platforms','imageformats','iconengines','networkinformation','sqldrivers','tls','generic','licenses')){
    $source=Join-Path $build $name
    if(Test-Path -LiteralPath $source){Copy-Item -LiteralPath $source -Destination $stage -Recurse -Force}
}
# Include Qt's offscreen platform only for reproducible local UI verification.
$qtRoot=if($env:QTDIR){$env:QTDIR}else{Join-Path $root 'out\tools\Qt\6.8.3\msvc2022_64'}
$offscreen=Join-Path $qtRoot 'plugins\platforms\qoffscreen.dll'
if(Test-Path -LiteralPath $offscreen){Copy-Item -LiteralPath $offscreen -Destination (Join-Path $stage 'platforms') -Force}
@'
[Paths]
Prefix=.
Plugins=.
QmlImports=qml
Binaries=.
'@ | Set-Content -LiteralPath (Join-Path $stage 'qt.conf') -Encoding ascii
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'licenses') -Destination $stage -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $stage 'licenses\Tonal-LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $root 'CREDITS.md') -Destination $stage -Force
foreach($docName in @('PERSONAL_LIBRARY_050.md','RARE_RECORDINGS_LOCAL.md')){Copy-Item -LiteralPath (Join-Path $root ('docs/'+$docName)) -Destination $stage -Force}
& (Join-Path $PSScriptRoot 'collect-audio-notices.ps1') -Destination (Join-Path $stage 'licenses\audio')
Copy-Item -LiteralPath (Join-Path $root 'docs\TONAL_HUB_LOCAL.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $root 'docs\INDIE_SEARCH_FIX_LOCAL.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $root 'docs\TONALIFY_RECOVERY_LOCAL.md') -Destination $stage -Force
Get-ChildItem -LiteralPath $stage -Filter '*.exe' | ForEach-Object {
    $hash=Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256
    "$($hash.Hash.ToLower())  $($_.Name)"
} | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Encoding ascii
if($Zip){Compress-Archive -Path (Join-Path $stage '*') -DestinationPath ($stage+'.zip') -Force}
Write-Host "Pacote LOCAL: $stage"
