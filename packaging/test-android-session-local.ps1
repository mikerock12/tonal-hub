param([switch]$Import,[string]$Package='app.tonal.android.benchmark',[ValidateRange(60,3600)][int]$MinimumValiditySeconds=60)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
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
$headers=@{Authorization="Bearer $($stored.access)"}
$profile=Invoke-RestMethod -Uri 'https://api.spotify.com/v1/me' -Headers $headers
$credentials=Get-Content -LiteralPath (Join-Path $env:LOCALAPPDATA 'Tonal\Tonal\librespot\credentials.json') -Raw | ConvertFrom-Json
$seed=@{clientId=$clientId;tokens=@{access_token=$stored.access;refresh_token=$stored.refresh;expires_at=$expires.ToUnixTimeMilliseconds()};credentials=$credentials} | ConvertTo-Json -Depth 8 -Compress
$privatePath=Join-Path $root 'out\android\session-private'
New-Item -ItemType Directory -Force $privatePath | Out-Null
$identity=[System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
& icacls.exe $privatePath '/inheritance:r' '/grant:r' "*${identity}:(OI)(CI)F" | Out-Null
if($LASTEXITCODE -ne 0){throw 'Não foi possível restringir o armazenamento temporário da sessão'}
$seedPath=Join-Path $privatePath 'test-auth.json'
[System.IO.File]::WriteAllText($seedPath,$seed,[System.Text.UTF8Encoding]::new($false))
[pscustomobject]@{sessionAvailable=$true;product=$profile.product;expiresAt=$expires.ToString('o');storedCredentialsAvailable=($null -ne $credentials)} | ConvertTo-Json
if($Import){
    $adb=Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    & $adb shell run-as $Package mkdir -p files
    $start=[System.Diagnostics.ProcessStartInfo]::new($adb)
    $start.ArgumentList.Add('shell');$start.ArgumentList.Add("run-as $Package sh -c 'cat > files/test-auth.json'")
    $start.RedirectStandardInput=$true;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
    $process=[System.Diagnostics.Process]::Start($start)
    $process.StandardInput.Write($seed);$process.StandardInput.Close();$process.WaitForExit()
    if($process.ExitCode -ne 0){throw 'Importação privada falhou'}
    Write-Host 'Sessão copiada somente para o sandbox do APK de teste; ausente no APK distribuído.'
}
