"""0.13.3: changing the theme (and the launcher icon) keeps Tonalify open.

Reproduces the reported path on the dedicated AVD, without an account: open
the Tonal Hub from its launcher icon, enter Tonalify, play the local
rehearsal, pick another theme. The launcher alias must change, Tonalify must
stay the resumed screen with its audio advancing, and Back still returns to
the Hub. The default theme is restored at the end.
"""
from pathlib import Path
import json,re,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/android/theme-keep-open-0133';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE;checks=[]
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,details if details else '',flush=True)
def resumed():
    text=ui.run('shell','dumpsys','activity','activities')
    m=re.search(r'(?:mResumedActivity|topResumedActivity)[^\n]*\{[^\n]*? ([\w.]+)/([\w.]+)',text)
    return (m.group(1),m.group(2).lstrip('.')) if m else ('','')
def aliases():
    text=ui.run('shell','cmd','package','query-activities','--brief','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER')
    return sorted(set(re.findall(re.escape(P)+r'/app\.tonal\.android\.(HubIcon\w+)',text)))
def frames():
    ui.run('shell','run-as',P,'rm','-f','files/audio-snapshot.json')
    ui.run('shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true')
    for _ in range(30):
        r=subprocess.run([str(ui.ADB),'-s',ui.SERIAL,'exec-out','run-as',P,'cat','files/audio-snapshot.json'],capture_output=True,timeout=10)
        if r.returncode==0:
            try:return json.loads(r.stdout.decode('utf-8'))['frames']
            except (json.JSONDecodeError,KeyError):pass
        time.sleep(.2)
    raise AssertionError('audio snapshot unavailable')
def pick(name):
    ui.click('Conta');ui.click('Temas',scroll=True);time.sleep(.6);ui.click('Tema '+name,contains=True,scroll=True);time.sleep(3)
def launch_from_icon():
    alias=aliases()[0]
    ui.run('shell','am','start','-W','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER','-n',P+'/app.tonal.android.'+alias);time.sleep(2)
    ui.click('Tonalify • Spotify e arquivos locais');time.sleep(2)
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    ui.run('install','-r',str(ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'),timeout=180);ui.run('logcat','-c')
    version=ui.run('shell','dumpsys','package',P).split('versionName=')[1].split()[0];assert version==__import__('re').search(r"versionName '([^']+)'",(ROOT/'android/app/build.gradle').read_text()).group(1) or __import__('os').environ.get('TONAL_REPRO_OLD')=='1',version
    check('benchmark_apk_0133_installed',version=version)
    ui.run('shell','am','force-stop',P)
    if aliases()!=['HubIconTonal']:
        # A previous interrupted run may have left another theme chosen.
        ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2);pick('Tonal Azul');ui.run('shell','am','force-stop',P)
    assert aliases()==['HubIconTonal'],aliases()
    launch_from_icon();assert resumed()[1].endswith('MainActivity'),resumed()
    check('tonalify_opened_from_hub_icon')
    ui.click('Conta');ui.click('Testar áudio por 60 segundos',scroll=True);time.sleep(4)
    before=frames()
    pick('Neon')
    assert aliases()==['HubIconNeon'],aliases()
    pkg,activity=resumed();assert pkg==P and activity.endswith('MainActivity'),(pkg,activity)
    time.sleep(2);after=frames();assert after>before+44100*3,(before,after)
    check('theme_icon_changed_tonalify_stays_open',alias='HubIconNeon',resumed=activity,frames_gained=after-before)
    (OUT/'tonalify-aberto-apos-tema-0133.png').write_bytes(ui.run('exec-out','screencap','-p',binary=True))
    ui.run('shell','input','keyevent','4');time.sleep(2)
    assert resumed()[1].endswith('HubActivity'),resumed()
    check('back_returns_to_hub')
    ui.click('Tonalify • Spotify e arquivos locais');time.sleep(2)
    pick('Tonal Azul');assert aliases()==['HubIconTonal'],aliases()
    assert resumed()[1].endswith('MainActivity'),resumed()
    check('default_theme_restored_still_open')
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs[:2000]
    check('no_fatal_exception')
    ui.run('shell','am','force-stop',P)
    (OUT/'result.json').write_text(json.dumps(dict(passed=True,checks=checks,version=version),ensure_ascii=False,indent=2),encoding='utf-8')
    print('THEME_KEEP_OPEN_PASS',len(checks),'checks')
if __name__=='__main__':main()
