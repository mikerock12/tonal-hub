param([ValidateSet('devices','state','play','pause','transfer','seek','resume')][string]$Action='state',[string]$DeviceId='',[string]$TrackUri='',[int]$PositionMs=30000,[int]$MinimumValiditySeconds=60)
$ErrorActionPreference='Stop'
# Explicitly authorized local session transfer. Never print tokens or place them in argv.
if(-not ('TonalLocalCredential' -as [type])){Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class TonalLocalCredential {
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Credential {
  public uint Flags,Type; public string TargetName,Comment; public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
  public uint CredentialBlobSize; public IntPtr CredentialBlob; public uint Persist,AttributeCount; public IntPtr Attributes;public string TargetAlias,UserName;
 }
 [DllImport("advapi32.dll",EntryPoint="CredReadW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Read(string name,uint type,uint flags,out IntPtr value);
 [DllImport("advapi32.dll",EntryPoint="CredWriteW",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool Write(ref Credential credential,uint flags);
 [DllImport("advapi32.dll")] static extern void CredFree(IntPtr value);
 public static string Load(){IntPtr pointer;if(!Read("Tonal/Spotify",1,0,out pointer))throw new Exception("Sessão Tonal ausente no cofre do Windows");try{Credential c=Marshal.PtrToStructure<Credential>(pointer);byte[] bytes=new byte[c.CredentialBlobSize];Marshal.Copy(c.CredentialBlob,bytes,0,bytes.Length);return Encoding.UTF8.GetString(bytes);}finally{CredFree(pointer);}}
 public static void Save(string json){byte[] bytes=Encoding.UTF8.GetBytes(json);IntPtr p=Marshal.AllocHGlobal(bytes.Length);try{Marshal.Copy(bytes,0,p,bytes.Length);Credential c=new Credential{Type=1,TargetName="Tonal/Spotify",Comment="Login do Tonal no Spotify",CredentialBlob=p,CredentialBlobSize=(uint)bytes.Length,Persist=2,UserName="Spotify Web API"};if(!Write(ref c,0))throw new Exception("Não foi possível atualizar o cofre");}finally{for(int i=0;i<bytes.Length;i++)Marshal.WriteByte(p,i,0);Marshal.FreeHGlobal(p);Array.Clear(bytes,0,bytes.Length);}}
}
'@}
$stored=[TonalLocalCredential]::Load() | ConvertFrom-Json
$clientId=Get-ItemPropertyValue 'HKCU:\Software\Tonal\Tonal\auth' -Name clientId
$expires=if($stored.expiresAt -is [DateTime]){[DateTimeOffset]::new($stored.expiresAt)}else{[DateTimeOffset]::Parse($stored.expiresAt,[Globalization.CultureInfo]::InvariantCulture)}
if($expires.ToUnixTimeMilliseconds() -lt [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()+$MinimumValiditySeconds*1000){
    $fresh=Invoke-RestMethod -Uri 'https://accounts.spotify.com/api/token' -Method Post -Body @{grant_type='refresh_token';refresh_token=$stored.refresh;client_id=$clientId}
    if($fresh.refresh_token){$stored.refresh=$fresh.refresh_token}
    $stored.access=$fresh.access_token
    $expires=[DateTimeOffset]::UtcNow.AddSeconds($fresh.expires_in)
    $stored.expiresAt=$expires.UtcDateTime.ToString("yyyy-MM-ddTHH:mm:ssZ")
    [TonalLocalCredential]::Save(($stored | ConvertTo-Json -Compress))
}

# Use only the application's own authorized credential, without exporting it.
$headers=@{Authorization="Bearer $($stored.access)"}
$base='https://api.spotify.com/v1'
if($Action -eq 'devices'){
 $response=Invoke-RestMethod -Uri "$base/me/player/devices" -Headers $headers
 @($response.devices | Select-Object id,name,type,is_active,is_restricted,volume_percent) | ConvertTo-Json -Depth 5 -Compress
}elseif($Action -eq 'state'){
 $response=Invoke-RestMethod -Uri "$base/me/player" -Headers $headers
 @{playing=$response.is_playing;positionMs=$response.progress_ms;deviceId=$response.device.id;device=$response.device.name;uri=$response.item.uri;title=$response.item.name;artist=($response.item.artists.name -join ', ');durationMs=$response.item.duration_ms} | ConvertTo-Json -Compress
}else{
 if($DeviceId -notmatch '^[A-Za-z0-9_-]{8,160}$'){throw 'Choose one returned device ID'}
 if($Action -eq 'transfer'){
  $body=@{device_ids=@($DeviceId);play=$true}|ConvertTo-Json -Compress
  $null=Invoke-RestMethod -Uri "$base/me/player" -Method Put -Headers $headers -ContentType 'application/json' -Body $body
 }elseif($Action -eq 'resume'){
  $null=Invoke-RestMethod -Uri "$base/me/player/play?device_id=$DeviceId" -Method Put -Headers $headers
 }elseif($Action -eq 'seek'){
  if($PositionMs -lt 0 -or $PositionMs -gt 86400000){throw 'Invalid position'}
  $null=Invoke-RestMethod -Uri "$base/me/player/seek?position_ms=$PositionMs&device_id=$DeviceId" -Method Put -Headers $headers
 }elseif($Action -eq 'play'){
  if($TrackUri -notmatch '^spotify:track:[A-Za-z0-9]{22}$'){throw 'Choose a valid track URI'}
  $body=@{uris=@($TrackUri);position_ms=30000}|ConvertTo-Json -Compress
  $null=Invoke-RestMethod -Uri "$base/me/player/play?device_id=$DeviceId" -Method Put -Headers $headers -ContentType 'application/json' -Body $body
 }else{$null=Invoke-RestMethod -Uri "$base/me/player/pause?device_id=$DeviceId" -Method Put -Headers $headers}
 @{action=$Action;ok=$true} | ConvertTo-Json -Compress
}
