"""Native seasonal song page and rotation checks on the dedicated AVD only."""
from pathlib import Path
import json,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/playlist'))
import android_ui as ui
OUT=ROOT/'out/android/seasonal-0157';OUT.mkdir(parents=True,exist_ok=True)

def labels():
    return [n.get('text') or n.get('content-desc','') for n in ui.dump() if n.get('text') or n.get('content-desc')]
def wait_labels(predicate,timeout=25):
    current=[]
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:current=labels()
        except RuntimeError:time.sleep(.4);continue
        if predicate(current):return current
        time.sleep(.4)
    raise AssertionError('Labels not ready: '+str(current)[:1200])
def shot(name):(OUT/(name+'.png')).write_bytes(ui.adb('exec-out','screencap','-p',binary=True))
def rotation(value):ui.adb('shell','settings','put','system','user_rotation',str(value));time.sleep(2.5)
def main():
    assert ui.adb('shell','getprop','ro.kernel.qemu').strip()=='1' and 'Tonal_Local_20261015' in ui.adb('emu','avd','name')
    package=ui.PKG
    ui.adb('logcat','-c')
    previous=ui.adb('shell','settings','get','system','user_rotation').strip()
    checks=[]
    try:
        ui.adb('shell','settings','put','system','accelerometer_rotation','0')
        ui.adb('shell','am','force-stop',package)
        rotation(0)
        ui.adb('shell','am','start','-W','-n',package+'/app.tonal.android.MainActivity','--ez','songPageFixture','true','--es','themeFixture','natal','--es','fixtureBackground','Natal')
        portrait=wait_labels(lambda text:'Reproduzir' in text and 'Atualizar letra' not in text)
        assert 'Buscar no Spotify' in portrait and 'Conta' in portrait and 'Tonal Hub' in portrait
        checks.append('portrait_player_and_standard_header');shot('portrait-natal')
        rotation(1)
        landscape=wait_labels(lambda text:'Reproduzir' in text and 'Atualizar letra' in text)
        assert 'Buscar no Spotify' in landscape and 'Conta' in landscape and 'Tonal Hub' in landscape
        nodes=ui.dump();play=next(n for n in nodes if n.get('content-desc')=='Reproduzir');lyric=next(n for n in nodes if n.get('content-desc')=='Atualizar letra')
        px=list(map(int,__import__('re').findall(r'\d+',play.get('bounds'))))[0];lx=list(map(int,__import__('re').findall(r'\d+',lyric.get('bounds'))))[0]
        assert px<lx,(play.get('bounds'),lyric.get('bounds'))
        checks.append('landscape_player_left_lyrics_right_with_header');shot('landscape-natal')
        rotation(0)
        wait_labels(lambda text:'Reproduzir' in text and 'Atualizar letra' not in text)
        checks.append('portrait_page_restored_after_rotation');shot('portrait-restored')
        assert 'FATAL EXCEPTION' not in ui.adb('logcat','-d','-s','AndroidRuntime:E')
        checks.append('no_fatal_exception')
        report={'passed':True,'checks':checks,'synthetic':True,'physicalPhone':False,'liveSpotify':False}
        (OUT/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
        print(json.dumps(report))
    except Exception as error:
        (OUT/'result.json').write_text(json.dumps({'passed':False,'checks':checks,'error':str(error)},indent=2),encoding='utf-8');raise
    finally:
        ui.adb('shell','settings','put','system','user_rotation',previous or '0')
        ui.adb('shell','am','force-stop',package)
if __name__=='__main__':main()
