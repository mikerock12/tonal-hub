import sys,time,json,re
from pathlib import Path
sys.path.insert(0,str(Path('tests/android').resolve()))
import companion_smoke as ui
ui.OUT=ui.ROOT/'out/android/catalog-polish-0.8.0';ui.OUT.mkdir(exist_ok=True,parents=True)
import os
os.environ['TONAL_FAST_UI']='1'
checks=[]
def launch(query):
 ui.run('shell','am','start','--activity-single-top','-n',ui.PACKAGE+'/app.tonal.android.MainActivity','--es','literalQuery',query)
def waitlabel(text,timeout=35):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  if any(not n.get('class','').endswith('EditText') and (n.get('text')==text or n.get('content-desc')==text) for n in ui.nodes()):return
  time.sleep(.5)
 raise AssertionError('Missing '+text)
try:
 ui.run('shell','am','force-stop',ui.PACKAGE);launch('Veruca Salt');ui.click('Artistas');waitlabel('Veruca Salt');ui.click('Veruca Salt',scroll=True);time.sleep(2);ui.screenshot('01-artist');checks.append('real_spotify_artist_page')
 launch('American Thighs');ui.click('Álbuns');waitlabel('American Thighs');ui.click('American Thighs',scroll=True);time.sleep(2);ui.screenshot('02-album');ui.click('Mostrar ou ocultar estatísticas');ui.screenshot('03-album-stats');checks.append('real_album_metadata_date_and_inline_statistics')
 ui.click('Get Back',scroll=True);end=time.monotonic()+65
 while time.monotonic()<end:
  nodes=ui.nodes()
  if any(n.get('content-desc')=='Pausar' for n in nodes):break
  time.sleep(1)
 else:raise AssertionError('Native playback did not start')
 node=next(n for n in ui.nodes() if n.get('resource-id','').endswith('/now_title'));b=list(map(int,re.findall(r'\d+',node.get('bounds'))));ui.run('shell','input','tap',(b[0]+b[2])//2,(b[1]+b[3])//2);time.sleep(3);ui.screenshot('04-playing-song-cover');checks.append('album_simplified_track_reaches_song_page_with_cover')
 ui.click('Fechar');ui.run('shell','am','start','--activity-single-top','-n',ui.PACKAGE+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true');time.sleep(1)
 audio=json.loads(ui.run('exec-out','run-as',ui.PACKAGE,'cat','files/audio-snapshot.json'));assert audio['frames']>0,audio;checks.append('actual_spotify_pcm_after_album_selection')
 result=dict(passed=True,checks=checks,audio=audio)
except Exception as e:
 ui.screenshot('failure');result=dict(passed=False,checks=checks,error=str(e));raise
finally:
 (ui.OUT/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
