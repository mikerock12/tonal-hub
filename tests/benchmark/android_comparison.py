from pathlib import Path
import subprocess,os,json,time,re,sys,hashlib
ROOT=Path(__file__).resolve().parents[2];OUT=ROOT/'out/benchmark-2026-10-17/android';OUT.mkdir(parents=True,exist_ok=True)
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe';SERIAL='emulator-5560'
sys.path.insert(0,str(ROOT/'tests/android'));os.environ['TONAL_FAST_UI']='1';import companion_smoke as ui
T='app.tonal.android.comparison';S='com.spotify.music'
def adb(*a):return ui.run(*a)
def component(p):return adb('shell','cmd','package','resolve-activity','--brief','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER',p).splitlines()[-1]
def launch(p):return adb('shell','am','start','-S','-W','-n',T+'/app.tonal.android.MainActivity' if p==T else component(p))
def pause_other(p):
 for other in [T,S,'app.tonal.android.benchmark','app.tonal.android']:
  if other!=p:adb('shell','am','force-stop',other)
def measure(p,label,background=False):
 command=[sys.executable,'-X','utf8',str(ROOT/'tests/android/measure.py'),'--adb',str(ADB),'--serial',SERIAL,'--package',p,'--no-restart','--label',label,'--output',str(OUT/label),'--samples','3' if background else '6','--interval','5','--warmup','10']
 if background:command+=['--background']
 subprocess.run(command,check=True,timeout=210);return json.loads((OUT/label/'result.json').read_text())
assert 'Tonal_Local_20261015' in adb('emu','avd','name')
adb('install','-r',ROOT/'android/app/build/outputs/apk/comparison/app-comparison.apk')
# Resolve the declared test notification permissions before startup trials.
for package in [T,S]:adb('shell','pm','grant',package,'android.permission.POST_NOTIFICATIONS')
adb('shell','input','keyevent','4')
initial=[]
for p in [T,S]:
 pause_other(p);launch(p);time.sleep(2);labels=[n.get('text','') for n in ui.nodes() if n.get('text')]
 if p==T and any(x in labels for x in ['Allow','Permitir']):
  for deny in ["Don't allow",'Não permitir','Don’t allow']:
   if deny in labels:ui.click(deny);break
 labels=[n.get('text','') for n in ui.nodes() if n.get('text')]
 initial.append(dict(package=p,visible_labels=labels[:45],package_metadata=adb('shell','dumpsys','package',p)))
(OUT/'initial-state.json').write_text(json.dumps(initial,ensure_ascii=False,indent=2),encoding='utf-8')
# No application data is cleared. The comparison build has a separate fresh private profile.
results=[];startups={}
for p in [T,S]:
 pause_other(p);trials=[]
 for i in range(3):
  raw=launch(p);m=re.search(r'TotalTime:\s*(\d+)',raw);trials.append(int(m[1]) if m else None);time.sleep(2)
 startups[p]=trials
for round,order in [(1,[T,S]),(2,[S,T])]:
 for p in order:
  pause_other(p);launch(p);label=('tonalify' if p==T else 'spotify')+f'-round{round}-foreground';results.append(measure(p,label))
for p in [T,S]:
 pause_other(p);launch(p);results.append(measure(p,('tonalify' if p==T else 'spotify')+'-background',True))
pause_other(T);adb('shell','am','start','-W','-n',T+'/app.tonal.android.HubActivity');adb('shell','am','start','-W','-n',T+'/app.tonal.android.MainActivity');results.append(measure(T,'tonalify-with-hub'))
summary=dict(comparison_apk_sha256=hashlib.sha256((ROOT/'android/app/build/outputs/apk/comparison/app-comparison.apk').read_bytes()).hexdigest(),startup_ms=startups,results=results,authentication='Both baseline apps logged out; verify initial-state.json',spotify_streaming_measured=False,device='Dedicated x86_64 emulator, not the POCO X7 Pro',physical_bluetooth=False)
(OUT/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
for p in [T,S]:adb('shell','am','force-stop',p)
print('ANDROID_BENCHMARK_COMPLETE',flush=True)
