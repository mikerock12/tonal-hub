"""Standalone authorized Tonal streaming; never an official Spotify comparison."""
from pathlib import Path
import json,os,subprocess,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/benchmark-2026-10-19/android/tonalify-streaming';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE

def snapshot():
    # A previous app process can leave a perfectly valid-looking old snapshot.
    # Remove only this test diagnostic, then wait for the current intent's write.
    ui.run('shell','run-as',P,'rm','-f','files/audio-snapshot.json')
    ui.run('shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true')
    for attempt in range(30):
        result=subprocess.run([str(ui.ADB),'-s',ui.SERIAL,'exec-out','run-as',P,'cat','files/audio-snapshot.json'],capture_output=True,timeout=10)
        if result.returncode==0:
            try:return json.loads(result.stdout.decode('utf-8'))
            except json.JSONDecodeError:pass
        time.sleep(.2)
    raise AssertionError('Current-process diagnostic snapshot unavailable')

def main():
    assert 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    for p in ['com.spotify.music','app.tonal.android','app.tonal.android.comparison']:
        ui.run('shell','am','force-stop',p)
    # Use this test app's existing authorized session. No official credentials.
    ui.run('shell','am','start','-S','-W','-n',P+'/app.tonal.android.MainActivity','--ez','streamingBenchmark','true','--es','trackUri','spotify:track:2FZ7davXrvyNYTbUEtpWbn')
    end=time.monotonic()+75
    while time.monotonic()<end:
        before=snapshot()
        if before.get('frames',0)>100000 and before.get('currentUri')=='spotify:track:2FZ7davXrvyNYTbUEtpWbn':break
        time.sleep(3)
    assert before.get('frames',0)>100000,'No verified PCM from authorized streaming session'
    assert before.get('currentUri')=='spotify:track:2FZ7davXrvyNYTbUEtpWbn','Reference track was not confirmed'
    # Use only the engine ID read from this emulator's own test app, never a
    # similarly named Connect device that could be the user's physical phone.
    sys.path.insert(0,str(ROOT/'tests/benchmark'));import windows_comparison as spotify
    device=ui.run('exec-out','run-as',P,'cat','files/librespot-oauth-v2/device-id').strip()
    spotify.control('seek',DeviceId=device,PositionMs=0);time.sleep(3)
    proofs=[];results=[]
    for r in [1,2]:
        before=snapshot()
        assert before['currentUri']=='spotify:track:2FZ7davXrvyNYTbUEtpWbn' and not before['paused']
        output=OUT/f'round{r}'
        subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tests/android/measure.py'),'--adb',str(ui.ADB),'--serial',ui.SERIAL,'--package',P,'--scenario','streaming','--no-restart','--label',f'tonalify-streaming-round{r}','--output',str(output),'--samples','6','--interval','5','--warmup','8'],check=True,timeout=210)
        after=snapshot();assert after['frames']>before['frames']
        assert after['currentUri']==before['currentUri']=='spotify:track:2FZ7davXrvyNYTbUEtpWbn'
        assert not before['paused'] and not after['paused']
        proofs.append(dict(before=before,after=after));results.append(json.loads((output/'result.json').read_text()))
    summary=dict(results=results,proofs=proofs,official_spotify_equivalent=False,variant='debuggable benchmark, Release native libraries, diagnostic UI polling every 2.5 seconds')
    (OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    ui.run('shell','am','force-stop',P)
    print('TONAL_STREAMING_0120_COMPLETE',flush=True)

if __name__=='__main__':main()
