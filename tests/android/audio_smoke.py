"""Functional test of real AudioTrack playback and MediaSession transport, on the local emulator."""
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET
import argparse

root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=root/'out/android/audio-smoke')
output=parser.parse_args().output
output.mkdir(parents=True,exist_ok=True)
def run(*args):
    return subprocess.check_output([str(adb),*args],text=True,encoding='utf-8',errors='replace',timeout=40).strip()
def state(label):
    run('shell','uiautomator','dump','/sdcard/tonal-smoke.xml')
    xml=run('shell','cat','/sdcard/tonal-smoke.xml')
    (output/f'{label}.xml').write_text(xml,encoding='utf-8')
    labels=[n.get('text','') or n.get('content-desc','') for n in ET.fromstring(xml).iter('node')]
    metric=next(s for s in labels if s.startswith('PCM:'))
    frames=int(re.search(r'PCM:\s*([\d,]+)',metric).group(1).replace(',',''))
    return dict(frames=frames,paused='Pausado' in labels,playing='Pausar' in labels,metrics=metric)
run('shell','am','force-stop','com.spotify.music')
run('shell','am','force-stop','app.tonal.android.benchmark')
run('shell','am','start','-S','-W','-n','app.tonal.android/.MainActivity','--ez','benchmark','true')
time.sleep(6)
playing=state('playing')
assert playing['playing'] and playing['frames']>0, playing
run('shell','cmd','media_session','dispatch','pause')
time.sleep(1)
paused1=state('paused-1')
time.sleep(2)
paused2=state('paused-2')
assert paused1['paused'] and paused2['paused'] and paused1['frames']==paused2['frames'],(paused1,paused2)
run('shell','cmd','media_session','dispatch','play')
time.sleep(2)
resumed=state('resumed')
assert resumed['playing'] and resumed['frames']>paused2['frames'], resumed
run('shell','input','keyevent','3')
time.sleep(2)
audio=run('shell','dumpsys','media.audio_flinger')
(output/'background-audio.txt').write_text(audio,encoding='utf-8')
sessions=run('shell','dumpsys','media_session')
(output/'background-media-session.txt').write_text(sessions,encoding='utf-8')
run('shell','am','start','-W','-n','app.tonal.android/.MainActivity')
time.sleep(1)
foreground=state('after-background')
assert foreground['playing'] and foreground['frames']>resumed['frames'],foreground
run('shell','screencap','-p','/sdcard/tonal-smoke.png')
run('pull','/sdcard/tonal-smoke.png',str(output/'tonal-playing.png'))
pid=run('shell','pidof','app.tonal.android')
logs=run('logcat','-d',f'--pid={pid}','-s','TonalTest:I','AndroidRuntime:E')
(output/'test-log.txt').write_text(logs,encoding='utf-8')
assert 'DSP_SELF_TEST PASS' in logs and 'FATAL EXCEPTION' not in logs, 'DSP test/crash check failed'
report=dict(pass_all=True,playing=playing,paused1=paused1,paused2=paused2,resumed=resumed,after_background=foreground,
            audible_click_tested=False,note='Headless emulator; verified PCM transport and state, not audible quality')
(output/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
run('shell','am','force-stop','app.tonal.android')
