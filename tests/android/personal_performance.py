"""Bounded local-audio presentation measurement; not a comparison against Spotify."""
import json,re,time
import companion_smoke as ui

ui.OUT=ui.ROOT/'out/android/personal-performance-0.7.0'
ui.OUT.mkdir(parents=True,exist_ok=True)
def cpu(pid):
    fields=ui.run('exec-out','run-as',ui.PACKAGE,'cat',f'/proc/{pid}/stat').split()
    return int(fields[13])+int(fields[14])
def memory(pid):
    raw=ui.run('shell','dumpsys','meminfo',pid)
    m=re.search(r'TOTAL PSS:\s*(\d+)',raw)
    assert m,raw
    return int(m[1])
def sample(label):
    pid=ui.run('shell','pidof',ui.PACKAGE).split()[0]
    before=cpu(pid);start=time.monotonic();time.sleep(8);elapsed=time.monotonic()-start
    return dict(label=label,seconds=elapsed,cpu_percent_one_core=(cpu(pid)-before)/100/elapsed*100,pss_kib=memory(pid))
assert ui.run('shell','getprop','ro.kernel.qemu')=='1' and 'Tonal_Local_20261015' in ui.run('emu','avd','name')
ui.run('shell','am','force-stop',ui.PACKAGE)
ui.run('shell','am','start','-W','-n',ui.PACKAGE+'/app.tonal.android.MainActivity','--ez','benchmark','true')
time.sleep(5)
samples=[sample('local_audio_player')]
ui.click('Biblioteca pessoal, estatísticas e letras')
ui.click('Agora');ui.click('Expandir letras em tela cheia',scroll=True)
time.sleep(3)
samples.append(sample('local_audio_fullscreen_effect'))
ui.screenshot('fullscreen-local-audio')
ui.run('shell','input','keyevent','4');ui.click('Fechar')
ui.run('shell','am','start','--activity-single-top','-n',ui.PACKAGE+'/app.tonal.android.MainActivity','--ez','audioSnapshot','true')
time.sleep(.5)
audio=json.loads(ui.run('exec-out','run-as',ui.PACKAGE,'cat','files/audio-snapshot.json'))
assert audio['frames']>0,audio
report=dict(samples=samples,audio=audio,emulator=True,physical_bluetooth=False,spotify_comparison=False,note='Short exploratory measurement on shared Windows host, synthetic Vorbis audio; no battery or phone performance claim.')
(ui.OUT/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
ui.run('shell','am','force-stop',ui.PACKAGE)
print(json.dumps(report,indent=2))
