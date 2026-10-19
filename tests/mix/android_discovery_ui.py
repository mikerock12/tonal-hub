"""Accepted discoveries survive mode changes in the real native Android dialog.
Only the dedicated synthetic test package/emulator are touched; no transport Apply.
"""
from pathlib import Path
import importlib.util,json,time
spec=importlib.util.spec_from_file_location('mix_ui',Path(__file__).with_name('android_ui.py'))
mix=importlib.util.module_from_spec(spec);spec.loader.exec_module(mix)
ui=mix.ui
ui.OUT=ui.ROOT/'out/discovery-android-ui';ui.OUT.mkdir(parents=True,exist_ok=True)

def outsiders():
    return {n.get('text','').split(' • ',1)[-1] for n in ui.dump() if 'Accepted Discovery ' in n.get('text','')}

def main():
    assert ui.SERIAL=='emulator-5560' and ui.PKG=='app.tonal.android.benchmark'
    ui.adb('shell','am','force-stop',ui.PKG)
    ui.adb('shell','pm','grant',ui.PKG,'android.permission.POST_NOTIFICATIONS')
    ui.adb('shell','am','start','-n',ui.PKG+'/app.tonal.android.MainActivity','--ez','mixUiFixture','true','--ez','mixWithDiscovery','true')
    mix.find('Agora');mix.header()
    proof=json.loads(ui.adb('exec-out','cat',f'/sdcard/Android/data/{ui.PKG}/files/discovery-retention-checks.json'))
    assert proof['passed'] and proof['checks']==24 and proof['network_requests']==0,proof
    try:mix.find('Ver fila de reprodução',True,timeout=2)
    except AssertionError:ui.adb('shell','input','swipe','270','1020','270','560','300')
    mix.click('Ver fila de reprodução');mix.find('Fila de reprodução')
    mix.find('7 • Accepted Discovery 2')
    before=outsiders();assert before=={'Accepted Discovery 1','Accepted Discovery 2'},before
    mix.click('Sortear outra ordem da fila');mix.find('Aplicar e reproduzir fila')
    assert outsiders()==before,'Shake discarded discoveries'
    mix.click('Sortear reprodução aleatória');assert outsiders()==before,'Random discarded discoveries'
    ui.screen('queue-retains-discovery.png')
    mix.click('Fechar fila');mix.click('Ver fila de reprodução');mix.find('7 • Accepted Discovery 2')
    assert outsiders()==before,'Reopened dialog discarded discoveries'
    mix.click('Ativar ou desativar Descoberta 2+1');mix.find('Aplicar e reproduzir fila')
    assert not outsiders(),'Discovery toggle failed to remove future outsiders'
    ui.screen('queue-discovery-off.png');mix.click('Fechar fila')
    report={'passed':True,'native_cpp_jni_checks':proof,'shake_keeps_discoveries':True,'random_keeps_discoveries':True,
      'reopen_keeps_discoveries':True,'only_discovery_toggle_removes_future_outsiders':True,
      'preview_did_not_apply_playback':True,'physical_phone':False,'package':ui.PKG,'serial':ui.SERIAL}
    (ui.OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False));ui.adb('shell','am','force-stop',ui.PKG)

if __name__=='__main__':main()
