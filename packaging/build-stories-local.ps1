param([switch]$Android,[string[]]$Abis=@('x86_64','arm64-v8a'),[string]$BuildDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$python=Join-Path $root 'out/tools/python/Scripts/python.exe'
& $python -X utf8 (Join-Path $PSScriptRoot 'fetch-story-runtime.py')
if($LASTEXITCODE -ne 0){throw 'Verificação do runtime local falhou'}
$cmake=Join-Path $root 'out/tools/cmake331/cmake/data/bin/cmake.exe'
$ninja=Join-Path $root 'out/tools/python/Scripts/ninja.exe'
if($Android){
 $ndk=Join-Path $env:LOCALAPPDATA 'Android/Sdk/ndk/27.0.12077973'
 foreach($abi in $Abis){
  $build=Join-Path $root "out/stories/android/$abi"
  & $cmake -S (Join-Path $root 'src/stories') -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" "-DANDROID_ABI=$abi" -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
  if($LASTEXITCODE -ne 0){throw 'Stories Android configure falhou'}
  & $cmake --build $build --target tonal_story --parallel 2
  if($LASTEXITCODE -ne 0){throw 'Stories Android build falhou'}
  $dest=Join-Path $root "android/app/src/main/jniLibs/$abi"
  New-Item -ItemType Directory -Force $dest | Out-Null
  Copy-Item -LiteralPath (Join-Path $build 'libtonal_story.so') -Destination $dest -Force
 }
}else{
 if(-not $BuildDirectory){$BuildDirectory=Join-Path $root 'out/stories/windows'}
 $qt=if($env:QTDIR){$env:QTDIR}else{Join-Path $root 'out/tools/Qt/6.8.3/msvc2022_64'}
 & $cmake -S (Join-Path $root 'src/stories') -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$qt" -DCMAKE_BUILD_TYPE=Release
 if($LASTEXITCODE -ne 0){throw 'Stories Windows configure falhou'}
 & $cmake --build $BuildDirectory --target TonalStories --parallel 2
 if($LASTEXITCODE -ne 0){throw 'Stories Windows build falhou'}
 Copy-Item -LiteralPath (Join-Path $BuildDirectory 'TonalStories.exe') -Destination (Join-Path $root 'out/build/release') -Force
}
