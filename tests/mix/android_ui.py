"""Native Android toolbar/order UI plus the real shared C++ JNI planner. Dedicated test package only."""
from pathlib import Path
import importlib.util,json,time,hashlib,xml.etree.ElementTree as ET
spec=importlib.util.spec_from_file_location('tonal_test_ui',Path(__file__).parents[1]/'playlist/android_ui.py');ui=importlib.util.module_from_spec(spec);spec.loader.exec_module(ui)
ui.OUT=ui.ROOT/'out/mix-android-ui';ui.OUT.mkdir(parents=True,exist_ok=True)

def find(label,click=False,timeout=35):
 end=time.monotonic()+timeout
 while time.monotonic()<end:
  try:nodes=ui.dump()
  except (RuntimeError,ET.ParseError):time.sleep(.3);continue
  for n in nodes:
   if label.casefold() in [n.get('text','').casefold(),n.get('content-desc','').casefold()] and n.get('enabled')=='true' and (not click or n.get('clickable')=='true'):return n
  time.sleep(.3)
 raise AssertionError('Native control missing: '+label)
def click(label):ui.tap(find(label,True));time.sleep(.3)
def header():
 labels={n.get('content-desc','') for n in ui.dump()}
 required=['Buscar no Spotify','Biblioteca pessoal, estatísticas e letras','Áudio','Conta','Agora','Biblioteca','Estatísticas','Letras','Backup da biblioteca','Fechar página da música']
 assert all(label in labels for label in required),[label for label in required if label not in labels]
def songs():return [n.get('text') for n in ui.dump() if n.get('text','').startswith('Mix Fixture ')]

def main():
 assert ui.SERIAL=='emulator-5560' and ui.PKG.endswith('.benchmark')
 ui.adb('shell','am','force-stop',ui.PKG);ui.adb('shell','pm','clear',ui.PKG);ui.adb('shell','pm','grant',ui.PKG,'android.permission.POST_NOTIFICATIONS')
 ui.adb('shell','am','start','-n',ui.PKG+'/app.tonal.android.MainActivity','--ez','mixUiFixture','true')
 find('Agora');header();ui.screen('01-song-header.png');click('Biblioteca');header();before=songs();assert before
 ui.screen('02-library-original.png');click('Embaralhar lista / restaurar ordem');shaken=songs();assert shaken and shaken!=before,(before,shaken)
 ui.screen('03-library-shaken.png');click('Embaralhar lista / restaurar ordem');restored=songs();assert restored==before,(before,restored)
 click('Reprodução aleatória');click('Descoberta 2+1');ui.screen('04-modes.png');header()
 click('Estatísticas');find('Seu ritmo');header();ui.screen('05-statistics-header.png');click('Letras');header();ui.screen('06-lyrics-header.png')
 click('Backup da biblioteca');find('Exportar');ui.adb('shell','input','keyevent','4');click('Fechar página da música');find('Buscar no Spotify');click('Biblioteca pessoal, estatísticas e letras');find('Biblioteca')
 proof=json.loads(ui.adb('exec-out','cat',f'/sdcard/Android/data/{ui.PKG}/files/listening-mix-tests.json'));assert proof['native_cpp_via_jni'] and proof['real_spotify_writes']==0
 sequence=proof['sequence'];assert len(sequence)==9 and sequence[:2]==[0,1] and all(sequence[n]<0 for n in [2,5,8]) and len(set(proof['shaken_order']))==6
 report={'passed':True,'native_header_present_across_song_library_statistics_lyrics':True,'six_icon_navigation_controls':True,'temporary_shake_restores_visible_original_order':True,'random_and_discovery_controls_work':True,'native_cpp_jni':proof,'real_spotify_writes':0,'physical_phone':False,'screenshots':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in ui.OUT.glob('*.png')}}
 (ui.OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False));ui.adb('shell','am','force-stop',ui.PKG)
if __name__=='__main__':main()
