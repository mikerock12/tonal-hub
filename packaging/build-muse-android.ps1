param([string[]]$Abis=@('x86_64','arm64-v8a'))
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$cmake=Join-Path $root 'out/tools/cmake331/cmake/data/bin/cmake.exe'
$ninja=Join-Path $root 'out/tools/python/Scripts/ninja.exe'
$ndk=Join-Path $env:LOCALAPPDATA 'Android/Sdk/ndk/27.0.12077973'
foreach($abi in $Abis){
    $build=Join-Path $root "out/android/muse/$abi"
    & $cmake -S (Join-Path $root 'android/muse') -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$ndk/build/cmake/android.toolchain.cmake" "-DANDROID_ABI=$abi" -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE -ne 0){throw 'Muse CMake falhou'}
    & $cmake --build $build --parallel 2
    if($LASTEXITCODE -ne 0){throw 'Muse build falhou'}
    Copy-Item -LiteralPath (Join-Path $build 'libtonal_muse.so') -Destination (Join-Path $root "android/app/src/main/jniLibs/$abi/libtonal_muse.so") -Force
}
