"""Collect aggregate process resources only; never inspect account, URI or tokens."""
import json,statistics,time
from pathlib import Path
import psutil
out=Path('live-spotify-resources.json')
def processes():
 result={}
 for p in psutil.process_iter(['pid','name','create_time']):
  try:
   if (p.info['name'] or '').lower() in {'spotify.exe','spotifywebhelper.exe'}:result[(p.pid,p.info['create_time'])]=p
  except (psutil.NoSuchProcess,psutil.AccessDenied):pass
 return result
def cpu(p):
 t=p.cpu_times();return t.user+t.system
rows=[]
try:
 time.sleep(15)
 for i in range(12):
  before=processes();start=time.monotonic();ticks={k:cpu(p) for k,p in before.items()};time.sleep(5);after=processes();elapsed=time.monotonic()-start
  if not after or set(before)!=set(after):raise RuntimeError('Spotify process set changed; incomplete measurement')
  metrics=[(cpu(p)-ticks[k],p.memory_full_info().uss) for k,p in after.items()]
  rows.append({'sample':i+1,'cpu_one_core_percent':100*sum(v[0] for v in metrics)/elapsed,'uss_bytes':sum(v[1] for v in metrics),'processes':len(metrics),'interval_seconds':elapsed})
 result={'status':'collected','scope':'Spotify named processes; 100% CPU = one core; USS private resident bytes','authentication':'not independently verified','playback':'not verified','samples':rows}
except Exception as e:result={'status':'incomplete','error':str(e),'samples':rows}
out.write_text(json.dumps(result,indent=2)+'\n')
if result['status']!='collected':raise SystemExit(1)
