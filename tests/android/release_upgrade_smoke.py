"""Upgrade the non-debug APK in the dedicated AVD, retaining the package's private data."""
from pathlib import Path
import hashlib,json,os,re,shlex,subprocess,time,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
SERIAL=os.environ.get('ANDROID_SERIAL','emulator-5560');PACKAGE='app.tonal.android'
VERSION=re.search(r"versionName '([^']+)'",(ROOT/'android/app/build.gradle').read_text()).group(1).removesuffix('-local')
CODE=re.search(r'versionCode (\d+)',(ROOT/'android/app/build.gradle').read_text()).group(1)
OUT=ROOT/f'out/android/release-upgrade-{VERSION}';OUT.mkdir(parents=True,exist_ok=True)
def run(*args,binary=False):
    if args[0]=='shell':args=('shell',shlex.join(map(str,args[1:])))
    out=subprocess.check_output([str(ADB),'-s',SERIAL,*map(str,args)],timeout=70)
    return out if binary else out.decode('utf-8',errors='replace').strip()
def identity():
    dump=run('shell','dumpsys','package',PACKAGE)
    def field(pattern):
        match=re.search(pattern,dump);return match.group(1).strip() if match else None
    return dict(version=field(r'versionName=([^\r\n]+)'),code=field(r'versionCode=(\d+)'),first_install=field(r'firstInstallTime=([^\r\n]+)'),app_id=field(r'(?:appId|userId)=(\d+)'),ce_inode=field(r'ceDataInode=(\d+)'))
def nodes():
    result=run('shell','am','instrument','-w','app.tonal.automation/app.tonal.automation.Probe')
    match=re.search(r'INSTRUMENTATION_RESULT: result=(.*)',result)
    if match:return json.loads(match.group(1))
    for _ in range(4):
        result=run('shell','uiautomator','dump','/data/local/tmp/tonal-release.xml')
        if 'dumped' in result:return list(ET.fromstring(run('exec-out','cat','/data/local/tmp/tonal-release.xml')).iter('node'))
        time.sleep(.5)
    raise AssertionError('UI dump unavailable')
def click(text):
    for n in nodes():
        if text not in (n.get('text',''),n.get('content-desc','')):continue
        b=list(map(int,re.findall(r'\d+',n.get('bounds',''))));assert len(b)==4
        run('shell','input','tap',(b[0]+b[2])//2,(b[1]+b[3])//2);time.sleep(.5);return
    raise AssertionError('Missing '+text)
assert run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in run('emu','avd','name')
before=identity();assert before['version'] in ('0.3.0-local','0.4.0-local','0.5.0-local','0.6.0-local','0.7.0-local','0.14.0-local','0.15.0-local',VERSION+'-local'),before
apk=ROOT/f'out/android/Tonal-Android-{VERSION}-local.apk';assert 'Success' in run('install','-r',apk)
after=identity();assert after['version']==VERSION+'-local' and after['code']==CODE,after
assert before['first_install']==after['first_install'] and before['app_id']==after['app_id'] and before['ce_inode']==after['ce_inode'],(before,after)
(OUT/'identity-upgrade.json').write_text(json.dumps(dict(passed=True,before=before,after=after,updated_without_uninstall=True,data_directory_preserved=True),indent=2),encoding='utf-8')
run('shell','am','start','-W','--activity-reorder-to-front','-n',PACKAGE+'/app.tonal.android.HubActivity');time.sleep(1)
assert any(n.get('text')=='Tonal Hub' for n in nodes())
click('IndieTonal • outras fontes e shows ao vivo');assert any(n.get('text')=='IndieTonal' for n in nodes())
edit=next(n for n in nodes() if n.get('class','').endswith('EditText'));b=list(map(int,re.findall(r'\d+',edit.get('bounds'))));run('shell','input','tap',(b[0]+b[2])//2,(b[1]+b[3])//2);run('shell','input','keycombination','113','29');run('shell','input','text','Veruca%sSalt');click('Buscar')
deadline=time.monotonic()+60
while time.monotonic()<deadline:
    current=nodes()
    if any(n.get('class','').endswith('TextView') and n.get('text')=='Veruca Salt' for n in current) and any('MusicBrainz' in n.get('text','') for n in current):break
    time.sleep(2)
else:raise AssertionError('Release catalog did not return Veruca Salt')
(OUT/'release-search.png').write_bytes(run('exec-out','screencap','-p',binary=True))
result=dict(passed=True,apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),before=before,after=after,updated_without_uninstall=True,data_directory_preserved=True,release_hub_and_public_search=True)
(OUT/'result.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result,indent=2))
