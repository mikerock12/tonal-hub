"""Exercise an already authorized test-only Spotify session; never reads tokens."""
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=root/'out/android/streaming-smoke-0.2.0')
output=parser.parse_args().output
output.mkdir(parents=True,exist_ok=True)
package='app.tonal.android.benchmark'
component=package+'/app.tonal.android.MainActivity'
def run(*args):return subprocess.check_output([str(adb),*args],text=True,encoding='utf-8',errors='replace',timeout=45).strip()
def state(label):
    run('shell','uiautomator','dump','/sdcard/tonal-stream.xml')
    xml=run('shell','cat','/sdcard/tonal-stream.xml')
    (output/f'{label}.xml').write_text(xml,encoding='utf-8')
    labels=[n.get('text','') or n.get('content-desc','') for n in ET.fromstring(xml).iter('node')]
    metric=next(text for text in labels if text.startswith('PCM:'))
    return dict(frames=int(re.search(r'PCM:\s*([\d,]+)',metric).group(1).replace(',','')),
                playing='Pausar' in labels,paused='Pausado' in labels,metrics=metric,
                track=next((text for text in labels if 'Wolf' in text),None),
                errors=[text for text in labels if text.startswith(('Falha','Tempo de conexão'))])
playing=state('playing')
assert playing['playing'] and playing['frames']>0 and playing['track'] and not playing['errors'],playing
run('shell','cmd','media_session','dispatch','pause');time.sleep(1)
paused1=state('paused-1');time.sleep(2);paused2=state('paused-2')
assert paused1['paused'] and paused2['paused'] and paused1['frames']==paused2['frames'],(paused1,paused2)
run('shell','cmd','media_session','dispatch','play');time.sleep(3)
resumed=state('resumed')
assert resumed['playing'] and resumed['frames']>paused2['frames'],resumed
run('shell','input','keyevent','3');time.sleep(4)
run('shell','am','start','-W','-n',component);time.sleep(1)
background=state('after-background')
assert background['playing'] and background['frames']>resumed['frames'],background
(output/'audio-flinger.txt').write_text(run('shell','dumpsys','media.audio_flinger'),encoding='utf-8')
run('shell','screencap','-p','/sdcard/tonal-stream.png')
run('pull','/sdcard/tonal-stream.png',str(output/'tonal-streaming.png'))
run('shell','am','start','-S','-W','-n',component,'--ez','streamingBenchmark','true','--es','trackUri','spotify:track:65cOtEO1eVrTezvIMDsoDr')
reconnected=None
for attempt in range(10):
    time.sleep(2);candidate=state('reconnected')
    if candidate['playing'] and candidate['frames']>0 and candidate['track'] and not candidate['errors']:
        reconnected=candidate;break
    if candidate['errors']:raise AssertionError(candidate)
assert reconnected is not None,'Reconnection did not produce Spotify PCM'
report=dict(pass_all=True,package=package,version='0.2.0-local',track_uri='spotify:track:65cOtEO1eVrTezvIMDsoDr',
            authorization='User-authorized Premium cached engine credentials, copied only into benchmark sandbox',
            fresh_browser_oauth_completed_in_emulator=False,
            playing=playing,paused1=paused1,paused2=paused2,resumed=resumed,after_background=background,reconnected=reconnected,
            audible_quality_tested=False)
(output/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
run('shell','am','force-stop',package)
