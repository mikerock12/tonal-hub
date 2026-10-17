param([Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$deps=Join-Path $root 'out\audio-deps'
New-Item -ItemType Directory -Force $Destination | Out-Null
$entries=@{
    'FFmpeg'=@('ffmpeg\ffmpeg-n8.1-latest-win64-lgpl-shared-8.1\LICENSE.txt')
    'OpenAL-Soft'=@('openal-soft\COPYING','openal-soft\LICENSE-pffft')
    'libiamf'=@('libiamf-src\LICENSE','libiamf-src\PATENTS')
    'OAR'=@('libiamf-src\code\dep_external\src\oar\LICENSE','libiamf-src\code\dep_external\src\oar\PATENTS')
    'Abseil'=@('iamf-build\_deps\absl-src\LICENSE')
    'Eigen'=@('iamf-build\_deps\eigen-src\COPYING.README','iamf-build\_deps\eigen-src\COPYING.MPL2','iamf-build\_deps\eigen-src\COPYING.BSD','iamf-build\_deps\eigen-src\COPYING.MINPACK')
    'Opus'=@('iamf-build\dep_codecs\dep_opus-prefix\src\dep_opus\COPYING')
    'FDK-AAC'=@('iamf-build\dep_codecs\dep_aac-prefix\src\dep_aac\NOTICE')
    'FLAC'=@('iamf-build\dep_codecs\dep_flac-prefix\src\dep_flac\COPYING.Xiph','iamf-build\dep_codecs\dep_flac-prefix\src\dep_flac\COPYING.FDL','iamf-build\dep_codecs\dep_flac-prefix\src\dep_flac\COPYING.GPL','iamf-build\dep_codecs\dep_flac-prefix\src\dep_flac\COPYING.LGPL')
}
foreach($entry in $entries.GetEnumerator()){
    foreach($file in $entry.Value){$path=Join-Path $deps $file;if(Test-Path -LiteralPath $path){$dir=Join-Path $Destination $entry.Key;New-Item -ItemType Directory -Force $dir | Out-Null;Copy-Item -LiteralPath $path -Destination $dir -Force}}
}
Copy-Item -LiteralPath (Join-Path $root 'third_party\versions.lock') -Destination $Destination -Force
Copy-Item -LiteralPath (Join-Path $root 'third_party\AUDIO-NOTICES.md') -Destination $Destination -Force

$pffft=Join-Path $deps 'iamf-build\_deps\pffft-src\pffft.h'
if(Test-Path -LiteralPath $pffft){
    $header=Get-Content -LiteralPath $pffft -Raw
    $end=$header.IndexOf('*/')
    if($end -ge 0){$header.Substring(0,$end+2) | Set-Content -LiteralPath (Join-Path $Destination 'PFFFT-NOTICE.txt') -Encoding utf8}
}
