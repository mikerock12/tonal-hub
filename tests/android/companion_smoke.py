"""0.5 personal-library UI and public fan-recording playback, dedicated AVD only."""
from pathlib import Path
import hashlib,json,os,re,shlex,subprocess,time,sys,xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
SERIAL='emulator-5560';PACKAGE='app.tonal.android.benchmark'
OUT=ROOT/'out/android/companion-0.5.0';OUT.mkdir(parents=True,exist_ok=True)
checks=[]
def run(*args,binary=False,timeout=50):
    if args[0]=='shell':args=('shell',shlex.join(map(str,args[1:])))
    value=subprocess.check_output([str(ADB),'-s',SERIAL,*map(str,args)],timeout=timeout)
    return value if binary else value.decode('utf-8',errors='replace').strip()
def nodes():
    for _ in range(4):
        result=run('shell','uiautomator','dump','/data/local/tmp/tonal-companion.xml')
        if 'dumped' in result:return list(ET.fromstring(run('exec-out','cat','/data/local/tmp/tonal-companion.xml')).iter('node'))
        time.sleep(.5)
    raise AssertionError('UI unavailable')
def click(label,contains=False,scroll=False):
    for attempt in range(8 if scroll else 2):
        candidates=nodes()
        for node in candidates:
            text=[node.get('text',''),node.get('content-desc','')]
            if not any(label.casefold() in t.casefold() if contains else label.casefold()==t.casefold() for t in text):continue
            bounds=list(map(int,re.findall(r'\d+',node.get('bounds',''))))
            if len(bounds)==4 and bounds[2]>bounds[0] and bounds[3]>bounds[1] and 100<(bounds[1]+bounds[3])//2<2300:
                run('shell','input','tap',(bounds[0]+bounds[2])//2,(bounds[1]+bounds[3])//2);time.sleep(.6);return
        if scroll:run('shell','input','swipe','600','1800','600','1050','250')
        time.sleep(.5)
    raise AssertionError('Missing '+label+'; '+str([n.get('text') for n in candidates if n.get('text')])[:1500])
def launch(**extras):
    args=['shell','am','start','--activity-single-top','--activity-reorder-to-front','-n',PACKAGE+'/app.tonal.android.IndieActivity']
    for key,value in extras.items():args.extend(['--ez' if isinstance(value,bool) else '--es',key,str(value).lower() if isinstance(value,bool) else value])
    run(*args)
def snapshot():
    launch(indieSnapshot=True);time.sleep(.2)
    return json.loads(run('exec-out','run-as',PACKAGE,'cat','files/indie-snapshot.json'))
def wait(predicate,timeout=65):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        value=snapshot()
        if predicate(value):return value
        time.sleep(1)
    raise AssertionError('Timed out: '+json.dumps(value,ensure_ascii=False)[:1200])
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,flush=True)
def screenshot(name):(OUT/(name+'.png')).write_bytes(run('exec-out','screencap','-p',binary=True))
def set_text(value,field):
    for _ in range(6):
        candidates=[n for n in nodes() if n.get('class','').endswith('EditText') and (not field or n.get('content-desc')==field)]
        if candidates:
            n=candidates[0];b=list(map(int,re.findall(r'\d+',n.get('bounds',''))))
            if len(b)==4 and b[3]>b[1]:
                run('shell','input','tap',(b[0]+b[2])//2,(b[1]+b[3])//2)
                run('shell','input','text',value.replace(' ','%s'));run('shell','input','keyevent','4');time.sleep(.3);return
        run('shell','input','swipe','600','1650','600','1000','250')
    raise AssertionError('Text field missing: '+field)
def report(passed,error=None):
    (OUT/'result.json').write_text(json.dumps(dict(passed=passed,checks=checks,error=error,version='0.5.0-local',benchmark_apk_sha256=hashlib.sha256((ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk').read_bytes()).hexdigest(),spotify_authenticated_tested=False,physical_bluetooth_tested=False),ensure_ascii=False,indent=2),encoding='utf-8')
if '--finish-restore' in sys.argv:
    previous=json.loads((OUT/'result.json').read_text(encoding='utf-8'))
    assert previous['benchmark_apk_sha256']==hashlib.sha256((ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk').read_bytes()).hexdigest()
    checks=previous['checks'];assert checks[-1]['name']=='backup_restored_through_android_document_picker';checks.pop()
    assert run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in run('emu','avd','name')
    name='IndieTonal-'+run('shell','date','+%Y-%m-%d')+'.tonal-backup.json';click(name)
    time.sleep(2);click('Fechar');data=snapshot();assert data['library']['favorites']>=1 and data['library']['playlists']>=1 and data['library']['lyricLines']==0
    check('backup_restored_through_android_document_picker');(OUT/'state.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    before=data['positionMs'];time.sleep(2);after=snapshot();assert after['playing'] and after['positionMs']!=before;check('audio_keeps_progressing_while_using_personal_features',position_ms=after['positionMs'])
    click('Parar motor de áudio');logs=run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs
    report(True);sys.exit(0)
try:
    assert run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in run('emu','avd','name')
    run('logcat','-c');launch(literalQuery='L7');time.sleep(1)
    click('Fonte de catálogo');click('Aadam Jacobs')
    data=wait(lambda x:any(i['id']=='ajc02607_l7-1989-09-14' for i in x['items']))
    check('l7_fan_recording_discovered_from_audio_collection',recording='ajc02607_l7-1989-09-14');screenshot('01-rare-search')
    click('L7 Live at Dreamerz 1989-09-14',scroll=True)
    data=wait(lambda x:any(i['playable'] and i['kind']=='track' for i in x['items']))
    assert all(not i['id'].endswith('.ffp') for i in data['items'])
    first=next(i for i in data['items'] if i['playable'])
    click('Reproduzir '+first['title'],scroll=True)
    data=wait(lambda x:x['playing'] and x['positionMs']>1800)
    check('l7_actual_pcm_playback_no_checksum_files',position_ms=data['positionMs']);screenshot('02-playing')
    click('Biblioteca pessoal, estatísticas e letras')
    if any(n.get('text')=='♡ Favoritar' for n in nodes()):click('♡ Favoritar')
    click('+ Playlist');click('Criar playlist pessoal…');set_text('Show L7 teste','');click('SALVAR',contains=True)
    screenshot('03-personal-library');check('favorites_and_personal_playlist_ui')
    click('Estatísticas');screenshot('04-statistics');check('interactive_statistics_tab_opens')
    click('Letras e cenário');click('Editar dados / importar letra ou tradução')
    set_text('[00:00.00][00:01.00]Hello world','Letra TXT/LRC ou tradução com as mesmas linhas')
    click('IMPORTAR LETRA',contains=True);screenshot('05-lyric-stage')
    check('synced_lyrics_import_foreground_stage')
    click('Traduzir pt-BR');click('Inglês')
    # Metadata snapshot is collected after closing so debug intents never cover the dialog.
    end=time.monotonic()+40;translated=False
    while time.monotonic()<end:
        labels=' '.join(n.get('text','') for n in nodes())
        if 'tradução automática pt-BR' in labels:
            assert 'Olá mundo' in labels or 'Olá Mundo' in labels,labels
            translated=True;break
        if 'parcial/indisponível' in labels or 'limite local diário' in labels:break
        time.sleep(1)
    screenshot('06-translation');check('translation_service_ui_handles_result',translation_succeeded=translated)
    click('Aurora');click('Ondas');time.sleep(1);screenshot('07-waveform')
    click('Ondas');click('Espectro');time.sleep(1);screenshot('08-spectrum');check('actual_pcm_visualizers_open_during_playback')
    click('Backup');assert any('contas e senhas ficam de fora' in n.get('text','') for n in nodes());click('Exportar');click('Save');time.sleep(2)
    backup_name='IndieTonal-'+run('shell','date','+%Y-%m-%d')+'.tonal-backup.json'
    exported=run('shell','find','/sdcard/Download','/sdcard/Documents','-maxdepth','1','-name',backup_name)
    assert backup_name in exported,exported
    backup_path=next(p for p in exported.splitlines() if p.endswith(backup_name))
    backup=json.loads(run('exec-out','cat',backup_path));assert backup['format']=='tonal-personal-backup' and backup['module']=='indie'
    check('backup_saved_in_external_documents_storage',filename=backup_name)
    click('Backup');click('Restaurar');click('Escolher backup');click(backup_name,scroll=True);time.sleep(2);check('backup_restored_through_android_document_picker')
    click('Fechar');data=snapshot();assert data['library']['favorites']>=1 and data['library']['playlists']>=1 and data['library']['lyricLines']==0
    (OUT/'state.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    before=data['positionMs'];time.sleep(2);after=snapshot();assert after['positionMs']>before;check('audio_keeps_progressing_while_using_personal_features',position_ms=after['positionMs'])
    click('Parar motor de áudio');logs=run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs
    report(True)
except Exception as e:
    report(False,str(e));raise
