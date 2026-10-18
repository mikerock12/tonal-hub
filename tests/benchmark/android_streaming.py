from pathlib import Path
import subprocess,json,time,sys
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/benchmark-2026-10-17/android/tonalify-real-streaming';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE
ui.run('shell','am','force-stop','app.tonal.android')
ui.run('shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity');time.sleep(4)
def snapshot():
 ui.run('shell','am','start','--activity-single-top','-n',P+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true');time.sleep(.3)
 return json.loads(ui.run('exec-out','run-as',P,'cat','files/audio-snapshot.json'))
before=snapshot();assert before['frames']>0,before
subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tests/android/measure.py'),'--adb',str(ui.ADB),'--serial',ui.SERIAL,'--package',P,'--scenario','streaming','--no-restart','--label','tonalify-real-streaming','--output',str(OUT),'--samples','6','--interval','5','--warmup','10'],check=True,timeout=180)
after=snapshot();assert after['frames']>before['frames'],(before,after)
(OUT/'pcm-proof.json').write_text(json.dumps(dict(before=before,after=after,official_spotify_equivalent_streaming_comparison=False,variant='Separate debuggable benchmark app, same release native libraries'),indent=2),encoding='utf-8')
ui.run('shell','am','force-stop',P)
print('TONAL_REAL_STREAMING_MEASURED',flush=True)
