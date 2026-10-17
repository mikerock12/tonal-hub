param([switch]$WithoutIamf,[switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$deps=Join-Path $projectRoot 'out\audio-deps'
$lock=Get-Content -LiteralPath (Join-Path $projectRoot 'third_party\versions.lock') -Raw | ConvertFrom-Json
function Check([string]$label){if($LASTEXITCODE -ne 0){throw "$label falhou ($LASTEXITCODE)"}}
function Check-Hash([string]$path,[string]$expected){
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){throw "SHA256 incorreto: $path"}
}
function Ensure-GitSource([string]$path,$source,[switch]$Sparse){
    if(-not (Test-Path -LiteralPath (Join-Path $path '.git'))){
        if($VerifyOnly){throw "Dependencia ausente: $path"}
        git clone --filter=blob:none --no-checkout $source.url $path
        Check 'git clone (somente leitura remota)'
        git -C $path fetch --depth 1 origin $source.commit
        Check 'git fetch da revisao fixada'
        if($Sparse){git -C $path sparse-checkout set code;Check 'sparse checkout'}
        git -C $path checkout --detach $source.commit
        Check 'checkout local da dependencia'
    }
    $actual=git -C $path rev-parse HEAD
    Check 'git rev-parse'
    if($actual -ne $source.commit){throw "Revisao inesperada em $path : $actual"}
}
New-Item -ItemType Directory -Force $deps | Out-Null
$sdk=Join-Path $deps $lock.ffmpeg.sdkDir
if(-not (Test-Path -LiteralPath (Join-Path $sdk 'include\libavfilter\avfilter.h'))){
    if($VerifyOnly){throw 'SDK FFmpeg ausente'}
    $archive=Join-Path $deps 'ffmpeg-pinned.zip'
    if(-not (Test-Path -LiteralPath $archive)){Invoke-WebRequest $lock.ffmpeg.url -OutFile $archive}
    Check-Hash $archive $lock.ffmpeg.sha256
    $extract=Join-Path $deps 'ffmpeg-pinned'
    if(-not (Test-Path -LiteralPath $extract)){Expand-Archive -LiteralPath $archive -DestinationPath $extract}
    $folder=Get-ChildItem -LiteralPath $extract -Directory | Select-Object -First 1
    if(-not $folder){throw 'Arquivo SDK vazio'}
    # Both paths must remain under this project's dependency cache before moving.
    $sourceFull=[IO.Path]::GetFullPath($folder.FullName)
    $destinationFull=[IO.Path]::GetFullPath($sdk)
    $boundary=[IO.Path]::GetFullPath($deps).TrimEnd('\')+'\'
    if(-not $sourceFull.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase) -or -not $destinationFull.StartsWith($boundary,[StringComparison]::OrdinalIgnoreCase)){throw 'Destino fora do cache local'}
    New-Item -ItemType Directory -Force (Split-Path $destinationFull) | Out-Null
    Move-Item -LiteralPath $sourceFull -Destination $destinationFull
}
foreach($property in $lock.runtimeHashes.PSObject.Properties){Check-Hash (Join-Path $sdk "bin\$($property.Name)") $property.Value}
Ensure-GitSource (Join-Path $deps 'openal-soft') $lock.sources.'openal-soft'
if(-not $WithoutIamf){
    $iamf=Join-Path $deps 'libiamf-src'
    Ensure-GitSource $iamf $lock.sources.libiamf -Sparse
    $oar=Join-Path $iamf 'code\dep_external\src\oar'
    if(-not (Test-Path -LiteralPath (Join-Path $oar 'CMakeLists.txt'))){
        if($VerifyOnly){throw 'Submodulo OAR ausente'}
        git -C $iamf submodule update --init --depth 1 code/dep_external/src/oar
        Check 'submodulo OAR (download)'
    }
    if((git -C $oar rev-parse HEAD) -ne $lock.sources.oar.commit){throw 'Revisao OAR inesperada'}
    $obr=Join-Path $oar 'src\renderer\obr\obr_capi\obr\CMakeLists.txt'
    $text=Get-Content -LiteralPath $obr -Raw
    $text=$text.Replace('GIT_TAG v1.17.0',('GIT_TAG '+$lock.sources.googletest.commit))
    $text=$text.Replace('GIT_TAG 20250512.1',('GIT_TAG '+$lock.sources.absl.commit))
    if(-not $VerifyOnly){Set-Content -LiteralPath $obr -Value $text -Encoding utf8}
    $codecs=Join-Path $iamf 'code\dep_codecs\CMakeLists.txt'
    $text=Get-Content -LiteralPath $codecs -Raw
    foreach($name in @('opus','flac')){
        $entry=$lock.sources.$name
        $urlLine='URL '+$entry.url
        if($text -notmatch ('URL_HASH SHA256='+$entry.sha256)){$text=$text.Replace($urlLine,($urlLine+"`n    URL_HASH SHA256="+$entry.sha256))}
    }
    if(-not $VerifyOnly){Set-Content -LiteralPath $codecs -Value $text -Encoding utf8}
    $toolCmake=Join-Path $projectRoot 'out\tools\python\Lib\site-packages\cmake\data\bin'
    $env:Path="$toolCmake;$(Join-Path $projectRoot 'out\tools\python\Scripts');$env:Path"
    if(-not $VerifyOnly){
        $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        $vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        $env:Path+=';'+(Split-Path $vswhere)
        & (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
        chcp.com 850 | Out-Null
        $build=Join-Path $deps 'iamf-build'
        cmake -S (Join-Path $iamf 'code') -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DIAMF_BUILD_SHARED_LIB=ON -DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON -DENABLE_BUILD_CODECS=ON -DIAMF_TEST_TOOL=ON '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
        Check 'CMake IAMF/OAR'
        cmake --build $build --target iamf iamfdec --parallel 4
        Check 'Build IAMF/OAR'
    }
    foreach($name in @('absl','googletest','eigen','pffft')){
        $source=Join-Path $deps "iamf-build\_deps\$name-src"
        if((git -C $source rev-parse HEAD) -ne $lock.sources.$name.commit){throw "Revisao transitiva inesperada: $name"}
    }
    $aac=Join-Path $deps 'iamf-build\dep_codecs\dep_aac-prefix\src\dep_aac'
    if((git -C $aac rev-parse HEAD) -ne $lock.sources.'fdk-aac'.commit){throw 'Revisao AAC inesperada'}
    foreach($name in @('opus','flac')){
        $archive=Get-ChildItem -LiteralPath (Join-Path $deps 'iamf-build\dep_codecs') -Recurse -File | Where-Object {$_.Name -eq [IO.Path]::GetFileName($lock.sources.$name.url)} | Select-Object -First 1
        if(-not $archive){throw "Arquivo de origem ausente: $name"}
        Check-Hash $archive.FullName $lock.sources.$name.sha256
    }
}
Write-Host 'Dependencias locais conferidas contra third_party/versions.lock.'
