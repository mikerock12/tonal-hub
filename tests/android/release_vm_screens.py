"""Install the public universal APK in a fresh AVD and capture real screens."""
import json, re, subprocess, time, xml.etree.ElementTree as ET
from pathlib import Path
out=Path('evidence-android')
def adb(*args): return subprocess.run(['adb',*map(str,args)],check=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE).stdout
def shot(name): (out/(name+'.png')).write_bytes(adb('exec-out','screencap','-p'))
def nodes():
    adb('shell','uiautomator','dump','/sdcard/window.xml')
    raw=adb('exec-out','cat','/sdcard/window.xml'); (out/'last-ui.xml').write_bytes(raw)
    return list(ET.fromstring(raw).iter('node'))
def tap(label,exact=False):
    for n in nodes():
        values=[n.get('text',''),n.get('content-desc','')]
        if any((v==label if exact else label in v) for v in values):
            box=list(map(int,re.findall(r'\d+',n.get('bounds',''))))
            if len(box)==4:
                adb('shell','input','tap',(box[0]+box[2])//2,(box[1]+box[3])//2);time.sleep(3);return True
    return False
checks=[]
try:
    adb('install','-r',out/'Tonal-Android-0.15.4-local.apk');checks.append('apk_installed')
    adb('shell','am','start','-n','app.tonal.android/.HubActivity');time.sleep(8);shot('android-hub-0154');checks.append('hub_opened')
    adb('shell','am','start','-n','app.tonal.android/.MainActivity');time.sleep(12)
    for label in ('Permitir','Allow','While using the app'):
        if tap(label): break
    shot('android-tonalify-0154');checks.append('tonalify_opened')
    if not tap('Conta'): raise RuntimeError('Conta não encontrado')
    if not tap('Temas'): raise RuntimeError('Temas não encontrado')
    shot('android-themes-0154');checks.append('theme_picker_opened')
    found=False
    for _ in range(5):
        if tap('Halloween'):found=True;break
        adb('shell','input','swipe',500,1600,500,550,450);time.sleep(2)
    if not found:raise RuntimeError('Halloween não encontrado')
    time.sleep(4);shot('android-halloween-0154');checks.append('halloween_selected')
    adb('shell','am','start','-n','app.tonal.android/.HubActivity');time.sleep(3)
    if not tap('IndieTonal'):raise RuntimeError('IndieTonal não encontrado')
    time.sleep(8);shot('android-indie-0154');checks.append('indie_opened')
    (out/'results.json').write_text(json.dumps({'passed':True,'checks':checks,'spotify_login':False},indent=2))
except Exception as e:
    try:shot('android-failure');nodes()
    except Exception:pass
    (out/'results.json').write_text(json.dumps({'passed':False,'checks':checks,'error':str(e)},indent=2))
    raise
