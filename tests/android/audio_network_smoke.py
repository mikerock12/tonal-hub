"""Exercise progressive Spotify audio and network recovery on the exclusive test AVD.

Requires the authorized session in the benchmark sandbox. Never reads credentials.
Disables only emulator connectivity and restores its original Wi-Fi/data settings.
"""
from pathlib import Path
import argparse, json, subprocess, time

root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
package='app.tonal.android.benchmark'
component=package+'/app.tonal.android.MainActivity'
out=root/'out/android/audio-network-0.3.0';out.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser();parser.add_argument('--only-exhaustion',action='store_true');args=parser.parse_args()

def run(*args):
    return subprocess.check_output([str(adb),'-s','emulator-5554',*map(str,args)],text=True,encoding='utf-8',errors='replace',timeout=45).strip()

def intent(**values):
    args=['shell','am','start','-W','--activity-single-top','-n',component]
    for key,value in values.items():
        args.extend(['--ez' if isinstance(value,bool) else '--es',key,str(value).lower() if isinstance(value,bool) else str(value)])
    run(*args)

def snapshot():
    intent(audioSnapshot=True)
    return json.loads(run('exec-out','run-as',package,'cat','files/audio-snapshot.json'))

def wait_pcm(previous=0,timeout=100):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        time.sleep(2);value=snapshot()
        if value['frames']>previous+22050 and not value['paused']:return value
        if value['frames']==0 and value['engineStatus'].startswith('Falha Spotify:'):
            raise AssertionError(value['engineStatus'])
    raise AssertionError(('PCM did not resume',value))

def connect():
    for attempt in range(3):
        run('shell','am','start','-S','-W','-n',component,'--ez','streamingBenchmark','true','--es','trackUri','spotify:track:65cOtEO1eVrTezvIMDsoDr')
        try:return wait_pcm()
        except AssertionError as error:
            if attempt==2 or not any(reason in str(error) for reason in ['lookup address','client error (Connect)']):raise
            print('Initial emulator DNS unavailable after route change; retrying startup',flush=True)
            time.sleep(6)

wifi=run('shell','settings','get','global','wifi_on')=='1'
data=run('shell','settings','get','global','mobile_data')=='1'
report={'passed':False,'physical_bluetooth_tested':False}
if args.only_exhaustion and (out/'result.json').exists():
    previous=json.loads((out/'result.json').read_text(encoding='utf-8'))
    if 'short_outage' in previous:report['short_outage']=previous['short_outage']
try:
    run('shell','am','force-stop','com.spotify.music')
    run('shell','am','force-stop','app.tonal.android')
    if not args.only_exhaustion:
        if not wifi:run('shell','svc','wifi','enable')
        if data:run('shell','svc','data','disable')
        time.sleep(6)
        connect();time.sleep(10);before=snapshot()
        run('shell','svc','wifi','disable')
        samples=[];previous=before['frames']
        for _ in range(5):
            time.sleep(5);current=snapshot();samples.append(current)
            assert current['frames']>previous and not current['paused'],('short outage interrupted buffered music',current)
            assert current['underruns']==before['underruns'],('audio underrun during outage',before,current)
            previous=current['frames']
        run('shell','cmd','media_session','dispatch','pause');time.sleep(1);paused=snapshot()
        assert paused['paused'] and paused['volume']<.01,paused
        time.sleep(3);assert snapshot()['frames']==paused['frames'],'Offline pause did not stop PCM'
        run('shell','svc','wifi','enable');time.sleep(15);held=snapshot()
        assert held['paused'] and held['frames']==paused['frames'],'Reconnection overrode user pause'
        run('shell','cmd','media_session','dispatch','play');resumed=wait_pcm(held['frames'])
        report['short_outage']={'before':before,'offline_samples':samples,'paused':paused,'after_reconnect_paused':held,'resumed':resumed}
        print('PASS downloaded_audio_continues_25s_offline_and_pause_survives_reconnection',flush=True)

    # Clear only downloaded audio in our disposable benchmark package; credentials stay.
    run('shell','am','force-stop',package)
    run('shell','run-as',package,'find','files/librespot-oauth-v2/audio','-type','f','-delete')
    run('emu','network','speed','1000')
    run('shell','svc','data','disable');run('shell','svc','wifi','enable');time.sleep(6)
    initial=connect();run('shell','svc','wifi','disable')
    outage=[];last=initial;stable=0
    for _ in range(18):
        time.sleep(5);current=snapshot();outage.append(current)
        stable=stable+1 if current['frames']==last['frames'] else 0
        last=current
        if stable>=3:break
    assert stable>=3,'Throttled stream never exhausted: this run cannot validate starvation recovery'
    exhausted=snapshot()
    run('emu','network','speed','full');run('shell','svc','wifi','enable')
    recovered=wait_pcm(exhausted['frames'],timeout=110)
    assert recovered['engineStatus'] not in ('Sessão Spotify encerrada. Toque na música para reconectar.',),recovered
    assert recovered['currentUri']==initial['currentUri'],'Recovery skipped the current track'
    assert abs(recovered['positionMs']-recovered['frames']*1000/recovered['rate'])<3000,('Recovery position is not the decoded position',recovered)
    report['buffer_exhaustion']={'transport':'emulator Wi-Fi','throttled_kbps':1000,'before_outage':initial,'offline_samples':outage,'exhausted':exhausted,'automatically_resumed':recovered}
    print('PASS buffer_exhaustion_automatically_resumes_after_network_returns',flush=True)
    pid=run('shell','pidof',package)
    logs=run('logcat','-d',f'--pid={pid}','-s','TonalEngine:W','AndroidRuntime:E')
    (out/'log.txt').write_text(logs,encoding='utf-8')
    assert 'FATAL EXCEPTION' not in logs,logs
    report['passed']=True
except Exception as error:
    report['error']=str(error)
    raise
finally:
    run('emu','network','speed','full')
    run('shell','svc','wifi','enable' if wifi else 'disable')
    run('shell','svc','data','enable' if data else 'disable')
    (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    run('shell','cmd','media_session','dispatch','pause')
print('PASS network_audio_suite',flush=True)
