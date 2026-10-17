"""Song page, automatic lyrics and language detection on the dedicated benchmark AVD."""
import json,time
from pathlib import Path
import companion_smoke as ui

ui.OUT=ui.ROOT/'out/android/song-page-0.6.0'
ui.OUT.mkdir(parents=True,exist_ok=True)
checks=[]
def check(name,**details):
    checks.append(dict(name=name,passed=True,**details));print(name,flush=True)

try:
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1'
    assert 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    ui.run('shell','am','force-stop',ui.PACKAGE)
    ui.launch(songUiFixture=True,fixtureTitle='Get Back')
    data=ui.wait(lambda s:s['library']['lyricLines']>0,60)
    assert data['library']['currentTitle']=='Get Back'
    check('song_page_fetches_lyrics_without_pressing_search',lines=data['library']['lyricLines'])
    ui.screenshot('01-song-page')
    before=data['library']['favorites']
    label='Remover das favoritas' if any(n.get('content-desc')=='Remover das favoritas' for n in ui.nodes()) else 'Favoritar música'
    ui.click(label)
    assert ui.snapshot()['library']['favorites']!=before
    check('song_page_heart_updates_favorites')
    ui.click('Letras')
    data=ui.wait(lambda s:s['library']['translatedLines']>0,60)
    check('automatic_source_language_translation',translated_lines=data['library']['translatedLines'])
    ui.screenshot('02-lyrics-translation')
    ui.launch(songUiFixture=True,fixtureTitle='Number One Blind',openSong=False)
    data=ui.wait(lambda s:s['library']['currentTitle']=='Number One Blind' and s['library']['lyricLines']>0,60)
    check('open_page_follows_next_track_and_fetches_lyrics',lines=data['library']['lyricLines'])
    ui.screenshot('03-next-track-lyrics')
    ui.click('Estatísticas')
    assert any(n.get('text')=='Seu ritmo' for n in ui.nodes())
    ui.screenshot('04-statistics')
    check('statistics_cards_visible')
    ui.click('Agora')
    assert any(n.get('text')=='Number One Blind' for n in ui.nodes())
    ui.screenshot('05-next-song-page')
    check('song_page_metadata_follows_current_track')
    report=dict(passed=True,checks=checks,spotify_authenticated=False,physical_bluetooth=False)
except Exception as error:
    report=dict(passed=False,checks=checks,error=str(error))
    raise
finally:
    (ui.OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
