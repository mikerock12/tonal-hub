"""Additional UI regression on the packaged build: playlists, liked queue, seek and modes."""
from catalog_smoke import *
from urllib.request import Request,urlopen

def api_state(path):
    seed=json.loads((ROOT/'out/android/session-private/test-auth.json').read_text(encoding='utf-8-sig'))
    with urlopen(Request('https://api.spotify.com/v1/'+path,headers={'Authorization':'Bearer '+seed['tokens']['access_token']}),timeout=20) as response:
        return json.load(response)
def player_state():
    value=api_state("me/player");device_id=run("shell","run-as",PACKAGE,"cat","files/librespot-oauth-v2/device-id");assert value["device"]["id"]==device_id,"Playback belongs to another device";return value
def song_names():
    return [n.get('content-desc')[len('Opções de '):] for n in state() if n.get('content-desc','').startswith('Opções de ')]
try:
    run('shell','am','start','-S','-W','-n',PACKAGE+'/app.tonal.android.MainActivity','--ez','catalogTest','true')
    click('Biblioteca');wait_text('Girls Rock');click('Girls Rock');time.sleep(2)
    names=song_names();assert len(names)>=2,names
    shot('01-playlist');click(names[0]);wait_text('Pausar',timeout=65);wait_track(names[0]);assert frame_count()>0
    click('Próxima');wait_track(names[1]);check('playlist_context_and_next',first=names[0],second=names[1])
    run('shell','input','keyevent','4');click('Músicas Curtidas');time.sleep(2);names=song_names();assert len(names)>=2
    liked=api_state('me/tracks?limit=10')['items'];playable=[i.get('track') or i.get('item') for i in liked];playable=[t['name'] for t in playable if t and t.get('is_playable',True) and not t.get('is_local',False)];assert len(playable)>1;shot('02-liked');click(playable[0]);wait_track(playable[0]);time.sleep(3);click('Próxima');wait_track(playable[1]);check('liked_queue_and_next',first=playable[0],second=playable[1],unavailable_count=len(liked)-len(playable))
    tap_node(next(n for n in state() if n.get('resource-id','').endswith('/now_title')));wait_text('Tocando agora')
    seek=next(n for n in state() if n.get('class')=='android.widget.SeekBar');tap_node(seek);time.sleep(4)
    playback=player_state();position=playback['progress_ms'];duration=playback['item']['duration_ms']
    assert .40<position/duration<.65,(position,duration);check('seek_confirmed_by_spotify',position_ms=position,duration_ms=duration)
    click('Aleatório');click('Repetir');time.sleep(3);playback=player_state();assert playback['shuffle_state'] and playback['repeat_state']=='context';check('shuffle_repeat_confirmed_by_spotify')
    shot('03-player');click('Aleatório');click('Repetir');click('Fechar')
    run('shell','input','keyevent','3');time.sleep(3);run('shell','am','start','-W','-n',PACKAGE+'/app.tonal.android.MainActivity');wait_text('Pausar');check('catalog_streams_in_background')
    click('Buscar no Spotify');time.sleep(1);shot('04-search-after-background');check('search_available_after_background')
    (OUT/'result.json').write_text(json.dumps(dict(pass_all=True,version='0.2.0-local',checks=checks),indent=2),encoding='utf-8')
except Exception as error:
    shot('failure');(OUT/'result.json').write_text(json.dumps(dict(pass_all=False,checks=checks,error=str(error)),indent=2),encoding='utf-8');raise
