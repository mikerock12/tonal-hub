"""Native 0.15.1 song pages and sharesheet smoke, dedicated AVD + synthetic data.
Never sends a message, creates a remote playlist or enters account credentials.
"""
from pathlib import Path
import json,os,re,sys,time
os.environ['TONAL_FAST_UI']='1'
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'))
import companion_smoke as ui
OUT=ROOT/'out/android/song-0151';OUT.mkdir(parents=True,exist_ok=True)
checks=[]
def check(name,condition=True):
    assert condition,name
    checks.append(name);print(name,flush=True)
def labels():
    for attempt in range(4):
        try:return [n.get('text') or n.get('content-desc') for n in ui.nodes() if n.get('text') or n.get('content-desc')]
        except AssertionError:
            if attempt==3:raise
            time.sleep(1)
def shot(name):(OUT/(name+'.png')).write_bytes(ui.run('exec-out','screencap','-p',binary=True))
def launch(**extras):
    ui.run('shell','am','force-stop',ui.PACKAGE)
    args=['shell','am','start','-W','-f','0x10008000','-n',ui.PACKAGE+'/app.tonal.android.MainActivity']
    for k,v in extras.items():args.extend(['--ez',k,'true' if v else 'false'])
    ui.run(*args);time.sleep(3)
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    try:
        launch(songPageFixture=True)
        text=labels();check('simplified_player_header','Ir para o início' in text and 'Agora' not in text and 'Fechar página da música' not in text);check('hero_contains_controls_without_adjacent_lyrics','Reproduzir' in text and 'Atualizar letra' not in text)
        nodes=ui.nodes();control=next(n for n in nodes if n.get('text','').startswith('+ Adicionar'))
        bottom=int(re.findall(r'\d+',control['bounds'])[3]);check('hero_bottom_action_inside_viewport',bottom<1200);shot('music')
        ui.run('shell','input','swipe',270,950,270,400,300);time.sleep(.8)
        text=labels();check('swipe_snaps_to_lyrics','Atualizar letra' in text and 'Reproduzir' not in text);shot('lyrics')
        ui.click('Expandir letras em tela cheia');time.sleep(.5)
        check('fullscreen_controls_initially_hidden','Atualizar letra' not in labels())
        ui.run('shell','input','tap',270,180);time.sleep(.5)
        check('fullscreen_reveal_has_refresh','Atualizar letra' in labels() and 'Sair da tela cheia' in labels());shot('fullscreen-refresh')
        ui.run('shell','input','keyevent','4');time.sleep(.5)
        for _ in range(12):
            ui.run('shell','input','swipe',270,1100,270,670,280);time.sleep(.4)
            text=labels()
            if 'Sobre esta gravação' in text:break
        check('lyrics_inner_scroll_then_swipe_to_artist','Sobre esta gravação' in text and 'Atualizar letra' not in text);shot('artist')
        for _ in range(16):
            ui.run('shell','input','swipe',270,400,270,1050,300);time.sleep(.3)
            if 'Compartilhar música' in labels():break
        ui.click('Compartilhar música');check('share_customization_has_copy_and_card','Copiar link' in labels() and 'Compartilhar cartão + link' in labels());shot('share')
        ui.click('Copiar link');ui.click('Compartilhar cartão + link');time.sleep(2)
        text=labels();check('native_sharesheet_opened_without_sending',any('Compartilhar' in t or 'Share' in t or 'Quick Share' in t for t in text));shot('sharesheet')
        # The generated PNG, not any personal app file, is what the chooser gets.
        files=ui.run('shell','run-as',ui.PACKAGE,'ls','cache/share-cards').splitlines()
        check('share_created_only_restricted_png',any(re.fullmatch('[a-f0-9]{32}\\.png',f) for f in files))
        ui.run('shell','input','keyevent','4')
        launch(openSetlists=True);text=labels();check('setlist_wizard_available_on_android','Buscar shows' in text and any('API key' in t for t in text));shot('setlists')
        ui.click('Buscar shows');check('setlist_requires_artist_and_key','Informe artista e chave setlist.fm.' in labels())
        check('no_android_fatal_exception','FATAL EXCEPTION' not in ui.run('logcat','-d','-s','AndroidRuntime:E'))
        (OUT/'result.json').write_text(json.dumps(dict(passed=True,version='0.15.1',checks=checks,synthetic=True,remote_playlist_written=False,message_sent=False,physical_phone_tested=False),indent=2),encoding='utf-8')
    except Exception as e:
        (OUT/'result.json').write_text(json.dumps(dict(passed=False,checks=checks,error=str(e)),indent=2),encoding='utf-8');raise
    finally:ui.run('shell','am','force-stop',ui.PACKAGE)
if __name__=='__main__':main()
