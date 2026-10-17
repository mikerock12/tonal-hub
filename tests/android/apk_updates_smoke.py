"""0.4.0 device checks on the dedicated AVD. No Spotify account or remote playback commands."""
from pathlib import Path
import hashlib, json, os, re, shlex, subprocess, sys, time, xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
ADB=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
SERIAL=os.environ.get('ANDROID_SERIAL','emulator-5560')
PACKAGE='app.tonal.android.benchmark'
OUT=ROOT/'out/android/updates-0.4.0';OUT.mkdir(parents=True,exist_ok=True)
checks=[]

def run(*args, binary=False,timeout=50):
    if args and args[0]=='shell':args=('shell',shlex.join(map(str,args[1:])))
    data=subprocess.check_output([str(ADB),'-s',SERIAL,*map(str,args)],timeout=timeout)
    return data if binary else data.decode('utf-8',errors='replace').strip()

def check(name,**details):
    checks.append(dict(name=name,passed=True,**details));print(name,flush=True)

def launch(activity,**extras):
    args=['shell','am','start','-W','--activity-single-top','--activity-reorder-to-front','-n',PACKAGE+'/app.tonal.android.'+activity]
    for key,value in extras.items():args+=['--ez' if isinstance(value,bool) else '--es',key,str(value).lower() if isinstance(value,bool) else str(value)]
    result=run(*args);assert 'Error' not in result,result

def state():
    for attempt in range(4):
        output=run('shell','uiautomator','dump','/data/local/tmp/tonal-updates.xml')
        if 'dumped' in output:break
        time.sleep(.5)
    else:raise AssertionError('UI dump unavailable')
    root=ET.fromstring(run('exec-out','cat','/data/local/tmp/tonal-updates.xml'));parents={c:p for p in root.iter() for c in p}
    nodes=list(root.iter('node'))
    for node in nodes:
        ancestor=node;actionable=False
        for depth in range(6):
            if ancestor is None:break
            actionable|=ancestor.get('clickable')=='true' and ancestor.get('enabled')=='true'
            ancestor=parents.get(ancestor)
        node.set('_actionable',str(actionable))
    return nodes

def labels():return [n.get('text','') for n in state() if n.get('text')]

def tap(node):
    x1,y1,x2,y2=map(int,re.findall(r'\d+',node.get('bounds')))
    run('shell','input','tap',(x1+x2)//2,(y1+y2)//2)

def click(text,exact=False):
    for attempt in range(12):
        for n in state():
            if n.get('_actionable')!='True' or n.get('class','').endswith('EditText'):continue
            values=[n.get('text',''),n.get('content-desc','')]
            bounds=list(map(int,re.findall(r'\d+',n.get('bounds',''))))
            if len(bounds)!=4 or bounds[3]<=bounds[1] or bounds[2]<=bounds[0] or not 100<(bounds[1]+bounds[3])//2<2300:continue
            if any(v==text if exact else text in v for v in values):tap(n);time.sleep(.5);return
        run('shell','input','swipe','540','1750','540','950','250');time.sleep(.3)
    raise AssertionError('Missing control: '+text)

def snapshot():
    launch('IndieActivity',indieSnapshot=True);time.sleep(.3)
    return json.loads(run('exec-out','run-as',PACKAGE,'cat','files/indie-snapshot.json'))

def wait_snapshot(predicate,seconds=60):
    deadline=time.monotonic()+seconds
    while time.monotonic()<deadline:
        value=snapshot()
        if predicate(value):return value
        time.sleep(1)
    raise AssertionError('Snapshot timeout: '+json.dumps(value,ensure_ascii=False)[:1800])

def screenshot(name):
    (OUT/(name+'.png')).write_bytes(run('exec-out','screencap','-p',binary=True))

def save(passed,error=None):
    apk=ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'
    (OUT/'result.json').write_text(json.dumps(dict(version='0.4.0-local',passed=passed,checks=checks,error=error,benchmark_apk_sha256=hashlib.sha256(apk.read_bytes()).hexdigest(),physical_bluetooth_tested=False,spotify_real_stream_tested=False),ensure_ascii=False,indent=2),encoding='utf-8')

if '--finish-only' in sys.argv:
    previous=json.loads((OUT/'result.json').read_text(encoding='utf-8'))
    assert previous['checks'][-1]['name']=='switch_player_pauses_previous_audio'
    checks=previous['checks']
    assert run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in run('emu','avd','name')
    launch('IndieActivity');before={n for n in run('shell','run-as',PACKAGE,'ls','cache').splitlines() if n.startswith('stream-')}
    click('Parar motor de áudio');time.sleep(3)
    after={n for n in run('shell','run-as',PACKAGE,'ls','cache').splitlines() if n.startswith('stream-')}
    assert before-after and not after-before,(before,after)
    check('stopped_stream_cleans_its_disk_reserve',removed_directories=len(before-after))
    logs=run('logcat','-d','-s','AndroidRuntime:E');(OUT/'runtime-log.txt').write_text(logs,encoding='utf-8');assert 'FATAL EXCEPTION' not in logs
    save(True);sys.exit(0)

try:
    assert run('shell','getprop','ro.kernel.qemu')=='1'
    assert 'Tonal_Local_20261015' in run('emu','avd','name'), 'Wrong AVD; refusing mutations'
    run('logcat','-c')
    cache_before={name for name in run('shell','run-as',PACKAGE,'ls','cache').splitlines() if name.startswith('stream-')}
    launch('HubActivity');time.sleep(1);assert 'Tonal Hub' in labels();screenshot('01-hub')
    hub_pss=run('shell','dumpsys','meminfo',PACKAGE+':hub');(OUT/'hub-meminfo.txt').write_text(hub_pss,encoding='utf-8');check('hub_launcher_works')
    click('Tonalify • Spotify');time.sleep(1);assert any(t.strip()=='Tonalify' for t in labels());screenshot('02-tonalify');check('tonalify_keeps_spotify_catalog')
    # Fresh benchmark install has no OAuth; this exercises the actual JNI failure path.
    core=run('shell','am','instrument','-w','-e','action','coreTests',PACKAGE+'/app.tonal.android.DeviceInstrumentation',timeout=60)
    (OUT/'core-tests.txt').write_text(core,encoding='utf-8');assert 'INSTRUMENTATION_CODE: 0' in core and '"passed":true' in core,core;check('core_and_real_jni_recovery',output='core-tests.txt')
    launch('IndieActivity',literalQuery='Veruca Salt');result=wait_snapshot(lambda s:any(i['source']=='MusicBrainz' and i['kind']=='artist' and i['title']=='Veruca Salt' for i in s['items']))
    assert result['items'][0]['source']=='MusicBrainz' and result['items'][0]['title']=='Veruca Salt',result['items'][:4]
    (OUT/'veruca-salt.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');screenshot('03-relevant-search');check('exact_veruca_salt_artist_first')
    click('Veruca Salt',exact=True);result=wait_snapshot(lambda s:s['route'] is not None and any(i['title']=='All Hail Me' for i in s['items']))
    click('All Hail Me',exact=True);result=wait_snapshot(lambda s:s['route'] is not None and any(i['kind']=='release' for i in s['items']))
    # Open first edition in display order; recording order must come from the edition.
    first=next(i for i in result['items'] if i['kind']=='release');click(first['title'],exact=True)
    result=wait_snapshot(lambda s:s['route'] is not None and any(i['kind']=='track' for i in s['items']))
    assert all(not i['playable'] for i in result['items']);check('musicbrainz_artist_albums_editions_tracks_metadata_only',tracks=len(result['items']))
    launch('IndieActivity',literalQuery='Nirvana');result=wait_snapshot(lambda s:any(i['source']=='MusicBrainz' for i in s['items']))
    artist=next(i for i in result['items'] if i['kind']=='artist' and i['source']=='MusicBrainz');assert artist['id']=='5b11f4ce-a62d-471e-81fc-a69a8278c7da',artist;check('nirvana_relevance_uses_api_score')
    launch('IndieActivity',literalQuery='King Gizzard & The Lizard Wizard');result=wait_snapshot(lambda s:any(i['kind']=='show' for i in s['items']))
    shows=[i for i in result['items'] if i['kind']=='show'];assert any(not i['related'] for i in shows);click('Shows ao vivo');screenshot('04-live-shows');show=next(i for i in shows if not i['related']);click(show['title'],exact=True)
    result=wait_snapshot(lambda s:any(i['playable'] and i['source']=='Live Music Archive' for i in s['items']))
    playable=next(i for i in result['items'] if i['playable']);click(playable['title'],exact=True)
    playing=wait_snapshot(lambda s:s['audio']['frames']>44100*3,seconds=75);check('public_live_show_decodes_through_shared_dsp',frames=playing['audio']['frames'],buffered_bytes=playing['bufferedBytes'])
    time.sleep(5);second=snapshot();assert second['audio']['frames']>playing['audio']['frames'];assert second['bufferedBytes']<=32*1024*1024;check('stream_progress_and_bounded_disk_reserve',underruns=second['audio']['underruns']);screenshot('05-live-playback')
    click('Reproduzir / pausar');time.sleep(2);paused=snapshot();time.sleep(2);still=snapshot();assert not paused['playing'] and still['audio']['frames']==paused['audio']['frames'];check('pause_stops_pcm_without_restart')
    click('Reproduzir / pausar');resumed=wait_snapshot(lambda s:s['audio']['frames']>still['audio']['frames']);check('resume_continues_same_stream')
    # Wait until at least 1 MiB is cached before removing this AVD's network.
    wait_snapshot(lambda s:s['bufferedBytes']>1024*1024);before=snapshot();run('shell','svc','wifi','disable');run('shell','svc','data','disable')
    try:time.sleep(6);offline=snapshot();assert offline['audio']['frames']>before['audio']['frames'];check('offline_uses_downloaded_reserve',additional_frames=offline['audio']['frames']-before['audio']['frames'])
    finally:run('shell','svc','wifi','enable');run('shell','svc','data','enable')
    click('Áudio');panel=' '.join(labels());assert 'Saída e perfil' in panel and 'Streaming Spotify' not in panel;screenshot('06-indie-audio');run('shell','input','keyevent','4');check('indie_uses_own_output_profiles_and_dsp_panel')
    click('Tonalify • Spotify');time.sleep(2);launch('IndieActivity',indieSnapshot=True);time.sleep(1);assert not snapshot()['playing'];check('switch_player_pauses_previous_audio')
    click('Parar motor de áudio')
    time.sleep(3)
    cache_after={name for name in run('shell','run-as',PACKAGE,'ls','cache').splitlines() if name.startswith('stream-')}
    assert not cache_after-cache_before,cache_after-cache_before;check('stopped_stream_cleans_its_disk_reserve')
    logs=run('logcat','-d','-s','AndroidRuntime:E');(OUT/'runtime-log.txt').write_text(logs,encoding='utf-8');assert 'FATAL EXCEPTION' not in logs,logs
    save(True)
except Exception as error:
    try:screenshot('failure')
    except Exception:pass
    save(False,str(error));raise
