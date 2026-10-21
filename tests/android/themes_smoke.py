"""0.13.0 themes on the dedicated AVD: pick themes while local test audio plays.

No account is used: the bundled Vorbis rehearsal plays through the same
AudioTrack/DSP path. Changing the theme must rebuild only the views; PCM keeps
advancing without new underruns and the playing item stays the same.
"""
from pathlib import Path
import json,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/android/themes-0130';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE;checks=[]
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,details if details else '',flush=True)
def screenshot(name):(OUT/(name+'.png')).write_bytes(ui.run('exec-out','screencap','-p',binary=True))
def main_activity(**extras):
    args=['shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity']
    for k,v in extras.items():args+=['--ez',k,'true' if v else 'false']
    ui.run(*args)
def audio():
    ui.run('shell','run-as',P,'rm','-f','files/audio-snapshot.json');main_activity(audioSnapshot=True)
    for _ in range(30):
        r=subprocess.run([str(ui.ADB),'-s',ui.SERIAL,'exec-out','run-as',P,'cat','files/audio-snapshot.json'],capture_output=True,timeout=10)
        if r.returncode==0:
            try:return json.loads(r.stdout.decode('utf-8'))
            except json.JSONDecodeError:pass
        time.sleep(.2)
    raise AssertionError('audio snapshot unavailable')
def rehearsal():
    ui.click('Conta');ui.click('Testar áudio por 60 segundos',scroll=True)
    end=time.monotonic()+20
    while time.monotonic()<end:
        value=audio()
        if value['frames']>44100 and not value.get('paused'):return value
        time.sleep(1)
    raise AssertionError('rehearsal audio did not start: '+json.dumps(value)[:400])
def shown_title():
    return next((n.get('text','') for n in ui.nodes() if n.get('resource-id','').endswith('now_title')),None)
def chosen():return ui.run('exec-out','run-as',P,'cat','files/theme.txt').strip()
def pick(name):
    ui.click('Conta');ui.click('Temas',scroll=True);time.sleep(.6)
    ui.click('Tema '+name,contains=True,scroll=True);time.sleep(.8)
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    apk=ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'
    ui.run('install','-r',str(apk),timeout=180);ui.run('logcat','-c')
    ui.run('shell','am','force-stop',P);ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2)
    version=ui.run('shell','dumpsys','package',P).split('versionName=')[1].split()[0];assert version.startswith('0.13.'),version
    check('benchmark_apk_0130_installed',version=version)
    before=rehearsal()
    check('local_audio_playing',frames=before['frames'])
    ui.click('Conta');ui.click('Temas',scroll=True);time.sleep(.8);screenshot('android-temas-0130')
    names=[n.get('content-desc','') for n in ui.nodes() if n.get('content-desc','').startswith('Tema ')]
    ui.click('Tema Aurora',contains=True,scroll=True);time.sleep(1.5)
    after=audio();assert chosen()=='aurora',chosen()
    gained=after['frames']-before['frames']
    assert gained>44100*2,f'PCM stalled during theme change: {gained} frames'
    assert after['underruns']==before['underruns'],f"new underruns {before['underruns']}->{after['underruns']}"
    check('theme_change_keeps_audio',theme='aurora',frames_gained=gained,underruns=after['underruns'],themes_listed=len(names))
    screenshot('android-tema-aurora-0130')
    for name,expected in [('Pôr do sol','solar'),('Neon','neon'),('Oceano','oceano'),('Grafite suave','grafite')]:
        # The rehearsal lasts 60 s; its end-of-stream drain is not a theme effect.
        start=audio()
        if start['frames']-before['frames']>44100*35:
            before=start=rehearsal()
        title=shown_title();pick(name);end=audio();assert chosen()==expected,chosen()
        assert shown_title()==title,('mini player title changed',title,shown_title())
        assert end['frames']>start['frames'] and end['underruns']==start['underruns'],(name,start['frames'],end['frames'],end['underruns'])
        check('theme_'+expected+'_applied_audio_continues',frames_gained=end['frames']-start['frames'])
        screenshot('android-tema-'+expected+'-0130')
    pick('Tonal Azul');assert chosen()=='tonal';check('default_theme_restored')
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs[:2000]
    check('no_fatal_exception')
    (OUT/'result.json').write_text(json.dumps(dict(passed=True,checks=checks,version=version),ensure_ascii=False,indent=2),encoding='utf-8')
    print('THEMES_ANDROID_PASS',len(checks),'checks')
if __name__=='__main__':main()
