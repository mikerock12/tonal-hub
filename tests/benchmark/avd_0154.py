"""KVM AVD baseline: release APK, signed out; no comparison to Spotify."""
import json,os,re,statistics,subprocess,sys,time,xml.etree.ElementTree as ET
from pathlib import Path
out=Path('evidence');tmp=Path('/tmp/tonal-avd-benchmark');tmp.mkdir(exist_ok=True)
adb=Path(os.environ['ANDROID_HOME'])/'platform-tools/adb';pkg='app.tonal.android';serial='emulator-5554'
def cmd(*args):return subprocess.run([str(adb),'-s',serial,*map(str,args)],capture_output=True,text=True,check=True,timeout=45).stdout
def tap(label):
    cmd('shell','uiautomator','dump','/sdcard/window.xml'); raw=cmd('exec-out','cat','/sdcard/window.xml')
    for n in ET.fromstring(raw).iter('node'):
        if label in n.get('text','') or label in n.get('content-desc',''):
            b=list(map(int,re.findall(r'\d+',n.get('bounds',''))))
            if len(b)==4:cmd('shell','input','tap',(b[0]+b[2])//2,(b[1]+b[3])//2);return
    raise RuntimeError('Target not found: '+label)
def prepare(which):
    cmd('shell','am','force-stop',pkg)
    cmd('shell','am','start','-W','-n',pkg+'/.HubActivity');time.sleep(3)
    if which=='tonalify':cmd('shell','am','start','-W','-n',pkg+'/.MainActivity')
    else:tap('IndieTonal')
    time.sleep(4)
    top=cmd('shell','dumpsys','activity','activities')
    expected='MainActivity' if which=='tonalify' else 'IndieActivity'
    if expected not in top:raise RuntimeError('Target activity not visible: '+expected)
def main():
    cmd('install','-r',out/'Tonal-Android-0.15.4-local.apk')
    cmd('shell','pm','grant',pkg,'android.permission.POST_NOTIFICATIONS')
    rows=[]
    for round_num,order in [(1,['tonalify','indietonal']),(2,['indietonal','tonalify'])]:
        for which in order:
            prepare(which); target=tmp/f'{which}-{round_num}'
            subprocess.run([sys.executable,'tests/android/measure.py','--adb',str(adb),'--serial',serial,'--package',pkg,'--no-restart','--scenario','idle','--label',which,'--output',str(target),'--samples','6','--interval','5','--warmup','15'],check=True,timeout=180)
            data=json.loads((target/'result.json').read_text())
            rows.append({'app':which,'round':round_num,'profile':data['profile'],'samples':[{'pss_kib':s['pss_kib'],'swap_kib':s['swap_kib'],'cpu_one_core_percent':s['cpu_one_core_percent'],'processes':s['processes'],'interval_seconds':s['interval_seconds']} for s in data['samples']]})
    summary={'release':'0.15.4','environment':'GitHub-hosted Ubuntu VM, Android 35 x86_64 AVD, KVM; SwiftShader software rendering','gpu':'No CUDA GPU measured','scenario':'Signed-out foreground idle; Hub and player package processes included; 15 s warmup; 6 x 5 s samples in two reversed rounds','limits':'No Spotify measurement, no playback, no battery or physical-device inference; process memory is PSS; CPU 100% = one core','rounds':rows}
    (out/'android-0154-baseline.json').write_text(json.dumps(summary,indent=2)+'\n')
if __name__=='__main__':main()
