"""0.15.3 media keys on the dedicated AVD: with Tonalify closed, PLAY from a headset/car must not wake Spotify.

No account: Tonalify plays the bundled local rehearsal, then stops. A PLAY key is dispatched
through the system media-session service (as Bluetooth/headset keys are), first with the
Tonalify process alive and then after the process is gone. The Spotify app installed on the
AVD must stay closed, Tonalify must not resume, and the key must reach MediaButtonReceiver.
"""
from pathlib import Path
import json,re,sys,time
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tests/android'));import companion_smoke as ui
OUT=ROOT/'out/android/media-button-0153';OUT.mkdir(parents=True,exist_ok=True)
P=ui.PACKAGE;SPOTIFY='com.spotify.music';RECEIVER='app.tonal.android.MediaButtonReceiver';checks=[]
def check(name,**details):checks.append(dict(name=name,passed=True,**details));print(name,details if details else '',flush=True)
def pid(package):return __import__('subprocess').run([str(ui.ADB),'-s',ui.SERIAL,'shell','pidof',package],capture_output=True,text=True,timeout=30).stdout.strip()
def sessions():return ui.run('shell','dumpsys','media_session',timeout=30).replace('\r','')
def tonalify_playing():return bool(re.search(r'\n    Tonalify '+re.escape(P)+r'/[^\n]*\n(?:      [^\n]*\n)*?      [^\n]*state=PlaybackState \{state=PLAYING',sessions()))
def ignored_keys():
    text=ui.run('shell','run-as',P,'cat','files/diagnostics/tonalify/playback.log')
    return text.count('Botão de mídia com o player fechado ignorado')
def press_play():ui.run('shell','cmd','media_session','dispatch','play');time.sleep(6)
def main():
    assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
    assert SPOTIFY in ui.run('shell','pm','list','packages',SPOTIFY),'Spotify app must be installed on the AVD for this check'
    ui.run('install','-r',str(ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk'),timeout=180);ui.run('logcat','-c')
    version=ui.run('shell','dumpsys','package',P).split('versionName=')[1].split()[0]
    assert version==re.search(r"versionName '([^']+)'",(ROOT/'android/app/build.gradle').read_text()).group(1),version
    check('benchmark_apk_installed',version=version)
    ui.run('shell','am','force-stop',SPOTIFY);ui.run('shell','am','force-stop',P)
    ui.run('shell','am','start','-W','-n',P+'/app.tonal.android.MainActivity');time.sleep(2)
    ui.click('Conta');ui.click('Testar áudio por 60 segundos',scroll=True);time.sleep(6)
    assert tonalify_playing(),sessions()[:3000]
    text=sessions()
    assert RECEIVER in text,'session must register the media button receiver'
    check('tonalify_session_registers_receiver')
    # Player stopped by the user (session released); the activity stays in the background.
    ui.run('shell','cmd','media_session','dispatch','stop');time.sleep(3);ui.run('shell','input','keyevent','KEYCODE_HOME');time.sleep(2)
    assert not tonalify_playing()
    before=ignored_keys();press_play()
    assert not pid(SPOTIFY),'Spotify started from a media key'
    assert not tonalify_playing(),'closed Tonalify resumed from a media key'
    assert ignored_keys()==before+1,'the key did not reach MediaButtonReceiver'
    check('play_with_player_stopped_wakes_nothing',spotify_running=False,tonalify_playing=False)
    # Process gone (as after the system reclaims memory): the receiver starts alone and ignores the key.
    ui.run('shell','am','kill',P);time.sleep(3)
    assert not pid(P),'Tonal process still alive after am kill'
    press_play()
    assert not pid(SPOTIFY),'Spotify started from a media key after Tonal died'
    assert not tonalify_playing(),'killed Tonalify resumed from a media key'
    assert ignored_keys()==before+2,'the key did not reach MediaButtonReceiver in a fresh process'
    check('play_after_process_death_wakes_nothing',spotify_running=False,tonalify_playing=False)
    ui.run('shell','am','force-stop',P)
    logs=ui.run('logcat','-d','-s','AndroidRuntime:E');assert 'FATAL EXCEPTION' not in logs,logs[:2000]
    check('no_fatal_exception')
    (OUT/'report.json').write_text(json.dumps(dict(version=version,checks=checks,physical_device=False,real_headset_or_car=False,spotify_account=False),indent=2,ensure_ascii=False),encoding='utf-8')
    print('MEDIA_BUTTON_ANDROID_PASS',len(checks),'checks')
if __name__=='__main__':main()
