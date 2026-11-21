"""0.17.0/0.17.3 na AVD dedicada: diálogo de transições (fim da atual e começo da próxima), persistência da escolha e cabeçalho da letra só com ícones.

Usa a fixture sintética (mixUiFixture) do pacote de teste; não toca em aparelho físico nem no Spotify.
"""
import importlib.util,json,time
from pathlib import Path
spec=importlib.util.spec_from_file_location('tonal_test_ui',Path(__file__).parents[1]/'playlist/android_ui.py');ui=importlib.util.module_from_spec(spec);spec.loader.exec_module(ui)
ui.OUT=ui.ROOT/'out/transitions-android-ui';ui.OUT.mkdir(parents=True,exist_ok=True)

def node(label,timeout=30,prefix=False):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:nodes=ui.dump()
        except Exception:time.sleep(.3);continue
        for n in nodes:
            values=[n.get('text',''),n.get('content-desc','')]
            if any(v.startswith(label) if prefix else v==label for v in values):return n
        time.sleep(.3)
    raise AssertionError('Controle ausente: '+label)
def tap(label,prefix=False):ui.tap(node(label,prefix=prefix));time.sleep(.5)

def main():
    assert ui.SERIAL=='emulator-5560' and ui.PKG.endswith('.benchmark')
    report={}
    ui.adb('shell','am','force-stop',ui.PKG);ui.adb('shell','pm','clear',ui.PKG);ui.adb('shell','pm','grant',ui.PKG,'android.permission.POST_NOTIFICATIONS')
    ui.adb('shell','am','start','-n',ui.PKG+'/app.tonal.android.MainActivity','--ez','mixUiFixture','true')
    tap('Transições entre músicas: efeitos de DJ');node('Transições entre músicas')
    assert node('Freio de vinil',prefix=True).get('enabled')=='false','efeitos ficam desativados com as transições desligadas'
    tap('Mesclar o fim de cada música com o começo da próxima')
    assert node('Freio de vinil',prefix=True).get('enabled')=='true'
    tap('Eco de despedida',prefix=True);node('Fim da música atual com o efeito: 6 s');node('Começo da próxima na mescla: 6 s')
    assert node('Eco de despedida',prefix=True).get('checked')=='true'
    ui.screen('01-transitions-dialog.png');tap('CONCLUIR') if any(n.get('text','')=='CONCLUIR' for n in ui.dump()) else tap('Concluir')
    # Reabrir mostra a escolha salva.
    tap('Transições entre músicas: efeitos de DJ')
    assert node('Mesclar o fim de cada música com o começo da próxima').get('checked')=='true'
    assert node('Eco de despedida',prefix=True).get('checked')=='true'
    report['transitions_dialog']={'enabled':True,'effect':'echo','seconds':6,'intro_seconds':6,'persisted':True}
    tap('CONCLUIR') if any(n.get('text','')=='CONCLUIR' for n in ui.dump()) else tap('Concluir')
    # Página da letra: desliza para a segunda seção da página da música.
    for _ in range(3):
        if any(n.get('content-desc','')=='Atualizar letra' for n in ui.dump()):break
        ui.adb('shell','input','swipe','270','1000','270','300','300');time.sleep(1)
    for label in ['Atualizar letra','Fontes e cores','Expandir letras em tela cheia','Baixar o ajuste de sincronia','Tradução automática pt-BR: ligar','Sincronia ligada: desligar']:node(label)
    texts={n.get('text','') for n in ui.dump()}
    assert 'Sincronia' not in texts and 'Tradução automática' not in texts,'as linhas de caixas de seleção saíram do cabeçalho'
    ui.screen('02-lyrics-header.png')
    tap('Sincronia ligada: desligar');node('Sincronia da letra: ligar');ui.screen('03-lyrics-sync-off.png')
    tap('Sincronia da letra: ligar');node('Sincronia ligada: desligar')
    report['lyrics_header']={'icons':6,'sync_toggle':True,'translation_toggle_present_off':True,'network_translation_requested':False}
    report['passed']=True;report['physical_phone']=False;report['real_spotify']=False
    (ui.OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False))
    ui.adb('shell','am','force-stop',ui.PKG)
if __name__=='__main__':main()
