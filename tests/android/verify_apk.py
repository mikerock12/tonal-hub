"""Verify local APK packaging and check private test tokens are absent; never print them."""
import hashlib
import json
from pathlib import Path
import struct
import zipfile
import argparse

root = Path(__file__).resolve().parents[2]
parser=argparse.ArgumentParser()
parser.add_argument('--apk',type=Path,default=root/'out/android/Tonal-Android-0.3.0-local.apk')
parser.add_argument('--output',type=Path,default=root/'out/android/apk-validation-0.3.0.json')
parser.add_argument('--abis',default='arm64-v8a,x86_64')
args=parser.parse_args()
apk = args.apk
report = dict(sha256=hashlib.sha256(apk.read_bytes()).hexdigest(), size_bytes=apk.stat().st_size,
              architectures={}, license_files=0, private_seed_file_present=False, session_token_present=False)
with apk.open('rb') as raw, zipfile.ZipFile(apk) as archive:
    names = archive.namelist()
    report['license_files'] = sum(n.startswith('assets/licenses/') for n in names)
    report['private_seed_file_present'] = any(n.endswith(('test-auth.json', 'test-account.json', 'credentials.json')) for n in names)
    for abi, machine in [('arm64-v8a', 183), ('x86_64', 62)]:
        if abi not in args.abis.split(','):
            assert not any(n.startswith(f'lib/{abi}/') for n in names)
            continue
        libraries = [n for n in names if n.startswith(f'lib/{abi}/') and n.endswith('.so')]
        expected = {'libtonal_android.so','libtonal_audio.so','libopenal.so','libc++_shared.so',
                    'libavfilter.so','libavformat.so','libavcodec.so','libavutil.so','libswresample.so'}
        assert {Path(n).name for n in libraries} == expected
        results = {}
        for name in libraries:
            info = archive.getinfo(name)
            binary = archive.read(info)
            assert binary[:5] == b'\x7fELF\x02' and struct.unpack_from('<H', binary, 18)[0] == machine
            phoff = struct.unpack_from('<Q', binary, 32)[0]
            size, count = struct.unpack_from('<HH', binary, 54)
            alignment = [struct.unpack_from('<Q', binary, phoff+i*size+48)[0] for i in range(count)
                         if struct.unpack_from('<I', binary, phoff+i*size)[0] == 1]
            raw.seek(info.header_offset+26)
            name_len, extra_len = struct.unpack('<HH', raw.read(4))
            offset = info.header_offset+30+name_len+extra_len
            filename=Path(name).name
            stripped = root / f'android/app/build/intermediates/stripped_native_libs/release/stripReleaseDebugSymbols/out/lib/{abi}/{filename}'
            merged = root / f'android/app/build/intermediates/merged_native_libs/release/mergeReleaseNativeLibs/out/lib/{abi}/{filename}'
            source = root / f'android/app/src/main/jniLibs/{abi}/{filename}'
            result = dict(elf_load_alignment=alignment, zip_offset=offset,
                          uncompressed=info.compress_type == zipfile.ZIP_STORED,
                          matches_packaged_build=binary == stripped.read_bytes(),
                          before_strip_matches_source=merged.read_bytes() == source.read_bytes(),
                          sha256=hashlib.sha256(binary).hexdigest())
            assert all(a >= 16384 for a in alignment) and offset % 16384 == 0, name
            assert result['uncompressed'] and result['matches_packaged_build'] and result['before_strip_matches_source'], name
            results[filename]=result
        report['architectures'][abi] = results
    seed_file = root / 'out/android/session-private/test-auth.json'
    if seed_file.exists():
        seed = json.loads(seed_file.read_text(encoding='utf-8-sig'))
        needles = [seed['tokens'].get(key, '').encode() for key in ('access_token', 'refresh_token')]
        for name in names:
            if name.endswith(('.dex', '.json', '.xml', '.txt')):
                content = archive.read(name)
                if any(value and value in content for value in needles):
                    report['session_token_present'] = True
    assert not report['private_seed_file_present'] and not report['session_token_present']
args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
