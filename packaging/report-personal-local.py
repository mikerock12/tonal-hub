"""Consolidate executed 0.5.0 checks; never substitute historical tests for this build."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,re,zipfile
root=Path(__file__).resolve().parents[1]
def read(p):return json.loads((root/p).read_text(encoding='utf-8-sig'))
def text(p):return (root/p).read_text(encoding='utf-8-sig')
def sha(p):return hashlib.sha256((root/p).read_bytes()).hexdigest()
assert '100% tests passed, 0 tests failed out of 8' in text('out/ctest-050-package.log')
core=json.loads(re.search(r'result=(\{[^\r\n]+\})',text('out/android/core-050-final.log')).group(1));assert core['passed']
ui=read('out/android/companion-0.5.0/result.json');assert ui['passed']
assert ui['benchmark_apk_sha256']==sha('android/app/build/outputs/apk/benchmark/app-benchmark.apk')
audio=read('out/android/audio-features-0.5.0/result.json');assert audio['passed']
upgrade=read('out/android/release-upgrade-0.5.0/result.json');assert upgrade['passed']
assert 'CPP_BACKUP_IMPORT_PASS' in text('out/android/interop-cpp-to-android.log')
assert 'COMPANION_PASS' in text('out/interop-android-to-cpp.log')
assert 'LIVE_TRANSLATION_PASS' in text('out/companion-live-translation-http1.log')
shows={}
for band,filename in [('L7','l7-rare-live-test.json'),('Nirvana','nirvana-rare-live-test.json')]:
    result=read('out/research/'+filename);assert result['passed']
    shows[band]=result
artifacts={};cert=None
for kind,suffix in [('universal','local'),('arm64','arm64-v8a')]:
    filename=f'out/android/Tonal-Android-0.5.0-{suffix}.apk'
    validation=read(f'out/android/apk-validation-0.5.0-{kind}.json');assert validation['sha256']==sha(filename)
    signature=text(f'out/android/apk-signature-0.5.0-{kind}.txt');assert 'Verifies' in signature and 'v2): true' in signature
    digest=re.search(r'certificate SHA-256 digest: ([0-9a-f]+)',signature).group(1)
    assert cert is None or cert==digest;cert=digest
    with zipfile.ZipFile(root/filename) as apk:
        for entry in apk.namelist():
            if entry.endswith('.dex'):
                dex=apk.read(entry);assert b'Lapp/tonal/android/DeviceInstrumentation;' not in dex and b'Lapp/tonal/android/CompanionDeviceTests;' not in dex
        assert 'assets/licenses/CREDITS.md' in apk.namelist()
    artifacts[kind]=dict(path=filename,sha256=validation['sha256'],size_bytes=validation['size_bytes'],abis=list(validation['architectures']),elf_zip_alignment=16384,license_files=validation['license_files'])
assert cert=='464ae7cf34f4f145541569db215f8fcdef5deda1be31921d8e366eed20aa22c2'
assert upgrade['apk_sha256']==artifacts['universal']['sha256']
windows='out/release-local/Tonal-Hub-0.5.0-local.zip'
stage=root/'out/release-local/Tonal-Hub-0.5.0-local'
assert (stage/'CREDITS.md').is_file() and (stage/'sqldrivers/qsqlite.dll').is_file()
artifacts['windows']=dict(path=windows,sha256=sha(windows),size_bytes=(root/windows).stat().st_size)
report=dict(version='0.5.0-local',recorded_at_utc=datetime.now(timezone.utc).isoformat(),artifacts=artifacts,certificate_sha256=cert,
    windows_ctest=dict(passed=True,suites=8),android_core=dict(passed=True,checks=core['checks']),
    android_ui=dict(passed=True,checks=ui['checks'],benchmark_apk_sha256=ui['benchmark_apk_sha256']),
    android_native_audio=dict(passed=True,checks=len(audio['checks'])),release_upgrade=upgrade,
    backup_interoperability=dict(cpp_to_android=True,android_to_cpp=True,android_external_document_export_and_restore=True),
    live_translation=dict(windows_qt_http1=True,android=True,synthetic_phrase_only=True),public_fan_recording_playback=shows,
    limits=dict(spotify_authenticated_retested=False,physical_poco_x7_pro_tested=False,physical_bluetooth_tested=False,new_official_spotify_cpu_ram_comparison=False))
path=root/'docs/personal-library-050-tests.json';path.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(passed=True,report=str(path),android_core_checks=len(core['checks']),android_ui_checks=len(ui['checks']),native_audio_checks=len(audio['checks']),windows_suites=8),ensure_ascii=False))
