"""Text to Tonalify missing-reference filter and scroll retention, synthetic AVD only."""
from pathlib import Path
import json,re,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/playlist'))
import android_ui as ui
OUT=ROOT/'out/android/text-review-0157';OUT.mkdir(parents=True,exist_ok=True)

def visible_titles(nodes):return [n.get('text','') for n in nodes if re.match(r'^\d+\. ',n.get('text',''))]
def main():
    assert ui.adb('shell','getprop','ro.kernel.qemu').strip()=='1' and 'Tonal_Local_20261015' in ui.adb('emu','avd','name')
    package=ui.PKG;checks=[]
    ui.adb('logcat','-c')
    try:
        ui.adb('shell','am','force-stop',package)
        ui.adb('shell','am','start','-W','-n',package+'/app.tonal.android.MainActivity','--ez','openTextPlaylist','true','--ez','textPlaylistFixture','true','--ez','textPlaylistMissing','true','--ez','textPlaylistMany','true')
        ui.wait_for('Interpretar e buscar');ui.click('Interpretar e buscar');ui.wait_for('Revisão pronta',timeout=45)
        ui.click('Só não encontradas (1)');time.sleep(.4);nodes=ui.dump();text=[n.get('text','') for n in nodes]
        assert any('Shove' in value for value in text) and not any('Seether' in value for value in text),text
        assert any('Editar busca' in value for value in text)
        checks.append('filter_only_missing_reference_and_edit_available')
        ui.click('Mostrar todas as referências')
        target=None
        for _ in range(10):
            nodes=ui.dump()
            for node in nodes:
                if node.get('content-desc','').startswith('Incluir Música 20') and node.get('enabled')=='true':target=node;break
            if target is not None:break
            ui.adb('shell','input','swipe','400','1000','400','500','350');time.sleep(.35)
        assert target is not None,'scrollable review target not found'
        before=visible_titles(ui.dump());ui.tap(target);time.sleep(.45);after=visible_titles(ui.dump())
        assert before==after,(before,after)
        assert after,'review unexpectedly returned to the first rows'
        checks.append('checkbox_preserves_visible_rows_after_scrolling')
        (OUT/'review.png').write_bytes(ui.adb('exec-out','screencap','-p',binary=True))
        assert 'FATAL EXCEPTION' not in ui.adb('logcat','-d','-s','AndroidRuntime:E')
        checks.append('no_fatal_exception')
        report={'passed':True,'checks':checks,'synthetic':True,'realSpotifyWrites':0,'physicalPhone':False}
        (OUT/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))
    except Exception as error:
        (OUT/'result.json').write_text(json.dumps({'passed':False,'checks':checks,'error':str(error)},indent=2),encoding='utf-8');raise
    finally:ui.adb('shell','am','force-stop',package)
if __name__=='__main__':main()
