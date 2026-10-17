param([switch]$NativeOnly,[switch]$SkipNative,[switch]$SkipAudio,[switch]$PhoneOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$sdkPath=Join-Path $env:LOCALAPPDATA 'Android\Sdk'
$ndkPath=Join-Path $sdkPath 'ndk\27.0.12077973'
$compilerPath=Join-Path $ndkPath 'toolchains\llvm\prebuilt\windows-x86_64\bin'
$env:JAVA_HOME='C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot'
$env:ANDROID_HOME=$sdkPath
$env:ANDROID_NDK_HOME=$ndkPath
$env:PATH="$env:USERPROFILE\.cargo\bin;$env:JAVA_HOME\bin;$compilerPath;$env:PATH"
$env:CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER=Join-Path $compilerPath 'x86_64-linux-android28-clang.cmd'
$env:CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER=Join-Path $compilerPath 'aarch64-linux-android28-clang.cmd'
$env:CC_x86_64_linux_android=$env:CARGO_TARGET_X86_64_LINUX_ANDROID_LINKER
$env:CC_aarch64_linux_android=$env:CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER
$env:AR_x86_64_linux_android=Join-Path $compilerPath 'llvm-ar.exe'
$env:AR_aarch64_linux_android=$env:AR_x86_64_linux_android
$env:CARGO_TARGET_X86_64_LINUX_ANDROID_RUSTFLAGS='-C link-arg=-Wl,-z,max-page-size=16384'
$env:CARGO_TARGET_AARCH64_LINUX_ANDROID_RUSTFLAGS=$env:CARGO_TARGET_X86_64_LINUX_ANDROID_RUSTFLAGS
if(-not $SkipNative){
    Push-Location (Join-Path $root 'android\native')
    try {
        cargo build --locked --release --target x86_64-linux-android --target aarch64-linux-android
        if($LASTEXITCODE -ne 0){throw 'Build Rust Android falhou'}
    } finally {Pop-Location}
    foreach($abi in @(@('x86_64','x86_64-linux-android'),@('arm64-v8a','aarch64-linux-android'))){
        $dest=Join-Path $root "android\app\src\main\jniLibs\$($abi[0])"
        New-Item -ItemType Directory -Force $dest | Out-Null
        Copy-Item -LiteralPath (Join-Path $root "android\native\target\$($abi[1])\release\libtonal_android.so") -Destination $dest -Force
    }
}
if($NativeOnly){return}
if(-not $SkipAudio){
    & (Join-Path $PSScriptRoot 'build-android-audio-local.ps1')
}
foreach($abi in @('x86_64','arm64-v8a')){
    if(-not(Test-Path -LiteralPath (Join-Path $root "android\app\src\main\jniLibs\$abi\libtonal_android.so"))){
        throw "Biblioteca nativa $abi ausente. Execute novamente sem -SkipNative."
    }
}
[IO.File]::WriteAllText((Join-Path $root 'android\local.properties'),"sdk.dir=$($sdkPath.Replace('\','/').Replace(':','\:'))`n",[Text.Encoding]::ASCII)
$gradlePath=Join-Path $env:USERPROFILE '.gradle\wrapper\dists\gradle-8.14.3-all\10utluxaxniiv4wxiphsi49nj\gradle-8.14.3\bin\gradle.bat'
$gradleArgs=@('-p',(Join-Path $root 'android'),'assembleRelease','--console=plain')
if($PhoneOnly){$gradleArgs+='-PtonalAbi=arm64-v8a'}
& $gradlePath @gradleArgs
if($LASTEXITCODE -ne 0){throw 'Build APK falhou'}
$outputPath=Join-Path $root 'out\android'
New-Item -ItemType Directory -Force $outputPath | Out-Null
$apkName=if($PhoneOnly){'Tonal-Android-0.6.0-arm64-v8a.apk'}else{'Tonal-Android-0.6.0-local.apk'}
Copy-Item -LiteralPath (Join-Path $root 'android\app\build\outputs\apk\release\app-release.apk') -Destination (Join-Path $outputPath $apkName) -Force
Write-Host "APK local: $outputPath\$apkName"
