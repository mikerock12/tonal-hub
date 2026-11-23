"""Regression for touch cancellation while MusicPanel replaces its SongPager."""
import json,time
import companion_smoke as ui

ui.OUT=ui.ROOT/'out/android/song-pager-race-0175'
ui.OUT.mkdir(parents=True,exist_ok=True)
report={'passed':False,'synthetic':True,'physicalPhone':False,'rendersDuringGestures':30}
try:
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1'
    assert 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    ui.run('shell','am','force-stop',ui.PACKAGE)
    ui.run('logcat','-c')
    ui.launch(songUiFixture=True,fixtureTitle='Get Back')
    ui.wait(lambda s:s['library']['currentTitle']=='Get Back' and s['library']['lyricLines']>0,60)
    for n in range(30):
        ui.run('shell','input','swipe','520','1050','520','450','120')
        ui.launch(songUiFixture=True,fixtureTitle='Get Back' if n%2 else 'Number One Blind')
    time.sleep(1)
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E')
    assert 'FATAL EXCEPTION' not in logs and 'NullPointerException' not in logs,logs
    assert ui.run('shell','pidof',ui.PACKAGE+':indie'),'player process died after pager touch cancellation'
    data=ui.snapshot()
    assert data['library']['currentTitle'] in ('Get Back','Number One Blind'),data
    report['passed']=True
    print('SONG_PAGER_RACE_PASS 30 cancelled gestures/renders, no Java or native crash',flush=True)
except Exception as error:
    report['error']=str(error)
    raise
finally:
    (ui.OUT/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
