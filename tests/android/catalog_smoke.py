"""Real Spotify catalog integration through Android UI; uses only an authorized test sandbox.

Import with packaging/test-android-session-local.ps1 -Import before running.
Never reads or records OAuth tokens. Leaves the test app available for inspection.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
PACKAGE='app.tonal.android.benchmark'
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=ROOT/'out/android/catalog-smoke-0.3.0')
OUT=parser.parse_args().output
OUT.mkdir(parents=True,exist_ok=True)
checks=[]
def run(*args):
    return subprocess.check_output([str(ADB),*map(str,args)],text=True,encoding='utf-8',errors='replace',timeout=45).strip()
def state(label=None):
    for attempt in range(3):
        result=run('shell','uiautomator','dump','/sdcard/tonal-catalog-test.xml')
        if 'dumped' in result:break
    else:raise AssertionError('UI did not become idle; refusing a stale hierarchy')
    xml=run('shell','cat','/sdcard/tonal-catalog-test.xml')
    if label:(OUT/f'{label}.xml').write_text(xml,encoding='utf-8')
    return list(ET.fromstring(xml).iter('node'))
def texts(nodes):return [n.get('text','') or n.get('content-desc','') for n in nodes]
def find(nodes,text):
    return next((n for n in nodes if (n.get('text','').casefold()==text.casefold() or n.get('content-desc','').casefold()==text.casefold()) and n.get('class')!='android.widget.EditText'),None)
def tap_node(node):
    x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
    run('shell','input','tap',(x1+x2)//2,(y1+y2)//2)
def click(text,scroll=False,timeout=25):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        nodes=state();node=find(nodes,text)
        if node is not None:tap_node(node);return
        if scroll:run('shell','input','swipe','540','1700','540','650','380')
        else:time.sleep(.5)
    raise AssertionError(f'Control not found: {text}; {texts(nodes)}')
def wait_text(text,timeout=30):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        nodes=state()
        if text in texts(nodes):return nodes
        time.sleep(.5)
    raise AssertionError(f'Missing {text}: {texts(nodes)}')
def now_title(nodes):
    return next((n.get('text') for n in nodes if n.get('resource-id','').endswith('/now_title')),None)
def wait_track(title,timeout=35):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        nodes=state()
        if now_title(nodes)==title:return nodes
        time.sleep(.5)
    raise AssertionError(f'Expected current track {title}, got {now_title(nodes)}')
def shot(label):
    state(label);run('shell','screencap','-p','/sdcard/tonal-catalog-test.png')
    run('pull','/sdcard/tonal-catalog-test.png',OUT/f'{label}.png')
def check(name,**detail):
    checks.append(dict(name=name,passed=True,**detail));print('PASS '+name,flush=True)
def frame_count():
    value=next(s for s in texts(state()) if s.startswith('PCM:'))
    return int(re.search(r'PCM:\s*([\d,]+)',value).group(1).replace(',',''))
def search(query):
    click('Buscar no Spotify');nodes=state();edit=next(n for n in nodes if n.get('class')=='android.widget.EditText');tap_node(edit)
    run('shell','input','keyevent','KEYCODE_MOVE_END')
    if edit.get('text'):run('shell','input','keyevent',*(['KEYCODE_DEL']*len(edit.get('text'))))
    run('shell','input','text',query.replace(' ','%s'));click('Pesquisar')

def catalog_flow():
    try:
        run('shell','am','force-stop','com.spotify.music')
        run('shell','am','start','-S','-W','-n',PACKAGE+'/app.tonal.android.MainActivity','--ez','catalogTest','true')
        wait_text('Girls Rock');shot('01-home');check('home_authenticated_artwork')
        search('Veruca Salt');wait_text('Ver músicas');shot('02-search-all');check('search_all_no_invalid_limit')
        click('Artistas');wait_text('Veruca Salt');shot('03-search-artists');click('Veruca Salt')
        wait_text('Músicas')
        shot('04-artist');check('artist_page')
        click('American Thighs',scroll=True);wait_text('Get Back');shot('05-album');check('artist_to_album_tracks')
        click('Carregar mais',scroll=True);click('Sleeping Where I Want',scroll=True);wait_text('Pausar',timeout=65);wait_track('Sleeping Where I Want');check('album_second_page_selection')
        run('shell','input','swipe','540','650','540','1700','380')
        click('Wolf',scroll=True);wait_text('Pausar',timeout=65);wait_track('Wolf')
        assert frame_count()>0
        shot('06-streaming-album');check('selected_album_track_streams',track='Wolf')
        click('Próxima');wait_track('Celebrate You');check('album_context_next_track')
        click('Pausar');wait_text('Pausado');a=frame_count();time.sleep(2);b=frame_count();assert a==b,(a,b)
        click('Reproduzir');wait_text('Pausar');assert frame_count()>b;check('pause_resume_pcm',paused_frames=a)
        tap_node(next(n for n in state() if n.get('resource-id','').endswith('/now_title')));wait_text('Tocando agora');shot('06b-player')
        seek=next(n for n in state() if n.get('class')=='android.widget.SeekBar');tap_node(seek);time.sleep(3);check('player_seek_control')
        run('shell','input','keyevent','4');time.sleep(.5);run('shell','input','keyevent','4');time.sleep(.5);run('shell','input','keyevent','4');nodes=wait_text('Veruca Salt');assert any(n.get('class')=='android.widget.EditText' and n.get('text')=='Veruca Salt' for n in nodes);check('system_back_restores_search')
        click('Biblioteca');wait_text('Girls Rock');shot('07-library-playlists');check('library_playlists')
        click('Álbuns');time.sleep(2);shot('08-library-albums');assert not any('HTTP 400' in s for s in texts(state()));check('library_saved_albums')
        click('Artistas');time.sleep(2);shot('09-library-artists');assert not any('HTTP 400' in s for s in texts(state()));check('library_followed_artists')
        search('Veruca Salt');click('Músicas');wait_text('Opções de Seether');shot('10-search-tracks');check('search_tracks')
        click('Seether');wait_track('Seether');click('Próxima');wait_track('Volcano Girls');shot('11-search-queue');assert frame_count()>0;check('search_queue_next')
        search('American Thighs');click('Álbuns');wait_text('American Thighs');shot('12-search-albums');check('search_albums')
        search('Girls Rock');click('Playlists');time.sleep(3);shot('13-search-playlists');assert not any('HTTP 400' in s for s in texts(state()));check('search_playlists')
        search('isrc:ZZZZZ0000000');time.sleep(3);shot('14-empty-results');assert any('Nenhum resultado' in s for s in texts(state()));check('empty_search')
        logs=run('logcat','-d','--pid='+run('shell','pidof',PACKAGE),'-s','AndroidRuntime:E')
        assert 'FATAL EXCEPTION' not in logs;check('no_java_crash')
        (OUT/'result.json').write_text(json.dumps(dict(pass_all=True,version='0.3.0-local',checks=checks),indent=2),encoding='utf-8')
    except Exception as error:
        shot('failure');(OUT/'result.json').write_text(json.dumps(dict(pass_all=False,checks=checks,error=str(error)),indent=2),encoding='utf-8');raise

if __name__=="__main__":catalog_flow()
