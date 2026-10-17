"""Verify the scrollable audio panel at 360x640 dp on the exclusive test AVD."""
from catalog_smoke import *

try:
    run('shell','wm','size','720x1280');run('shell','wm','density','320')
    run('shell','am','start','-S','-W','-n',PACKAGE+'/app.tonal.android.MainActivity')
    click('Áudio');wait_text('Áudio e saída Bluetooth');shot('01-audio-output')
    observed=set()
    for page in range(12):
        observed.update(texts(state()))
        run('shell','input','swipe','360','1000','360','350','350')
    shot('02-spatial-and-components')
    required=['Saída e perfil','Perfil deste dispositivo','Buffer e estabilidade',
              'Streaming Spotify','Qualidade Spotify','Normalizar volume no Spotify',
              'Ativar processamento de áudio','Preset','Ativar equalizador de 10 bandas',
              'Compressor','Normalização de arquivos locais','Normalizar por ReplayGain / EBU R128',
              'Áudio espacial','Renderização espacial','Componentes de áudio']
    missing=[name for name in required if name not in observed]
    assert not missing,missing
    assert any('Teto de amostras' in name for name in observed),observed
    assert any('Graves' in name for name in observed),observed
    click('Concluir');check('all_audio_sections_reachable_at_360dp',controls=required)
    logs=run('logcat','-d','--pid='+run('shell','pidof',PACKAGE),'-s','AndroidRuntime:E')
    assert 'FATAL EXCEPTION' not in logs,logs
    (OUT/'result.json').write_text(json.dumps(dict(passed=True,viewport_dp=[360,640],checks=checks),ensure_ascii=False,indent=2),encoding='utf-8')
except Exception as error:
    shot('failure');(OUT/'result.json').write_text(json.dumps(dict(passed=False,error=str(error)),indent=2),encoding='utf-8');raise
finally:
    run('shell','wm','size','reset');run('shell','wm','density','reset')
