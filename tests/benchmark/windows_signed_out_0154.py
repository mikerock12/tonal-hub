"""Fresh Windows VM: exploratory signed-out UI baseline; no Spotify session."""
import ctypes,json,os,statistics,subprocess,time
from pathlib import Path
import psutil
from PIL import ImageGrab
out=Path('evidence').resolve(); tonal=out/'portable'/'Tonalify.exe'; spotify=Path(os.environ['APPDATA'])/'Spotify'/'Spotify.exe'
apps={'tonalify':{'exe':tonal,'names':{'tonalify.exe','tonalhub.exe','librespot.exe'}},'spotify':{'exe':spotify,'names':{'spotify.exe','spotifywebhelper.exe'}}}

def alive(app):
    found={}
    for p in psutil.process_iter(['pid','name','create_time']):
        try:
            if (p.info['name'] or '').lower() in apps[app]['names']:
                found[(p.pid,p.info['create_time'])]=p
        except (psutil.NoSuchProcess,psutil.AccessDenied):pass
    return found

def stop():
    for app in apps:
        for p in alive(app).values():
            try:p.kill()
            except (psutil.NoSuchProcess,psutil.AccessDenied):pass
    time.sleep(3)

def cpu(p):
    t=p.cpu_times();return t.user+t.system

def windows(pids):
    user32=ctypes.windll.user32; handles=[]
    @ctypes.WINFUNCTYPE(ctypes.c_bool,ctypes.c_void_p,ctypes.c_void_p)
    def callback(hwnd,lparam):
        pid=ctypes.c_ulong(); user32.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
        if pid.value in pids and user32.IsWindowVisible(hwnd):handles.append(int(hwnd))
        return True
    user32.EnumWindows(callback,0);return handles

def capture(app,roundno):
    try:ImageGrab.grab().save(out/f'{app}-round{roundno}.png')
    except Exception:pass

rows=[]
try:
 for roundno,order in [(1,['tonalify','spotify']),(2,['spotify','tonalify'])]:
  for app in order:
   stop();subprocess.Popen([str(apps[app]['exe'])],cwd=str(apps[app]['exe'].parent));time.sleep(30)
   processes=alive(app);wins=windows({p.pid for p in processes.values()})
   if not processes or not wins:raise RuntimeError(f'{app}: no running process or visible window; no comparable UI sample')
   # Window capture is retained for visual validation; do not treat title as proof of UI state.
   capture(app,roundno); samples=[]; attempts=0
   while len(samples)<6 and attempts<10:
    attempts+=1; before=alive(app); t0=time.monotonic(); counters={}
    for key,p in before.items():
     try:counters[key]=cpu(p)
     except (psutil.NoSuchProcess,psutil.AccessDenied):pass
    time.sleep(5);after=alive(app);elapsed=time.monotonic()-t0
    if set(before)!=set(after) or set(counters)!=set(after):continue
    try:
     values=[(cpu(p)-counters[key],p.memory_full_info().uss) for key,p in after.items()]
    except (psutil.NoSuchProcess,psutil.AccessDenied):continue
    if not windows({p.pid for p in after.values()}):raise RuntimeError(f'{app}: visible window disappeared')
    samples.append({'cpu_one_core_percent':100*sum(x[0] for x in values)/elapsed,'uss_bytes':sum(x[1] for x in values),'processes':len(values),'interval_seconds':elapsed})
   if len(samples)!=6:raise RuntimeError(f'{app}: not enough stable process samples')
   rows.append({'app':app,'round':roundno,'attempts':attempts,'samples':samples})
   (out/'windows-signed-out-partial.json').write_text(json.dumps({'rounds':rows},indent=2))
 result={'release':'0.15.4','scenario':'Fresh VM, signed out, visible window, no music; 30 s warm-up; 6 x 5 s, two reversed rounds','cpu_definition':'100% = one full core; package process times summed','ram_definition':'sum of per-process USS (private resident working set); includes named helpers','limits':'Exploratory baseline, screen contents and visibility must be verified; not authenticated streaming or physical machine; no hardware GPU utilization or power measurement','rounds':rows}
 (out/'windows-signed-out-0154.json').write_text(json.dumps(result,indent=2)+'\n')
finally:stop()
