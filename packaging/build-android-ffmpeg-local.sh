#!/usr/bin/env bash
set -euo pipefail
export PATH="/usr/bin:$PATH"
cd "$(dirname "$0")/.."
root="$PWD"
ndk="$(cygpath -u "$LOCALAPPDATA")/Android/Sdk/ndk/27.0.12077973"
bin="$ndk/toolchains/llvm/prebuilt/windows-x86_64/bin"
sysroot="$(cygpath -m "$ndk/toolchains/llvm/prebuilt/windows-x86_64/sysroot")"
revision="$(tr -d '\r\n' < out/android/audio-deps/ffmpeg-revision.txt)"
source="$root/out/android/audio-deps/FFmpeg-$revision"
for abi in arm64-v8a x86_64; do
  if [[ "$abi" == arm64-v8a ]]; then arch=aarch64; target=aarch64-linux-android28; else arch=x86_64; target=x86_64-linux-android28; fi
  build="$root/out/android/audio-deps/$abi/ffmpeg-build"
  prefix="$root/out/android/audio-deps/$abi/ffmpeg"
  mkdir -p "$build" "$prefix/include"
  cd "$build"
  if [[ ! -f "$prefix/lib/libavfilter.so" ]]; then
    "$source/configure" --prefix="$(cygpath -m "$prefix")" --target-os=android --arch="$arch" --enable-cross-compile \
      --cc="$bin/clang.exe --target=$target" --cxx="$bin/clang++.exe --target=$target" \
      --ld="$bin/clang.exe --target=$target" --ar="$bin/llvm-ar.exe" --nm="$bin/llvm-nm.exe" --ranlib="$bin/llvm-ranlib.exe" --strip="$bin/llvm-strip.exe" \
      --sysroot="$sysroot" --extra-cflags="-O3 -fPIC" --extra-ldflags="-Wl,-z,max-page-size=16384" \
      --disable-autodetect --disable-everything --disable-programs --disable-doc --disable-network --disable-asm --disable-debug \
      --enable-small --enable-shared --disable-static --disable-symver --enable-version3 \
      --enable-avcodec --enable-avformat --enable-avfilter --enable-swresample --enable-avutil --disable-swscale --disable-avdevice \
      --enable-protocol=file \
      --enable-decoder='aac,aac_fixed,ac3,eac3,alac,ape,flac,mp1,mp2,mp3,mp3float,opus,vorbis,wavpack,wmav1,wmav2,wmapro,wmalossless,pcm*,adpcm*' \
      --enable-demuxer='aac,ac3,eac3,aiff,ape,asf,flac,matroska,mov,mp3,ogg,wav,wv' \
      --enable-parser='aac,aac_latm,ac3,flac,mpegaudio,opus,vorbis' \
      --enable-filter='abuffer,abuffersink,aformat,aresample,equalizer,bass,volume,acompressor,alimiter,anull'
    make -j4 > build.log 2>&1 || { tail -50 build.log; exit 1; }
    make install > install.log 2>&1 || { tail -30 install.log; exit 1; }
  fi
  echo "FFmpeg Android $abi pronto"
done
