"""Drive native views in the dedicated emulator; never touch a physical device or real Spotify playlists."""
import hashlib,json,os,re,subprocess,time,xml.etree.ElementTree as ET
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'out/text-playlist-android-ui';OUT.mkdir(parents=True,exist_ok=True)
ADB=Path(os.environ['LOCALAPPDATA'])/'Android/Sdk/platform-tools/adb.exe'
SERIAL='emulator-5560';PKG='app.tonal.android.benchmark'
def adb(*args,timeout=40,binary=False):
    r=subprocess.run([str(ADB),'-s',SERIAL,*args],capture_output=True,timeout=timeout,creationflags=subprocess.CREATE_NO_WINDOW)
    if r.returncode:raise RuntimeError(r.stderr.decode(errors='replace'))
    return r.stdout if binary else r.stdout.decode('utf-8',errors='replace')
def bounds(node):
    x,y,xx,yy=map(int,re.findall(r'\d+',node.get('bounds')))
    return (x+xx)//2,(y+yy)//2
def tap(node):adb('shell','input','tap',*map(str,bounds(node)))
def dump():
    # UIAutomator can return exit 0 without writing a file when the UI is busy.
    # A unique path prevents an old ANR hierarchy from masquerading as current UI.
    path=f'/sdcard/tonal-text-window-{time.monotonic_ns()}.xml'
    adb('shell','uiautomator','dump','--compressed',path)
    raw=adb('exec-out','cat',path)
    adb('shell','rm','-f',path)
    (OUT/'last-window.xml').write_text(raw,encoding='utf-8')
    if not raw.lstrip().startswith('<'):raise RuntimeError('UI not ready for a fresh hierarchy: '+raw[:150])
    tree=ET.fromstring(raw);nodes=list(tree.iter('node'))
    if any("isn't responding" in n.get('text','') for n in nodes):
        assert any('Pixel Launcher' in n.get('text','') for n in nodes),'The app or System UI is not responding'
        for n in nodes:
            if n.get('resource-id')=='android:id/aerr_close':tap(n);break
        time.sleep(1);return dump()
    return nodes
def wait_for(text,timeout=25,prefix=False,clickable=False):
    end=time.monotonic()+timeout
    while time.monotonic()<end:
        try:nodes=dump()
        except (RuntimeError,ET.ParseError):time.sleep(.4);continue
        for n in nodes:
            value=n.get('text','')
            if (value.startswith(text) if prefix else text in value) and n.get('enabled')=='true' and (not clickable or n.get('clickable')=='true'):return n
        time.sleep(.25)
    raise AssertionError('Missing native control/status: '+text)
def click(text,prefix=False):tap(wait_for(text,prefix=prefix,clickable=True));time.sleep(.25)
def screen(name):
    (OUT/name).write_bytes(adb('exec-out','screencap','-p',binary=True))
def edit(node,value):
    tap(node);adb('shell','input','keyevent','123')
    old=node.get('text','')
    if old:adb('shell','input','keyevent',*(['67']*len(old)))
    adb('shell','input','text',value)
def receipt():return json.loads(adb('exec-out','cat',f'/sdcard/Android/data/{PKG}/files/text-playlist-tests.json'))
def main():
    assert SERIAL.startswith('emulator-') and PKG.endswith('.benchmark')
    adb('shell','am','force-stop',PKG);adb('shell','pm','clear',PKG)
    adb('shell','pm','grant',PKG,'android.permission.POST_NOTIFICATIONS')
    adb('shell','am','start','-n',PKG+'/app.tonal.android.MainActivity','--ez','openTextPlaylist','true','--ez','textPlaylistFixture','true','--ez','textPlaylistDropBatch','true')
    wait_for('Interpretar e buscar');screen('paste.png');click('Interpretar e buscar')
    wait_for('Revisão pronta');screen('review.png');click('Personalizar',prefix=True)
    nodes=[n for n in dump() if n.get('class')=='android.widget.EditText']
    assert len(nodes)==2
    edit(nodes[0],'TonalTextNativeTest')
    nodes=[n for n in dump() if n.get('class')=='android.widget.EditText']
    edit(nodes[1],'Native_CPP_playlist_test')
    # Ui.insets keeps the footer above the IME; do not send Back into the dialog.
    click('Salvar no Tonalify');wait_for('Playlist salva no Tonalify.')
    first=receipt();assert first['native_cpp'] and first['local_tracks']==3 and first['has_cover'] and first['cloud_creates']==0 and first['real_spotify_writes']==0,first
    assert first['description']=='Native_CPP_playlist_test',first
    screen('local-saved.png');click('Salvar também no Spotify');wait_for('HTTP 503')
    interrupted=receipt();assert interrupted['cloud_creates']==1 and interrupted['cloud_items']==3 and interrupted['uncertain'],interrupted
    click('Concluir / conferir envio');wait_for('Playlist e capa salvas no Tonalify e no Spotify.')
    final=receipt();assert final['cloud_creates']==1 and final['cloud_items']==3 and final['cover_mime']=='image/jpeg' and not final['public'] and not final['uncertain'] and final['real_spotify_writes']==0,final
    screen('cloud-recovered.png')
    report={'passed':True,'serial':SERIAL,'package':PKG,'native_views':True,'shared_cpp_via_JNI':True,'real_spotify_writes':0,'before_explicit_cloud_request':first,'response_lost_after_server_applied_batch':interrupted,'verified_resume_without_duplicates':final,'screenshots':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in OUT.glob('*.png')}}
    (OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False))
    adb('shell','am','force-stop',PKG)
if __name__=='__main__':main()
