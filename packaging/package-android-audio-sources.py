"""Create the local corresponding-source bundle from explicit public-source directories only."""
from pathlib import Path
import tarfile, hashlib, json, re

root=Path(__file__).resolve().parents[1]
version=re.search(r"versionName '([^']+)'",(root/'android/app/build.gradle').read_text(encoding='utf-8')).group(1).removesuffix('-local')
revision=json.loads((root/'third_party/android-audio.lock').read_text(encoding='utf-8'))['ffmpeg']['revision']
paths=[
    'android/audio','android/app/src/main/java','android/app/src/main/res','android/app/src/main/assets','android/app/src/benchmark',
    'android/app/src/main/AndroidManifest.xml','android/app/build.gradle','android/build.gradle',
    'android/settings.gradle','android/gradle.properties','android/native/src','android/native/vendor',
    'android/native/Cargo.toml','android/native/Cargo.lock','LICENSE','CREDITS.md',
    'docs/PERSONAL_LIBRARY_050.md','docs/RARE_RECORDINGS_LOCAL.md',
    'packaging/build-android-local.ps1','packaging/build-android-audio-local.ps1',
    'packaging/build-android-ffmpeg-local.sh','packaging/package-android-audio-sources.py',
    'packaging/report-android-audio-local.py',
    'third_party/android-audio.lock','third_party/versions.lock','third_party/AUDIO-NOTICES.md',
    'docs/ANDROID_AUDIO_LOCAL.md','docs/ANDROID_LOCAL.md','docs/android-audio-tests.json',
    'tests/android/audio_features_smoke.py','tests/android/audio_smoke.py','tests/android/verify_apk.py',
    'tests/android/audio_stability_smoke.py','tests/android/audio_network_smoke.py',
    'tests/android/audio_panel_smoke.py','tests/android/catalog_smoke.py',
    'tests/android/audio_transitions_smoke.py','tests/android/apk_updates_smoke.py','tests/android/release_upgrade_smoke.py','tests/android/companion_smoke.py',
    'docs/ANDROID_HUB_040_LOCAL.md',
    f'out/android/audio-deps/FFmpeg-{revision}','out/android/audio-deps/libebur128-1.2.6',
    'out/android/audio-deps/fdk-aac','out/audio-deps/openal-soft','out/audio-deps/libiamf-src',
]
paths += [f'out/audio-deps/iamf-build/_deps/{name}-src' for name in ['absl','eigen','googletest','pffft']]
paths += [f'out/audio-deps/iamf-build/dep_codecs/dep_{name}-prefix/src/dep_{name}' for name in ['opus','flac']]
output=root/f'out/android/Tonal-Android-{version}-audio-sources.tar.gz'
def keep(info):
    parts=Path(info.name).parts
    if any(part in {'.git','.gradle','target','jniLibs','__pycache__'} for part in parts):return None
    if info.issym() or info.islnk():return None
    if info.name.endswith(('.dll','.so','.a','.lib','.exe','.obj','.o','.pdb','.pyc')):return None
    if '/dep_codecs/lib/' in info.name or '/dep_external/lib/' in info.name:return None
    if info.name.endswith(('credentials.json','test-auth.json','local.properties')):raise RuntimeError('Private input refused')
    info.uid=info.gid=0;info.uname=info.gname='';return info
with tarfile.open(output,'w:gz',compresslevel=6) as archive:
    for name in paths:
        path=root/name
        if not path.exists():raise FileNotFoundError(name)
        archive.add(path,arcname=name,filter=keep)
    for abi in ['arm64-v8a','x86_64']:
        for name in ['config.h','ffbuild/config.mak']:
            source=root/f'out/android/audio-deps/{abi}/ffmpeg-build'/name
            archive.add(source,arcname=f'build-configs/{abi}/{name}',filter=keep)
report={'file':output.name,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'size_bytes':output.stat().st_size}
(root/f'out/android/audio-sources-{version}.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
