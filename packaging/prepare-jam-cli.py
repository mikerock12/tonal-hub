"""Prepare a verified upstream CLI plus the existing MIT protocol patches; no installation."""
from pathlib import Path
import hashlib, json, os, shutil, tomllib, tarfile, io

ROOT=Path(__file__).resolve().parents[1]
registry=Path(os.environ.get('CARGO_HOME',Path.home()/'.cargo'))/'registry/src'
matches=list(registry.glob('*/librespot-0.8.0'))
if len(matches)!=1: raise SystemExit('Expected one cached librespot 0.8.0 source package')
source=matches[0].resolve()
lock=tomllib.loads((ROOT/'android/native/Cargo.lock').read_text(encoding='utf-8'))
pinned=next(p for p in lock['package'] if p['name']=='librespot' and p['version']=='0.8.0')
archives=list(registry.parent.joinpath('cache').glob('*/librespot-0.8.0.crate'))
if len(archives)!=1: raise SystemExit('Expected one cached upstream crate archive')
archive=archives[0].read_bytes()
if hashlib.sha256(archive).hexdigest()!=pinned['checksum']: raise SystemExit('Upstream package checksum mismatch')
dest=ROOT/'out/jam-cli-source/librespot-0.8.0'
count=0
with tarfile.open(fileobj=io.BytesIO(archive),mode='r:gz') as package:
    for member in package:
        relative=Path(member.name)
        if relative.parts[0]!='librespot-0.8.0' or member.issym() or member.islnk(): raise SystemExit('Invalid crate entry')
        target=(dest/Path(*relative.parts[1:])).resolve()
        if not target.is_relative_to(dest.resolve()): raise SystemExit('Invalid crate path')
        if member.isdir(): target.mkdir(parents=True,exist_ok=True)
        elif member.isfile():
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(package.extractfile(member).read());count+=1
        else: raise SystemExit('Unsupported crate entry')
shutil.copyfile(ROOT/'android/native/Cargo.lock',dest/'Cargo.lock')
patches={name:{'path':(ROOT/'android/native/vendor'/name).as_posix()} for name in
         ('librespot-core','librespot-audio','librespot-playback','librespot-connect')}
config=dest/'.cargo/config.toml';config.parent.mkdir(exist_ok=True)
config.write_text('[patch.crates-io]\n'+''.join(f'{k} = {{path = "{v["path"]}"}}\n' for k,v in patches.items()),encoding='utf-8')
(ROOT/'out/jam-cli-source/provenance.json').write_text(json.dumps({'upstream':'librespot 0.8.0','registrySha256':pinned['checksum'],'verifiedSourceFiles':count,'patches':list(patches)},indent=2),encoding='utf-8')
print(f'Verified {count} upstream source files; CLI prepared in workspace')
