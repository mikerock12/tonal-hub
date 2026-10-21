"""0.13.1 song page controls on the dedicated AVD, without an account.

Opens the song page during the bundled local rehearsal, checks that the
transport row is Aleatório, Anterior, Tocar, Próxima, Repetir with Shake,
Descoberta and Fila below, cycles Repetir through its three states and checks
that the choice is saved and restored after a restart. Audio keeps playing.
"""
from pathlib import Path
import json,re,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/android/repeat-0131';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE;checks=[]
STATES=['Repetir a fila','Repetindo a fila: repetir só esta música','Repetindo esta música: desligar repetição']
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,details if details else '',flush=True)
def screenshot(name):(OUT/(name+'.png')).write_bytes(ui.run('exec-out','screencap','-p',binary=True))
def bounds(node):return list(map(int,re.findall(r'\d+',node.get('bounds',''))))
def by_desc(nodes,text):return next((n for n in nodes if n.get('content-desc','')==text),None)
def saved_mode():
    xml=ui.run('exec-out','run-as',P,'cat','shared_prefs/settings.xml')
    m=re.search(r'name="repeatMode" value="(\d+)"',xml);return int(m.group(1)) if m else 0
def audio_frames():
    ui.run('shell','run-as',P,'rm','-f','files/audio-snapshot.json')
    ui.run('shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true')
    for _ in range(30):
        r=subprocess.run([str(ui.ADB),'-s',ui.SERIAL,'exec-out','run-as',P,'cat','files/audio-snapshot.json'],capture_output=True,timeout=10)
        if r.returncode==0:
            try:return json.loads(r.stdout.decode('utf-8'))['frames']
            except (json.JSONDecodeError,KeyError):pass
        time.sleep(.2)
    raise AssertionError('audio snapshot unavailable')
def open_song_page():
    ui.click('Conta');ui.click('Testar áudio por 60 segundos',scroll=True);time.sleep(3)
    ui.click('Ensaio de áudio Vorbis',contains=True);time.sleep(2)
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    ui.run('install','-r',str(ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'),timeout=180);ui.run('logcat','-c')
    ui.run('shell','am','force-stop',P);ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2)
    version=ui.run('shell','dumpsys','package',P).split('versionName=')[1].split()[0];assert version.startswith('0.13.'),version
    check('benchmark_apk_0131_installed',version=version)
    open_song_page();nodes=ui.nodes()
    row=[by_desc(nodes,d) for d in ['Reprodução aleatória da fila','Anterior','Pausar','Próxima']]
    repeat=next((n for n in nodes if n.get('content-desc','') in STATES),None)
    assert all(n is not None for n in row) and repeat is not None,[n.get('content-desc') for n in nodes if n.get('content-desc')]
    row.append(repeat);centers=[((b[0]+b[2])//2,(b[1]+b[3])//2) for b in map(bounds,row)]
    assert all(abs(y-centers[0][1])<40 for _,y in centers) and [x for x,_ in centers]==sorted(x for x,_ in centers),centers
    below=[by_desc(nodes,d) for d in ['Shake: embaralhar a fila','Descoberta 2+1 da fila','Ver fila de reprodução']]
    assert all(n is not None for n in below) and all(bounds(n)[1]>centers[0][1] for n in below)
    check('controls_in_order',transport=['random','prev','play','next','repeat'],below=['shake','discover','queue'])
    screenshot('android-controles-repeticao-0131')
    start=STATES.index(repeat.get('content-desc'))
    for step in range(1,4):
        ui.click(STATES[(start+step-1)%3]);time.sleep(.5)
        expected=(start+step)%3
        assert by_desc(ui.nodes(),STATES[expected]) is not None,STATES[expected]
        assert saved_mode()==expected,(saved_mode(),expected)
        check('repeat_state_'+str(expected),description=STATES[expected])
        if expected==2:screenshot('android-repetir-faixa-0131')
    ui.click(STATES[start]);time.sleep(.5)
    target=(start+1)%3
    if saved_mode()!=target:ui.click(STATES[saved_mode()]);time.sleep(.5)
    before=audio_frames()
    ui.run('shell','am','force-stop',P);ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2)
    open_song_page()
    assert by_desc(ui.nodes(),STATES[saved_mode()]) is not None,'repeat choice not restored after restart'
    check('repeat_restored_after_restart',mode=saved_mode())
    f1=audio_frames();time.sleep(2);f2=audio_frames();assert f2>f1,'rehearsal audio stopped'
    check('audio_keeps_playing',frames_gained=f2-f1)
    while saved_mode()!=0:ui.click(STATES[saved_mode()]);time.sleep(.5)
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs[:2000]
    check('no_fatal_exception')
    ui.run('shell','am','force-stop',P)
    (OUT/'result.json').write_text(json.dumps(dict(passed=True,checks=checks,version=version),ensure_ascii=False,indent=2),encoding='utf-8')
    print('REPEAT_ANDROID_PASS',len(checks),'checks')
if __name__=='__main__':main()
