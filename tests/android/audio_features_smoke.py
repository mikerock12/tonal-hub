"""Run signal/decoder tests against the APK's actual FFmpeg/OpenAL/IAMF binaries."""
from pathlib import Path
import hashlib, json, subprocess, time, os

root=Path(__file__).resolve().parents[2]
adb=Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'
package='app.tonal.android.benchmark'
output=root/('out/android/audio-features-'+os.environ.get('TONAL_TEST_VERSION','0.3.0'))
output.mkdir(parents=True,exist_ok=True)
def run(*args,**kw):
    return subprocess.check_output([str(adb),*args],timeout=60,**kw)
run('shell','am','force-stop',package)
run('shell','run-as',package,'mkdir','-p','files/audio-fixtures')
for name in ['stereo-44100.wav','stereo-96000.wav','stereo.flac','stereo.mp3','stereo.m4a',
             'replaygain.flac','surround-51.wav','surround-71.wav','stereo.iamf','surround.iamf',
             'quiet.wav','loud.wav','corrupt.bin']:
    source=root/'out/audio-fixtures'/name
    remote='/data/local/tmp/tonal-audio-'+name
    run('push',str(source),remote)
    run('shell','run-as',package,'cp',remote,'files/audio-fixtures/'+name)
    actual=run('shell','run-as',package,'sha256sum','files/audio-fixtures/'+name).decode().split()[0]
    assert actual==hashlib.sha256(source.read_bytes()).hexdigest(), 'Fixture transfer corrupted '+name
    run('shell','rm',remote)
run('shell','run-as',package,'rm','-f','files/audio-suite-result.json')
run('shell','am','start','-S','-W','-n',f'{package}/app.tonal.android.MainActivity','--ez','audioSuite','true')
deadline=time.monotonic()+180
while time.monotonic()<deadline:
    try:
        raw=run('exec-out','run-as',package,'cat','files/audio-suite-result.json',stderr=subprocess.DEVNULL)
        result=json.loads(raw)
        break
    except (subprocess.CalledProcessError,json.JSONDecodeError):time.sleep(2)
else:raise AssertionError('Native audio suite timed out')
(output/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
pid=run('shell','pidof',package).decode().strip()
logs=run('logcat','-d',f'--pid={pid}','-s','TonalTest:I','AndroidRuntime:E').decode('utf-8',errors='replace')
(output/'log.txt').write_text(logs,encoding='utf-8')
print(json.dumps(result,ensure_ascii=False,indent=2))
assert result['passed'] and 'FATAL EXCEPTION' not in logs, result
