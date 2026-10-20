"""CPU/RAM collection for installed 0.12.0; private raw output, sanitized export later.

Uses the existing authorized Tonal OAuth/media/Core Audio probes. Does not export
credentials or synthesize GUI input. Both apps remain open; only one plays.
"""
from pathlib import Path
import ctypes,hashlib,json,os,psutil,statistics as st,time,subprocess
import windows_comparison as probe

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'out/benchmark-2026-10-19/windows';OUT.mkdir(parents=True,exist_ok=True)
probe.TONAL=Path(os.environ['LOCALAPPDATA'])/'Programs/Tonal Hub/Tonalify.exe'
probe.STAGE=probe.TONAL.parent;probe.OUT=OUT
TRACK='spotify:track:2FZ7davXrvyNYTbUEtpWbn'

def start_track(app,targets):
    # A paused Connect endpoint can lose its loaded context. Resume the native
    # official transport first, then transfer that real context to the target.
    probe.media(Match='Spotify',Action='play')
    time.sleep(3)
    assert probe.control('state').get('uri')==TRACK,'Official reference context unavailable'
    probe.start_track(app,targets,TRACK)

def save_report(results,proofs):
    config=Path(os.environ['LOCALAPPDATA'])/'Tonal/Tonal/audio.json'
    report=dict(version='0.12.0',track=TRACK,results=results,proofs=proofs,
      hardware=probe.ps_script(ROOT/'tests/benchmark/windows_hardware.ps1'),logical_cores=psutil.cpu_count(),ram_bytes=psutil.virtual_memory().total,
      tonal_exe_sha256=hashlib.sha256(probe.TONAL.read_bytes()).hexdigest(),tonal_audio_settings=json.loads(config.read_text(encoding='utf-8-sig')),
      visibility_controlled=False,spotify_audio_quality_configuration='not verified',scope='Authenticated same-track playback and paused; existing personal DSP configuration; no acoustic or battery measurement')
    (OUT/'summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')

def windows(app):
    """Read-only window state, never a UI automation fallback."""
    ids={p.pid for p in probe.rows(app)};out=[];u=ctypes.windll.user32
    u.GetForegroundWindow.restype=ctypes.c_void_p;foreground=u.GetForegroundWindow()
    callback=ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
    def visit(hwnd,_):
        pid=ctypes.c_ulong();u.GetWindowThreadProcessId(ctypes.c_void_p(hwnd),ctypes.byref(pid))
        if pid.value in ids and u.IsWindowVisible(ctypes.c_void_p(hwnd)):
            rect=(ctypes.c_long*4)();u.GetWindowRect(ctypes.c_void_p(hwnd),ctypes.byref(rect))
            out.append(dict(minimized=bool(u.IsIconic(ctypes.c_void_p(hwnd))),foreground=hwnd==foreground,width=rect[2]-rect[0],height=rect[3]-rect[1]))
        return True
    u.EnumWindows(callback(visit),0);return out

def measure(app,scenario,round_number,seconds=60):
    before=probe.snapshot(app);previous=time.monotonic();samples=[];psutil.cpu_percent(None)
    for i in range(seconds):
        time.sleep(1);after=probe.snapshot(app);now=time.monotonic();elapsed=now-previous;shared=before.keys()&after.keys()
        assert after,'Target app exited'
        cpu=100*sum(max(0,after[p]['cpu']-before[p]['cpu']) for p in shared)/elapsed
        samples.append(dict(sample=i+1,seconds=elapsed,cpu_one_core_percent=cpu,cpu_machine_percent=cpu/psutil.cpu_count(),
          uss_mib=sum(v['uss'] for v in after.values())/1048576,rss_sum_mib=sum(v['rss'] for v in after.values())/1048576,
          private_commit_mib=sum(v['commit'] for v in after.values())/1048576,processes=len(after),
          read_bytes=sum(max(0,after[p]['read']-before[p]['read']) for p in shared),write_bytes=sum(max(0,after[p]['write']-before[p]['write']) for p in shared),
          process_churn=before.keys()!=after.keys(),host_cpu_percent=psutil.cpu_percent(None),host_available_ram_mib=psutil.virtual_memory().available/1048576))
        before=after;previous=now
    result=dict(app=app,scenario=scenario,round=round_number,samples=samples,windows=windows(app))
    (OUT/f'{app}-{scenario}-{round_number}.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(dict(app=app,scenario=scenario,round=round_number,uss_mib=st.median(s['uss_mib'] for s in samples),cpu_mean=st.mean(s['cpu_one_core_percent'] for s in samples))),flush=True)
    return result

def proof(app,target,phase,round_number):
    state=probe.control('state');audio=probe.audio();ids={p.pid for p in probe.rows(app)}
    peak=max((s['peak'] for s in audio['sessions'] if s['pid'] in ids),default=0)
    assert state['uri']==TRACK and state['deviceId']==target['id'] and state['playing'], 'Playback mismatch'
    assert peak>0,'No nonzero Core Audio output for target processes'
    return dict(app=app,phase=phase,round=round_number,state=state,target_peak=peak,windows=windows(app))

def main():
    assert not any('qemu' in (p.info['name'] or '').lower() for p in psutil.process_iter(['name'])),'Stop emulator first'
    devices=probe.control('devices');devices=devices if isinstance(devices,list) else [devices]
    targets={'tonalify':next(d for d in devices if d['name'].startswith('Tonal @')),
             'spotify':next(d for d in devices if d['type']=='Computer' and not d['name'].startswith('Tonal'))}
    assert probe.rows('tonalify') and probe.rows('spotify')
    # Load the reference into the official client, then transfer its real context.
    if probe.control('state').get('uri')!=TRACK:
        # The official client needs a real loaded context after a cold launch.
        # This is its documented OS URI handler, not simulated GUI input.
        subprocess.run(['powershell.exe','-NoProfile','-Command',"Start-Process '"+TRACK+"' -WindowStyle Hidden"],check=True,capture_output=True,timeout=15)
        time.sleep(6)
    assert probe.control('state')['uri']==TRACK
    results=[];proofs=[]
    for round_number,order in [(1,['tonalify','spotify']),(2,['spotify','tonalify'])]:
        for app in order:
            start_track(app,targets);time.sleep(12)
            proofs.append(proof(app,targets[app],'before',round_number))
            results.append(measure(app,'playing',round_number))
            proofs.append(proof(app,targets[app],'after',round_number))
            probe.control('pause',DeviceId=targets[app]['id']);time.sleep(8)
            assert not probe.control('state')['playing']
            results.append(measure(app,'paused',round_number))
            (OUT/'progress.json').write_text(json.dumps(dict(results=results,proofs=proofs),indent=2),encoding='utf-8')
    save_report(results,proofs);print('WINDOWS_0120_COMPLETE',flush=True)

if __name__=='__main__':main()
