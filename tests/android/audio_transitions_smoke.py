"""Verify pause envelopes with each actual output buffer on the final benchmark APK."""
from pathlib import Path
import json, subprocess, time
root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
package='app.tonal.android.benchmark';component=package+'/app.tonal.android.MainActivity'
out=root/'out/android/audio-transitions-0.3.0';out.mkdir(parents=True,exist_ok=True)
def run(*args):return subprocess.check_output([str(adb),'-s','emulator-5554',*map(str,args)],text=True,encoding='utf-8',errors='replace',timeout=45).strip()
def intent(**values):
    cmd=['shell','am','start','-W','--activity-single-top','-n',component]
    for key,value in values.items():cmd.extend(['--ez' if isinstance(value,bool) else '--ei',key,str(value).lower() if isinstance(value,bool) else str(value)])
    run(*cmd)
def snapshot():
    intent(audioSnapshot=True);return json.loads(run('exec-out','run-as',package,'cat','files/audio-snapshot.json'))
run('shell','am','force-stop','app.tonal.android')
run('shell','am','start','-S','-W','-n',component,'--ez','streamingBenchmark','true','--es','trackUri','spotify:track:65cOtEO1eVrTezvIMDsoDr')
for _ in range(45):
    time.sleep(2);first=snapshot()
    if first['frames']>44100 and not first['paused']:break
else:raise AssertionError(('No Spotify PCM',first))
cycles=[]
for mode in [0,2,1]:
    intent(audioTestSettings=True,audioPreset=0,audioHrtf=False,audioBuffer=mode,audioBypass=False);time.sleep(1)
    for _ in range(2):
        before=snapshot()
        run('shell','cmd','media_session','dispatch','pause');time.sleep(.15);paused=snapshot()
        deadline=time.monotonic()+2
        while (not paused['paused'] or paused['volume']>=.01) and time.monotonic()<deadline:
            time.sleep(.1);paused=snapshot()
        assert paused['paused'] and paused['volume']<.01,paused
        assert snapshot()['frames']==paused['frames'],'Paused PCM advanced'
        run('shell','cmd','media_session','dispatch','play');time.sleep(.3);resumed=snapshot()
        deadline=time.monotonic()+2
        while (resumed['paused'] or resumed['volume']<=.99 or resumed['frames']<=paused['frames']) and time.monotonic()<deadline:
            time.sleep(.1);resumed=snapshot()
        assert not resumed['paused'] and resumed['volume']>.99 and resumed['frames']>paused['frames'],resumed
        assert resumed['underruns']==before['underruns'],('Pause drained buffer before fade completed',before,paused,resumed)
        cycles.append(dict(mode=mode,before=before,paused=paused,resumed=resumed))
sizes={mode:next(c['before']['bufferFrames'] for c in cycles if c['mode']==mode) for mode in [0,1,2]}
assert sizes[1]>sizes[0]>sizes[2],('AVD buffer profiles did not change the effective buffer',sizes)
report=dict(passed=True,audible_clicks_tested=False,buffer_frames=sizes,cycles=cycles)
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS six_pause_resume_cycles_all_buffer_profiles_no_new_underruns',flush=True)
run('shell','cmd','media_session','dispatch','pause')
