param(
    [string]$SourceDir,
    [string]$Compiler,
    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')][string]$AppVersion='0.15.2.0'
)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $SourceDir){$SourceDir=Join-Path $root 'out/release-local/Tonal-Hub-0.15.2-local'}
$SourceDir=(Resolve-Path -LiteralPath $SourceDir).Path
foreach($file in @('TonalHub.exe','Tonalify.exe','IndieTonal.exe','TonalStories.exe','TonalJam.exe','librespot.exe','qt.conf','platforms/qwindows.dll','sqldrivers/qsqlite.dll','CREDITS.md')){
    if(-not (Test-Path -LiteralPath (Join-Path $SourceDir $file))){throw "Pacote incompleto: falta $file"}
}
if(-not $Compiler){
    $candidates=@((Join-Path $env:LOCALAPPDATA 'Programs/Inno Setup 6/ISCC.exe'),(Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6/ISCC.exe'))
    $Compiler=$candidates | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
}
if(-not $Compiler){throw 'Instale o Inno Setup 6 ou informe -Compiler.'}
$output=Join-Path $root 'out/release-local'
& $Compiler '/Qp' "/DAppVersion=$AppVersion" "/DSourceDir=$SourceDir" "/DOutputDir=$output" "/DRepoRoot=$root" (Join-Path $PSScriptRoot 'tonal-hub.iss')
if($LASTEXITCODE -ne 0){throw "ISCC falhou ($LASTEXITCODE)"}
$setup=Join-Path $output "Tonal-Hub-$AppVersion-Windows-x64-Setup.exe"
$hash=Get-FileHash -LiteralPath $setup -Algorithm SHA256
"$($hash.Hash.ToLower())  $(Split-Path $setup -Leaf)" | Set-Content -LiteralPath ($setup+'.sha256') -Encoding ascii
Write-Host "Instalador pronto: $setup"
