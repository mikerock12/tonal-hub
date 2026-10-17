param([switch]$DependenciesOnly,[string[]]$Abis=@('x86_64','arm64-v8a'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$deps=Join-Path $root 'out\android\audio-deps'
$fdk=Join-Path $deps 'fdk-aac'
if(-not(Test-Path "$fdk\CMakeLists.txt")){
  Copy-Item -LiteralPath "$root\out\audio-deps\iamf-build\dep_codecs\dep_aac-prefix\src\dep_aac" -Destination $fdk -Recurse
  $log="$fdk\libSBRdec\include\log\log.h"
  if(Test-Path $log){$text=[IO.File]::ReadAllText($log).Replace('void android_errorWriteLog','static inline void android_errorWriteLog');[IO.File]::WriteAllText($log,$text)}
}
$cmake="$root\out\tools\cmake331\cmake\data\bin\cmake.exe"
$ninja="$root\out\tools\python\Scripts\ninja.exe"
$ndk="$env:LOCALAPPDATA\Android\Sdk\ndk\27.0.12077973"
$runtimeNotices=Join-Path $root 'android\app\src\main\assets\licenses\audio\LLVM-runtime'
New-Item -ItemType Directory -Force $runtimeNotices | Out-Null
Copy-Item -LiteralPath "$ndk\toolchains\llvm\prebuilt\windows-x86_64\NOTICE" -Destination (Join-Path $runtimeNotices 'NOTICE') -Force
foreach($abi in $Abis){
  $build="$root\out\android\audio-deps\$abi\plugin-build"
  & $cmake -S "$root\android\audio" -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk\build\cmake\android.toolchain.cmake" "-DANDROID_ABI=$abi" -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON -DCMAKE_BUILD_TYPE=Release
  if($LASTEXITCODE -ne 0){throw "CMake Android $abi falhou"}
  [string[]]$buildTargets=if($DependenciesOnly){@('OpenAL','iamf','ebur128')}else{@('tonal_audio')}
  & $cmake --build $build --target $buildTargets --parallel 4
  if($LASTEXITCODE -ne 0){throw "Plugin Android $abi falhou"}
  if(-not $DependenciesOnly){
    $dest="$root\android\app\src\main\jniLibs\$abi";New-Item -ItemType Directory -Force $dest | Out-Null
    Copy-Item "$build\libtonal_audio.so" $dest -Force
    Copy-Item "$build\openal\libopenal.so" $dest -Force
    Copy-Item "$deps\$abi\ffmpeg\lib\*.so" $dest -Force
    $triple=if($abi -eq 'x86_64'){'x86_64-linux-android'}else{'aarch64-linux-android'}
    Copy-Item "$ndk\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\$triple\libc++_shared.so" $dest -Force
    Get-ChildItem -LiteralPath $dest -Filter '*.so' | ForEach-Object {& "$ndk\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe" --strip-unneeded $_.FullName}
  }
}
