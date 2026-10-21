"""0.13.2 media notification and themed launcher icon on the dedicated AVD.

No account: Tonalify plays the bundled local rehearsal, IndieTonal plays a
public Aadam Jacobs recording (archive.org cover). Checks the MediaSession
metadata/position, the notification title, artist and cover, and that the
launcher alias follows the chosen theme (restored to Tonal Azul at the end).
"""
from pathlib import Path
import json,re,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/android/notification-0132';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE;checks=[]
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,details if details else '',flush=True)
def screenshot(name):(OUT/(name+'.png')).write_bytes(ui.run('exec-out','screencap','-p',binary=True))
def session_block(tag):
    text=ui.run('shell','dumpsys','media_session',timeout=30)
    # Session header at 4 spaces, its fields at 6 spaces.
    m=re.search(r'\n    '+re.escape(tag)+' '+re.escape(P)+r'/[^\n]*\n((?:      [^\n]*\n)+)',text.replace('\r',''))
    return m.group(1) if m else ''
def notification_block(channel):
    text=ui.run('shell','dumpsys','notification','--noredact',timeout=30)
    for block in re.split(r'\n\s+NotificationRecord\(',text):
        if 'pkg='+P in block and channel in block:return block
    return ''
def launcher_aliases():
    text=ui.run('shell','cmd','package','query-activities','--brief','-a','android.intent.action.MAIN','-c','android.intent.category.LAUNCHER')
    return sorted(set(re.findall(re.escape(P)+r'/app\.tonal\.android\.(HubIcon\w+)',text)))
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    ui.run('install','-r',str(ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'),timeout=180);ui.run('logcat','-c')
    version=ui.run('shell','dumpsys','package',P).split('versionName=')[1].split()[0];assert version.startswith('0.13.'),version
    check('benchmark_apk_0132_installed',version=version)
    ui.run('shell','am','force-stop',P);ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2)
    ui.click('Conta');ui.click('Testar áudio por 60 segundos',scroll=True);time.sleep(6)
    block=session_block('Tonalify')
    assert 'Ensaio de áudio Vorbis' in block,block[:1500]
    # The system extrapolates position from "updated" while speed is 1.0.
    state=re.search(r'state=PlaybackState \{state=PLAYING\(3\), position=(\d+), buffered position=\d+, speed=1\.0, updated=(\d+)',block)
    assert state,block[:1500]
    check('tonalify_session_metadata_and_playing_clock',title='Ensaio de áudio Vorbis',position_ms=int(state.group(1)),updated=int(state.group(2)))
    note=notification_block('playback')
    assert 'android.title=String (Ensaio de áudio Vorbis)' in note or 'android.title=Ensaio de áudio Vorbis' in note,note[:2000]
    check('tonalify_notification_shows_track')
    ui.run('shell','cmd','statusbar','expand-notifications');time.sleep(2);screenshot('notificacao-tonalify-0132');ui.run('shell','cmd','statusbar','collapse')
    # Themed launcher icon: alias follows the chosen theme, one entry at a time.
    assert launcher_aliases()==['HubIconTonal'],launcher_aliases()
    ui.click('Conta');ui.click('Temas',scroll=True);time.sleep(.6);ui.click('Tema Neon',contains=True,scroll=True);time.sleep(2)
    assert launcher_aliases()==['HubIconNeon'],launcher_aliases()
    check('launcher_icon_follows_theme',alias='HubIconNeon')
    ui.click('Conta');ui.click('Temas',scroll=True);time.sleep(.6);ui.click('Tema Tonal Azul',contains=True,scroll=True);time.sleep(2)
    assert launcher_aliases()==['HubIconTonal'],launcher_aliases()
    check('launcher_icon_restored',alias='HubIconTonal')
    ui.run('shell','am','force-stop',P)
    # IndieTonal: public recording with a cover image.
    ui.launch(literalQuery='L7');time.sleep(1)
    ui.click('Fonte de catálogo');ui.click('Aadam Jacobs')
    ui.wait(lambda x:any(i['id']=='ajc02607_l7-1989-09-14' for i in x['items']))
    ui.click('L7 Live at Dreamerz 1989-09-14',scroll=True)
    data=ui.wait(lambda x:any(i['playable'] and i['kind']=='track' for i in x['items']))
    first=next(i for i in data['items'] if i['playable'])
    ui.click('Reproduzir '+first['title'],scroll=True)
    ui.wait(lambda x:x['playing'] and x['positionMs']>1800)
    end=time.monotonic()+30;note=''
    while time.monotonic()<end:
        note=notification_block('indie-playback')
        if 'android.largeIcon' in note and 'null' not in re.search(r'android\.largeIcon=([^\n]*)',note).group(1):break
        time.sleep(2)
    assert first['title'] in note,note[:2000]
    large=re.search(r'android\.largeIcon=([^\n]*)',note)
    assert large and 'null' not in large.group(1),note[:2000]
    check('indietonal_notification_with_cover',title=first['title'],large_icon=large.group(1)[:60])
    ui.run('shell','cmd','statusbar','expand-notifications');time.sleep(2);screenshot('notificacao-indietonal-capa-0132');ui.run('shell','cmd','statusbar','collapse')
    ui.click('Parar motor de áudio')
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs[:2000]
    check('no_fatal_exception')
    for p in [P]:ui.run('shell','am','force-stop',p)
    (OUT/'result.json').write_text(json.dumps(dict(passed=True,checks=checks,version=version),ensure_ascii=False,indent=2),encoding='utf-8')
    print('NOTIFICATION_ANDROID_PASS',len(checks),'checks')
if __name__=='__main__':main()
