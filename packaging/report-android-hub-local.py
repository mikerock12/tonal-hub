"""Consolidate the executed Android 0.4.0 checks and verify their deliverable hashes."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,re,tarfile,zipfile

root=Path(__file__).resolve().parents[1]
def read(path):return json.loads((root/path).read_text(encoding='utf-8-sig'))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
artifacts={}
certificate=None
for kind,filename in [('universal','Tonal-Android-0.4.0-local.apk'),('arm64','Tonal-Android-0.4.0-arm64-v8a.apk')]:
    path=root/'out/android'/filename;validation=read(f'out/android/apk-validation-0.4.0-{kind}.json')
    assert digest(path)==validation['sha256'] and path.stat().st_size==validation['size_bytes']
    assert not validation['private_seed_file_present'] and not validation['session_token_present']
    signature=(root/f'out/android/apk-signature-0.4.0-{kind}.txt').read_text(encoding='utf-8-sig')
    assert 'Verifies' in signature and 'v2): true' in signature
    cert=re.search(r'certificate SHA-256 digest: ([0-9a-f]+)',signature).group(1)
    if certificate is not None:assert cert==certificate
    certificate=cert
    with zipfile.ZipFile(path) as apk:
        dex=apk.read('classes.dex')
        assert b'Lapp/tonal/android/CoreTests;' not in dex and b'Lapp/tonal/android/DeviceInstrumentation;' not in dex
    artifacts[kind]=dict(path=str(path.relative_to(root)).replace('\\','/'),sha256=validation['sha256'],size_bytes=path.stat().st_size,abis=list(validation['architectures']),elf_and_zip_alignment=16384,license_files=validation['license_files'],test_credentials_absent=True)

source=read('out/android/audio-sources-0.4.0.json');source_path=root/'out/android'/source['file'];assert digest(source_path)==source['sha256']
verified=[]
with tarfile.open(source_path,'r:gz') as archive:
    names={m.name:m for m in archive.getmembers()}
    assert not any(Path(name).name in {'credentials.json','test-auth.json','local.properties'} for name in names)
    assert not any(m.issym() or m.islnk() for m in names.values())
    critical=list((root/'android/app/src/main/java').rglob('*.java'))+list((root/'android/app/src/benchmark').rglob('*.java'))
    critical += [root/p for p in ['android/native/src/lib.rs','android/native/Cargo.toml','android/native/Cargo.lock','android/app/build.gradle','android/app/src/main/AndroidManifest.xml','docs/ANDROID_HUB_040_LOCAL.md','tests/android/apk_updates_smoke.py','tests/android/release_upgrade_smoke.py']]
    for path in critical:
        name=path.relative_to(root).as_posix();assert name in names,name
        assert hashlib.sha256(archive.extractfile(names[name]).read()).hexdigest()==digest(path),name
        verified.append(name)
artifacts['sources']=dict(path=str(source_path.relative_to(root)).replace('\\','/'),**{k:v for k,v in source.items() if k!='file'},verified_source_files=len(verified))

integration=read('out/android/updates-0.4.0/result.json');audio=read('out/android/audio-features-0.4.0/result.json');upgrade=read('out/android/release-upgrade-0.4.0/result.json')
assert integration['passed'] and audio['passed'] and upgrade['passed']
assert upgrade['apk_sha256']==artifacts['universal']['sha256']
assert digest(root/'android/app/build/outputs/apk/benchmark/app-benchmark.apk')==integration['benchmark_apk_sha256']
core_text=(root/'out/android/updates-0.4.0/core-tests.txt').read_text(encoding='utf-8')
core=json.loads(re.search(r'result=(\{[^\r\n]+\})',core_text).group(1));assert core['passed']
report=dict(version='0.4.0-local',recorded_at_utc=datetime.now(timezone.utc).isoformat(),local_only=True,git_commit_push_pr_performed=False,artifacts=artifacts,certificate_sha256=certificate,
    passed=True,core_checks=core,device_integration=integration,native_audio=audio,release_upgrade=upgrade,
    reports=['out/android/updates-0.4.0/result.json','out/android/audio-features-0.4.0/result.json','out/android/release-upgrade-0.4.0/result.json'],
    limitations=dict(physical_poco_bluetooth_or_car_tested=False,authenticated_spotify_stream_retested=False,new_official_spotify_benchmark=False,jamendo_own_credentials_tested=False,opensubsonic_own_server_tested=False))
(root/'docs/android-hub-040-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(passed=True,artifacts=artifacts,core_checks=len(core['checks']),integration_checks=len(integration['checks']),native_audio_checks=len(audio['checks']),upgrade_preserved_data=upgrade['data_directory_preserved']),ensure_ascii=False,indent=2))
