param([ValidateSet('dump','click','input')][string]$Action='dump',[string]$Text,[string]$PrivateInputFile)
$ErrorActionPreference='Stop'
$adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
$root=Split-Path $PSScriptRoot -Parent
$snapshot=Join-Path $root 'out/android/current-ui.xml'
& $adb shell uiautomator dump /sdcard/tonal-test-ui.xml | Out-Null
if($LASTEXITCODE -ne 0){throw 'Não foi possível inspecionar a tela atual'}
& $adb pull /sdcard/tonal-test-ui.xml $snapshot 2>$null | Out-Null
[xml]$ui=Get-Content -LiteralPath $snapshot
if($Action -eq 'dump'){
    $ui.SelectNodes('//node') | Where-Object {($_.text -or $_.'content-desc') -and $_.password -ne 'true'} | ForEach-Object {
        if($_.class -eq 'android.widget.EditText'){Write-Output "<input> | $($_.bounds)"}
        else {Write-Output "$($_.text) | $($_.'content-desc') | $($_.bounds)"}
    }
    return
}
$node=if($Action -eq 'input'){$ui.SelectNodes('//node[@class="android.widget.EditText"]') | Select-Object -First 1}
else {$ui.SelectNodes('//node') | Where-Object {$_.text -eq $Text -or $_.'content-desc' -eq $Text} | Select-Object -First 1}
if(-not $node -or $node.enabled -ne 'true'){throw 'Controle visível habilitado não encontrado'}
if($node.bounds -notmatch '^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$'){throw 'Coordenadas ausentes'}
$x=[int]( ([int]$Matches[1]+[int]$Matches[3])/2 );$y=[int]( ([int]$Matches[2]+[int]$Matches[4])/2 )
& $adb shell input tap $x $y
if($Action -eq 'input'){
    if(-not $PrivateInputFile){throw 'Arquivo privado de entrada necessário'}
    $value=[IO.File]::ReadAllText((Resolve-Path -LiteralPath $PrivateInputFile)).Trim()
    if($value -notmatch '^[a-zA-Z0-9@+._-]+$'){throw 'Formato não suportado pelo preenchimento ADB'}
    & $adb shell input keyevent 123
    if($node.text.Length){& $adb shell input keyevent (@(67)*$node.text.Length)}
    & $adb shell input text $value
    & $adb shell input keyevent 4
}
