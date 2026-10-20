"""Two reversed rounds on the dedicated AVD, signed-out baseline only.

No account data is cleared. Inspect initial-state.json before using these results.
Raw accessibility/account information stays in ignored out/.
"""
from pathlib import Path
import hashlib,json,os,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'out/benchmark-2026-10-19/android';OUT.mkdir(parents=True,exist_ok=True)
sys.path.insert(0,str(ROOT/'tests/android'));os.environ['TONAL_FAST_UI']='1'
import companion_smoke as ui
T='app.tonal.android.comparison';S='com.spotify.music'

def stop_others(target):
    for package in [T,S,'app.tonal.android','app.tonal.android.benchmark']:
        if package!=target:ui.run('shell','am','force-stop',package)

def launch(package):
    if package==T:
        ui.run('shell','am','start','-S','-W','-n',T+'/app.tonal.android.HubActivity')
        return ui.run('shell','am','start','-W','-n',T+'/app.tonal.android.MainActivity')
    component=ui.run('shell','cmd','package','resolve-activity','--brief','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER',package).splitlines()[-1]
    return ui.run('shell','am','start','-S','-W','-n',component)

def main():
    assert 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    initial=[]
    for p in [T,S]:
        stop_others(p);launch(p);time.sleep(8)
        initial.append(dict(package=p,labels=[n.get('text','') for n in ui.nodes() if n.get('text')]))
    (OUT/'initial-state.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2),encoding='utf-8')
    # Refuse to silently compare an authenticated home screen against a login screen.
    assert 'Entrar com Spotify' in initial[0]['labels'],initial[0]['labels']
    assert any('Log in' in t or 'Entrar' in t or 'Continue with' in t for t in initial[1]['labels']),initial[1]['labels']
    results=[]
    for round_number,order in [(1,[T,S]),(2,[S,T])]:
        for p in order:
            for background in [False,True]:
                stop_others(p);launch(p)
                label=('tonalify' if p==T else 'spotify')+f'-round{round_number}-'+('background' if background else 'foreground')
                cmd=[sys.executable,'-X','utf8',str(ROOT/'tests/android/measure.py'),'--adb',str(ui.ADB),'--serial',ui.SERIAL,'--package',p,'--no-restart','--label',label,'--output',str(OUT/label),'--samples','6','--interval','5','--warmup','12']
                if background:cmd+=['--background']
                subprocess.run(cmd,check=True,timeout=240)
                r=json.loads((OUT/label/'result.json').read_text());r['round']=round_number;results.append(r)
                (OUT/'progress.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    apk=ROOT/'android/app/build/outputs/apk/comparison/app-comparison.apk'
    summary=dict(results=results,comparison_apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),authentication='Both signed out; initial-state labels inspected',spotify_streaming_measured=False,tonal_hub_included=True,meminfo=ui.run('shell','cat','/proc/meminfo'))
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    for p in [T,S]:ui.run('shell','am','force-stop',p)
    print('ANDROID_0120_COMPLETE',flush=True)

if __name__=='__main__':main()
