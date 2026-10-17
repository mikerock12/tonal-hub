"""Check the real authenticated catalog at 360x640 dp, restoring this AVD afterwards."""
from catalog_smoke import *
original=dict(size=run('shell','wm','size'),density=run('shell','wm','density'))
assert 'Override' not in original['size'] and 'Override' not in original['density'],original
try:
    run('shell','wm','size','720x1280');run('shell','wm','density','320')
    run('shell','am','start','-S','-W','-n',PACKAGE+'/app.tonal.android.MainActivity')
    wait_text('Girls Rock');shot('01-home-360dp');check('home_360dp')
    search('The Breeders');click('Artistas');wait_text('The Breeders');shot('02-artists-360dp');click('The Breeders');wait_text('Músicas');shot('03-artist-360dp');check('search_artist_360dp')
    labels=texts(state());assert not any(s.startswith('PCM:') for s in labels)
    assert find(state(),'Reproduzir') is not None and find(state(),'Biblioteca') is not None;check('compact_transport_visible_without_debug_counters')
    run('shell','input','keyevent','4');nodes=state();assert any(n.get('class')=='android.widget.EditText' and n.get('text')=='The Breeders' for n in nodes);check('back_preserves_query_360dp')
    (OUT/'result.json').write_text(json.dumps(dict(pass_all=True,version='0.2.0-local',viewport_dp=[360,640],checks=checks),indent=2),encoding='utf-8')
except Exception as error:
    shot('failure');(OUT/'result.json').write_text(json.dumps(dict(pass_all=False,checks=checks,error=str(error)),indent=2),encoding='utf-8');raise
finally:
    run('shell','wm','size','reset');run('shell','wm','density','reset')
