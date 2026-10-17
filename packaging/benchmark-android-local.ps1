param([switch]$SkipReset)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$python=Join-Path $root 'out/tools/python/Scripts/python.exe'
$measurement=Join-Path $root 'tests/android/measure.py'
$results=Join-Path $root 'out/android/measurements-final'
if((& $adb shell getprop ro.kernel.qemu).Trim() -ne '1'){throw 'Este roteiro limpa dados somente no emulador isolado de testes.'}
$avd=(& $adb emu avd name | Select-Object -First 1).Trim()
if($avd -ne 'Tonal_Local_20261015'){throw "AVD inesperada: $avd. Use somente a AVD isolada Tonal_Local_20261015."}
& $adb shell am force-stop app.tonal.android.benchmark
& $adb shell am force-stop com.android.vending
if(-not $SkipReset){
    foreach($package in @('app.tonal.android','com.spotify.music')){
        & $adb shell pm clear $package
        if($LASTEXITCODE -ne 0){throw "Falha ao limpar $package no emulador"}
        & $adb shell pm grant $package android.permission.POST_NOTIFICATIONS
    }
}
foreach($round in 1,2){
    $packages=if($round -eq 1){@('app.tonal.android','com.spotify.music')}else{@('com.spotify.music','app.tonal.android')}
    foreach($background in $false,$true){
        foreach($package in $packages){
            foreach($peer in @('app.tonal.android','com.spotify.music')){& $adb shell am force-stop $peer}
            $short=if($package -eq 'com.spotify.music'){'spotify'}else{'tonal'}
            $visibility=if($background){'background'}else{'foreground'}
            $label="$short-$visibility-$round"
            $arguments=@($measurement,'--adb',$adb,'--package',$package,'--scenario','idle','--label',$label,'--output',(Join-Path $results $label),'--samples','6','--interval','3','--warmup','10')
            if($background){$arguments+='--background'}
            & $python @arguments
            if($LASTEXITCODE -ne 0){throw "Medição $label falhou"}
        }
    }
}
foreach($scenario in @('local','effects')){
    & $adb shell am force-stop com.spotify.music
    & $python $measurement --adb $adb --package app.tonal.android --scenario $scenario --label "tonal-$scenario" --output (Join-Path $results "tonal-$scenario") --samples 6 --interval 4 --warmup 10
    if($LASTEXITCODE -ne 0){throw "Medição $scenario falhou"}
}
& $adb shell am force-stop app.tonal.android
Write-Output "Resultados brutos: $results"
