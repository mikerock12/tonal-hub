"""Authorized local Windows comparison. No credential is exported or logged."""
from pathlib import Path
import subprocess,psutil,json,time,statistics,os,sys,platform,shutil
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'out/benchmark-2026-10-17/windows';OUT.mkdir(parents=True,exist_ok=True)
STAGE=ROOT/'out/release-local/Tonal-Hub-0.8.0-local';TONAL=STAGE/'Tonalify.exe'
PW=shutil.which('pwsh') or r'C:\Program Files\PowerShell\7\pwsh.exe'

def ps_script(path,**args):
    source=Path(path).read_text(encoding='utf-8-sig');command='& {\n'+source+'\n}'
    for k,v in args.items():command+=' -'+k+" '"+str(v).replace("'","''")+"'"
    result=subprocess.run(['powershell.exe','-NoProfile','-Command',command],capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=40)
    if result.returncode:raise RuntimeError(result.stderr[-1800:])
    return json.loads(result.stdout.lstrip('\ufeff'))
def control(action,**args):return ps_script(ROOT/'tests/benchmark/spotify_control.ps1',Action=action,**args)
def rows(app):
    selected=[]
    for p in psutil.process_iter(['name','exe','ppid']):
        try:
            name=(p.info['name'] or '').lower();exe=(p.info['exe'] or '').lower()
            if app=='tonalify' and name=='tonalify.exe' and exe==str(TONAL).lower():selected.append(p)
            # Sandboxed Chromium children can deny exe() while exposing CPU/RAM.
            elif app=='spotify' and name.startswith('spotify'):selected.append(p)
        except (psutil.NoSuchProcess,psutil.AccessDenied):pass
    if app=='tonalify':
        extra=[]
        for p in selected:
            try:extra+=p.children(recursive=True)
            except psutil.Error:pass
        selected+=extra
    return list({p.pid:p for p in selected}.values())
def stop(app):
    for p in reversed(rows(app)):
        try:p.terminate()
        except psutil.Error:pass
    time.sleep(1)
def launch(app):
    start=time.monotonic()
    if app=='tonalify':subprocess.Popen([str(TONAL)],cwd=STAGE)
    else:subprocess.run([PW,'-NoProfile','-Command',"Start-Process 'spotify:'"],check=True,capture_output=True,timeout=15)
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
        if rows(app):return (time.monotonic()-start)*1000
        time.sleep(.1)
    raise RuntimeError(app+' did not start')
def snapshot(app):
    result={}
    for p in rows(app):
        try:
            with p.oneshot():
                t=p.cpu_times();m=p.memory_full_info();io=p.io_counters()
                result[p.pid]=dict(cpu=t.user+t.system,uss=m.uss,rss=m.rss,commit=getattr(m,'private',0),read=io.read_bytes,write=io.write_bytes)
        except psutil.NoSuchProcess:pass
        except psutil.AccessDenied as e:raise RuntimeError('Incomplete process counters for PID '+str(p.pid)) from e
    return result

def measure(app,scenario,round):
    before=snapshot(app);samples=[];last=time.monotonic()
    for n in range(30):
        time.sleep(1);after=snapshot(app);now=time.monotonic();elapsed=now-last;shared=before.keys()&after.keys()
        if not after:raise RuntimeError('App exited during measurement')
        samples.append(dict(at=n+1,elapsed=elapsed,cpu_one_core_percent=100*sum(max(0,after[p]['cpu']-before[p]['cpu']) for p in shared)/elapsed,cpu_machine_percent=100*sum(max(0,after[p]['cpu']-before[p]['cpu']) for p in shared)/elapsed/psutil.cpu_count(),private_working_set_mib=sum(v['uss'] for v in after.values())/1048576,summed_working_set_mib=sum(v['rss'] for v in after.values())/1048576,private_commit_mib=sum(v['commit'] for v in after.values())/1048576,read_bytes=sum(max(0,after[p]['read']-before[p]['read']) for p in shared),write_bytes=sum(max(0,after[p]['write']-before[p]['write']) for p in shared),pids=list(after),process_count=len(after),process_churn=set(before)!=set(after)))
        before=after;last=now
    result=dict(app=app,scenario=scenario,round=round,samples=samples,median_private_working_set_mib=statistics.median(x['private_working_set_mib'] for x in samples),median_cpu_one_core_percent=statistics.median(x['cpu_one_core_percent'] for x in samples),mean_cpu_one_core_percent=statistics.mean(x['cpu_one_core_percent'] for x in samples))
    (OUT/f'{app}-{scenario}-{round}.json').write_text(json.dumps(result,indent=2));print(app,scenario,round,round_number(result['median_private_working_set_mib']),round_number(result['mean_cpu_one_core_percent']),flush=True);return result

def round_number(x):return float(f'{x:.3f}')
def gpu(app):return ps_script(ROOT/'tests/benchmark/windows_gpu.ps1',Ids=','.join(str(p.pid) for p in rows(app)))
def audio():
    value=ps_script(ROOT/'tests/benchmark/windows_audio.ps1')
    return value[0] if isinstance(value,list) else value
def media(**args):return ps_script(ROOT/'tests/benchmark/windows_media.ps1',**args)

def start_track(app,targets,track):
    # Bootstrap the reference track once through the official URI handler.
    # Transfer its valid loaded context; a synthetic single-URI play context
    # was not handled consistently by both clients in the preparation run.
    control('transfer',DeviceId=targets[app]['id']);time.sleep(2)
    control('seek',DeviceId=targets[app]['id'],PositionMs=30000)
    state=control('state')
    if not state['playing']:
        try:control('resume',DeviceId=targets[app]['id'])
        except RuntimeError:
            # A completed asynchronous resume can reject a redundant play.
            if not control('state')['playing']:raise
    assert control('state')['uri']==track,'Loaded track changed; bootstrap it again'
def main():
    assert not any('qemu-system' in (p.info['name'] or '').lower() for p in psutil.process_iter(['name'])),'Emulator must be stopped before Windows measurements'
    # Close the obsolete 0.7 instance started during preparation, preserving its stored data.
    for p in psutil.process_iter(['name','exe']):
        if (p.info['name'] or '').lower()=='tonalify.exe' and 'Tonal-Hub-0.7.0-local'.lower() in (p.info['exe'] or '').lower():
            for c in p.children(recursive=True):
                try:c.terminate()
                except psutil.Error:pass
            p.terminate()
    if '--prepared' not in sys.argv:
        stop('tonalify');stop('spotify');launch('tonalify');launch('spotify');time.sleep(12)
    devices=control('devices');devices=devices if isinstance(devices,list) else [devices]
    (OUT/'devices.json').write_text(json.dumps(devices,indent=2));state=control('state');track=state.get('uri')
    if not track:track=json.loads((ROOT/'out/android/catalog-polish-0.8.0/result.json').read_text())['audio']['currentUri']
    tonal=next(d for d in devices if d['name'].startswith('Tonal @'))
    official=next(d for d in devices if d['type']=='Computer' and not d['name'].startswith('Tonal'))
    targets={'tonalify':tonal,'spotify':official};results=[];proof=[];startups={}
    for r,order in [(1,['tonalify','spotify']),(2,['spotify','tonalify'])]:
        for app in order:
            start_track(app,targets,track);time.sleep(8)
            audible=False
            for attempt in range(6):
                a=audio();ids={p.pid for p in rows(app)}
                if any(x['pid'] in ids and x['peak']>0 for x in a['sessions']):audible=True;break
                time.sleep(2)
            s=control('state');proof.append(dict(app=app,round=r,state=s,audio=a,gpu_engines=gpu(app)));assert s['deviceId']==targets[app]['id'] and s['playing'],s
            assert audible,('No measured PCM output for '+app,a)
            (OUT/'audio-proof.json').write_text(json.dumps(proof,indent=2))
            results.append(measure(app,'playing-background',r))
            end_state=control('state');end_audio=audio();ids={p.pid for p in rows(app)}
            assert end_state['deviceId']==targets[app]['id'] and end_state['playing'] and end_state['uri']==track,end_state
            assert any(x['pid'] in ids and x['peak']>0 for x in end_audio['sessions']),('PCM output stopped during measurement',app)
            proof.append(dict(app=app,round=r,phase='after',state=end_state,audio=end_audio,gpu_engines=[]))
            (OUT/'audio-proof.json').write_text(json.dumps(proof,indent=2))
            control('pause',DeviceId=targets[app]['id']);time.sleep(6);results.append(measure(app,'paused-background',r))
    for app in ['tonalify','spotify']:
        trials=[]
        for _ in range(3):stop(app);trials.append(launch(app));time.sleep(5)
        startups[app]=dict(process_visible_in_enumeration_ms=trials,note='Process creation detection, not a ready UI or true cold OS startup')
    hw=dict(inventory=ps_script(ROOT/'tests/benchmark/windows_hardware.ps1'),os=platform.platform(),logical_cores=psutil.cpu_count(),physical_cores=psutil.cpu_count(logical=False),ram_bytes=psutil.virtual_memory().total)
    report=dict(hardware=hw,results=results,proof=proof,startup=startups,track=track,notes=['All app-owned process CPU and private working sets summed, including librespot for Tonalify.','100% CPU means one core; machine percentage also recorded.','Private working set is resident unique RAM; summed RSS includes shared pages and is not used as primary RAM.','Process I/O is logical I/O, not physical disk or isolated network traffic.','No emulator/build/CI/compression ran during measurement.','Scenario names contain background for historical reasons; window visibility was not enforced by the collector.'])
    (OUT/'summary.json').write_text(json.dumps(report,indent=2));print('WINDOWS_BENCHMARK_COMPLETE',flush=True)
if __name__=='__main__':main()
