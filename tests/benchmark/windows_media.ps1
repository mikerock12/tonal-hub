param([string]$Match='',[ValidateSet('snapshot','play','pause','seek')][string]$Action='snapshot',[long]$PositionMs=0)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$null=[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager,Windows.Media.Control,ContentType=WindowsRuntime]
function Await-Result($Operation,[Type]$ResultType){
 $method=[System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {$_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'} | Select-Object -First 1
 $task=$method.MakeGenericMethod($ResultType).Invoke($null,@($Operation));if(-not $task.Wait(12000)){throw 'Media API timeout'};return $task.Result
}
$manager=Await-Result ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager]::RequestAsync()) ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager])
$output=@()
foreach($session in $manager.GetSessions()){
 $id=$session.SourceAppUserModelId
 if($Match -and $id -notlike "*$Match*"){continue}
 if($Action -ne 'snapshot'){
  $operation=switch($Action){'play'{$session.TryPlayAsync()};'pause'{$session.TryPauseAsync()};'seek'{$session.TryChangePlaybackPositionAsync($PositionMs*10000)}}
  $success=Await-Result $operation ([bool])
 }
 $info=$session.GetPlaybackInfo();$time=$session.GetTimelineProperties()
 $media=Await-Result ($session.TryGetMediaPropertiesAsync()) ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties])
 $output+=@{app=$id;status=$info.PlaybackStatus.ToString();title=$media.Title;artist=$media.Artist;positionMs=$time.Position.TotalMilliseconds;durationMs=$time.EndTime.TotalMilliseconds;canPlay=$info.Controls.IsPlayEnabled;action=$Action;actionSucceeded=$success}
}
ConvertTo-Json -InputObject @($output) -Depth 5
