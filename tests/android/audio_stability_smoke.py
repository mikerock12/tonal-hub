"""Real Spotify PCM, process CPU/PSS, live DSP transitions and injected AudioTrack recovery.

Requires the user's authorized credentials in the benchmark sandbox only. No token is read.
Emulator output is not evidence of physical Bluetooth latency or audible quality.
"""
from pathlib import Path
import json, re, subprocess, time

root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
package='app.tonal.android.benchmark'
component=package+'/app.tonal.android.MainActivity'
out=root/'out/android/audio-stability-0.3.0';out.mkdir(parents=True,exist_ok=True)
def run(*args):return subprocess.check_output([str(adb),*map(str,args)],text=True,encoding='utf-8',errors='replace',timeout=45).strip()
def intent(**values):
    args=['shell','am','start','-W','--activity-single-top','-n',component]
    for key,value in values.items():args.extend(['--ez' if isinstance(value,bool) else '--ei',key,str(value).lower() if isinstance(value,bool) else str(value)])
    run(*args)
def snapshot():
    intent(audioSnapshot=True)
    return json.loads(run('exec-out','run-as',package,'cat','files/audio-snapshot.json'))
def settings(preset=0,hrtf=False,buffer=0,bypass=False):
    intent(audioTestSettings=True,audioPreset=preset,audioHrtf=hrtf,audioBuffer=buffer,audioBypass=bypass)
def cpu(pid):
    text=run('exec-out','run-as',package,'cat',f'/proc/{pid}/stat')
    fields=text[text.rfind(')')+2:].split();return int(fields[11])+int(fields[12])
def pss():
    text=run('shell','dumpsys','meminfo',package)
    match=re.search(r'TOTAL PSS:\s*(\d+)',text)
    if not match:match=re.search(r'^\s*TOTAL\s+(\d+)',text,re.M)
    assert match,text
    return int(match.group(1))
def phase(name,preset,hrtf,buffer):
    settings(preset,hrtf,buffer);time.sleep(3)
    before=snapshot();start_cpu=cpu(pid);start=time.monotonic();samples=[];previous=before['frames']
    for i in range(5):
        time.sleep(5);current=snapshot();assert current['frames']>previous,(name,'PCM stalled',current);previous=current['frames'];samples.append(current)
    elapsed=time.monotonic()-start;end_cpu=cpu(pid);after=samples[-1]
    result=dict(name=name,seconds=round(elapsed,2),cpu_percent_one_core=round((end_cpu-start_cpu)/ticks/elapsed*100,2),pss_kib=pss(),
                average_dsp_us=round((after['dspNanos']-before['dspNanos'])/max(1,after['blocks']-before['blocks'])/1000,2),
                underruns_during_phase=after['underruns']-before['underruns'],slow_blocks_during_phase=after['overBudgetBlocks']-before['overBudgetBlocks'],
                before=before,after=after,samples=samples)
    print(json.dumps({k:v for k,v in result.items() if k not in ('samples','before','after')},ensure_ascii=False),flush=True)
    return result
run('shell','am','force-stop','com.spotify.music')
run('shell','am','force-stop','app.tonal.android')
run('shell','am','start','-S','-W','-n',component,'--ez','streamingBenchmark','true','--es','trackUri','spotify:track:65cOtEO1eVrTezvIMDsoDr')
deadline=time.monotonic()+90
while time.monotonic()<deadline:
    time.sleep(2);s=snapshot()
    if s['frames']>44100 and not s['paused']:break
else:raise AssertionError('Spotify PCM did not start with the authorized cached session')
pid=run('shell','pidof',package)
ticks=int(run('shell','getconf','CLK_TCK'))
phases=[phase('flat_stereo',0,False,0),phase('rock_eq_and_bass',1,False,0),phase('rock_eq_bass_hrtf',1,True,0)]
require_playing=snapshot();assert require_playing['volume']>.99,require_playing
pauses=[]
for i in range(6):
    run('shell','cmd','media_session','dispatch','pause');time.sleep(.4);paused=snapshot();assert paused['paused'] and paused['volume']<.01,paused
    held=snapshot();assert held['frames']==paused['frames'],(paused,held)
    run('shell','cmd','media_session','dispatch','play');time.sleep(.6);playing=snapshot();assert not playing['paused'] and playing['volume']>.99 and playing['frames']>held['frames'],playing
    pauses.append(dict(paused=paused,resumed=playing))
print('PASS repeated_fades_pause_resume',flush=True)
before_recovery=snapshot();intent(audioRecover=True);time.sleep(3);after_recovery=snapshot()
assert after_recovery['recoveries']==before_recovery['recoveries']+1 and after_recovery['frames']>before_recovery['frames'],after_recovery
print('PASS injected_dead_object_recovery',flush=True)
settings(1,True,1);time.sleep(2);stable=snapshot();settings(1,True,2);time.sleep(2);small=snapshot();assert stable['bufferFrames']>=small['bufferFrames'],(stable,small)
settings(0,False,0,True);time.sleep(2);bypass=snapshot();assert not bypass['enabled'] and not bypass['hrtfRequested']
settings(0,False,0);time.sleep(2)
before_bg=snapshot();run('shell','input','keyevent','3');time.sleep(8)
intent(audioSnapshot=True);background=json.loads(run('exec-out','run-as',package,'cat','files/audio-snapshot.json'));assert background['frames']>before_bg['frames'],background
logs=run('logcat','-d',f'--pid={pid}','-s','AndroidRuntime:E','TonalTest:I','AudioTrack:W')
(out/'log.txt').write_text(logs,encoding='utf-8');assert 'FATAL EXCEPTION' not in logs,logs
report=dict(passed=True,version='0.3.0-local',physical_bluetooth_tested=False,audible_clicks_tested=False,
            cpu_unit='percent of one logical CPU, /proc process ticks / elapsed wall time',clock_ticks=ticks,
            phases=phases,pause_resume_cycles=pauses,injected_recovery=dict(before=before_recovery,after=after_recovery),
            stable_buffer=stable,smaller_buffer=small,bypass=bypass,background=background)
(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('PASS audio_stability_suite',flush=True)
run('shell','cmd','media_session','dispatch','pause')
